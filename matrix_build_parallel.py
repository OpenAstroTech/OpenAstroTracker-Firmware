"""
Module where all functionality that purely relates to how we parallelize matrix_build.py
should live. It's not a perfect split of course, but it helps to separate the 'matrix'
logic from the 'how we build' logic.
"""
import os
import shutil
import subprocess
import tempfile
import time
from pathlib import Path
from typing import Optional, List
from dataclasses import dataclass


@dataclass
class Executor:
    """
    Core data that defines a solution that is being built
    """
    # The directory where we are building the solution
    proj_dir: Path
    # The solution dictionary
    solution: Optional[dict] = None
    # The process building the solution
    proc: Optional[subprocess.Popen] = None
    # Object that holds tempdir data, so that it can be cleaned up later
    tempdir_obj: Optional[tempfile.TemporaryDirectory] = None


def generate_config_file(project_location: Path, flag_values: dict):
    content = "#pragma once\n\n"
    for key, value in flag_values.items():
        content += "#define {} {}\n".format(key, value)

    with open(Path(project_location, "Configuration_local_matrix.hpp"), 'w') as f:
        f.write(content)
        f.flush()


def execute(project_location: Path, board: str, flag_values: dict, jobs: int = 1, out_pipe=True) -> subprocess.Popen:
    """
    Start up an executor that is building a solution
    :param project_location: The directory where to build the solution
    :param board: The board type (aka environment)
    :param flag_values: Dictionary of #defines to create a config file from
    :param jobs: How many jobs the build process should use
    :param out_pipe: If the executor's stdout/stderr should be pipes
    :return: Process object that is executing the solution
    """
    build_env = dict(os.environ)
    build_env["PLATFORMIO_BUILD_FLAGS"] = "-DMATRIX_LOCAL_CONFIG=1"
    generate_config_file(project_location, flag_values)

    proc = subprocess.Popen(
        ['pio',
         'run',
         f'--project-dir={str(project_location.resolve())}',
         f'--environment={board}',
         f'--jobs={jobs}',
         ],
        stdout=subprocess.PIPE if out_pipe else None,
        stderr=subprocess.PIPE if out_pipe else None,
        env=build_env,
        close_fds=True,
    )
    return proc


def get_available_executor_idx(e_list: List[Executor]) -> Optional[int]:
    """
    Get the index of an idle executor
    :param e_list: List of executors
    :return: Idle executor index, else None if all are busy
    """
    for i, executor in enumerate(e_list):
        if executor.proc is None:
            return i
    return None


def get_finished_executor_idx(e_list: List[Executor]) -> Optional[int]:
    """
    Get the index of a finished executor
    :param e_list: List of executors
    :return: Finished executor index, else None if all are busy
    """
    for i, executor in enumerate(e_list):
        if executor.proc is not None and executor.proc.poll() is not None:
            return i
    return None


def cleanup_tempdirs(e_list: List[Executor]):
    """
    Delete all the temporary directories that executors were using
    :param e_list: List of executors
    """
    for executor in e_list:
        if executor.tempdir_obj is not None:
            tempdir_path = executor.tempdir_obj.name
            print(f'Deleting {tempdir_path}')
            shutil.rmtree(tempdir_path, ignore_errors=True)


def create_executors(num_executors: int, local_paths_to_link: List[Path]) -> List[Executor]:
    """
    Create a number of executors and their associated temporary directories, then
    soft-link all needed project files
    :param num_executors: Number of executors to create
    :param local_paths_to_link: List of files to soft-link into the executor projects
    :return: List of executors
    """
    executor_list: List[Executor] = []
    print(f'Creating {num_executors} executors')
    for executor_idx in range(num_executors):
        tempdir = tempfile.TemporaryDirectory()
        temp_proj_path = Path(tempdir.name)
        for local_path in local_paths_to_link:
            temp_dst_path = Path(temp_proj_path, local_path).resolve()
            os.makedirs(temp_dst_path.parent, exist_ok=True)
            os.symlink(local_path.resolve(), temp_dst_path)
        executor_list.append(Executor(temp_proj_path, tempdir_obj=tempdir))
        print(f'{executor_idx} ', end='')
    print()
    return executor_list


def copy_caches_to_executors(src_proj_dir: Path, dst_executors: List[Executor]):
    """
    Copy cache directories from a source directory to a number of executor project directories
    :param src_proj_dir: Directory to copy from
    :param dst_executors: List of executors to copy to
    """
    print('Copying caches to other executors')
    dir_names_to_copy = ['.pio', 'build_cache']
    for dir_name_to_copy in dir_names_to_copy:
        src_path = Path(src_proj_dir, dir_name_to_copy)
        for dst_executor in dst_executors:
            dst_path = Path(dst_executor.proj_dir, dir_name_to_copy)
            shutil.copytree(src_path, dst_path)


def get_source_files_to_link() -> List[Path]:
    """
    Create a list of the important files from the local project. I didn't want to
    use git here, since that might not pick up untracked (but needed) files.
    :return: List of source files that a project needs in order to compile
    """
    local_proj_path = Path('.')
    venv_dirs = list(local_proj_path.glob('*venv*/'))
    # Don't link the .pio directory because the builds need to be independent
    pio_dirs = list(local_proj_path.glob('*.pio*/'))
    cmake_dirs = list(local_proj_path.glob('*cmake-build*/'))

    local_dirs_to_not_link = [Path('.git/'), Path('build_cache/')] + venv_dirs + pio_dirs + cmake_dirs
    local_filenames_to_not_link = [
        Path('Configuration_local.hpp'),
        Path('Configuration_local_matrix.hpp'),
    ]

    local_paths_to_link = []
    for local_dir_str, local_subdirs, local_files in os.walk(local_proj_path):
        local_dir_path = Path(local_dir_str)
        dir_shouldnt_be_linked = any(d == local_dir_path or d in local_dir_path.parents for d in local_dirs_to_not_link)
        if dir_shouldnt_be_linked:
            continue
        for local_file in local_files:
            local_file_full_path = Path(local_dir_path, local_file)
            file_shouldnt_be_linked = any(local_file_full_path == f for f in local_filenames_to_not_link)
            if file_shouldnt_be_linked:
                continue
            local_paths_to_link.append(local_file_full_path)
    return local_paths_to_link


def wait_for_executor_to_finish(executor_list: List[Executor], timeout=0.1, poll_time=0.2):
    """
    Block until an executor has finished building
    :param executor_list: List of executors
    :param timeout: Time to communicate() with the running process (kind of a hack)
    :param poll_time: Time to wait before checking all executors again
    """
    while get_finished_executor_idx(executor_list) is None:
        for e in executor_list:
            if e.proc is not None and e.proc.poll() is None:
                # Communicate with the running processes to stop them from blocking
                # (i.e. they spew too much output)
                try:
                    _ = e.proc.communicate(timeout=timeout)
                except subprocess.TimeoutExpired:
                    pass  # This is expected and what should happen
        time.sleep(poll_time)
