Import("env", "projenv")

import os
import tempfile
from pathlib import Path


def cprint(*args, **kwargs):
    print(f'post_script_remove_patched_files.py:', *args, **kwargs)


def clean_up_patched_files(*_, **__):
    """
    Removes all temporary patched files created previously in the build process
    """
    # patch_path_key needs to be kept in sync with pre_script_patch_debug.py
    # We put the current directory name in the key so that we only remove
    # patched files that we know were built by the current build process.
    # This is only useful in safeguarding against multiple builds being done in
    # different directories at the same time. (i.e. we don't want to remove another
    # processes' files while they are still in use)
    project_dir_name = Path.cwd().name
    patch_path_key = f'_{project_dir_name}_patched_'
    tempdir_path = tempfile.gettempdir()
    cprint(f'Temp file dir is {tempdir_path}')
    patched_filepaths = []
    for filename in os.listdir(tempdir_path):
        full_filepath = os.path.join(tempdir_path, filename)
        if os.path.isfile(full_filepath) and patch_path_key in filename:
            patched_filepaths.append(full_filepath)
    for patched_filepath in patched_filepaths:
        cprint(f'Removing {patched_filepath}')
        try:
            os.remove(patched_filepath)
            pass
        except FileNotFoundError:
            cprint('Not found (deleted already?)')


env.AddPostAction('buildprog', clean_up_patched_files)
