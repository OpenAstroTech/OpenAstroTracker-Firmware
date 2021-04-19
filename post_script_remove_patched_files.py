Import("env", "projenv")

import os
import tempfile


def cprint(*args, **kwargs):
    print(f'post_script_remove_patched_files.py:', *args, **kwargs)


def clean_up_patched_files(*_, **__):
    """
    Removes all temporary patched files created previously in the build process
    """
    # patch_path_key needs to be kept in sync with pre_script_patch_debug.py
    patch_path_key = '_patched_'
    tempdir_path = tempfile.gettempdir()
    cprint(f'Temp file dir is {tempdir_path}')
    patched_filepaths = []
    for filename in os.listdir(tempdir_path):
        full_filepath = os.path.join(tempdir_path, filename)
        if os.path.isfile(full_filepath) and patch_path_key in filename:
            patched_filepaths.append(full_filepath)
    for patched_filepath in patched_filepaths:
        cprint(f'Removing {patched_filepath}')
        os.remove(patched_filepath)


env.AddPostAction('buildprog', clean_up_patched_files)
