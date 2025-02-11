Import("env")

import os
import tempfile
from pathlib import Path


def cprint(*args, **kwargs):
    print(f'pre_script_patch_debug.py:', *args, **kwargs)


def get_winterrupts_path():
    winterrupts_path = None
    for pio_package in env.PioPlatform().dump_used_packages():
        pio_dir = env.PioPlatform().get_package_dir(pio_package['name'])
        # TODO: This should change for non-mega cores!
        possible_path = os.path.join(pio_dir, 'cores', 'MegaCore', 'WInterrupts.c')
        if os.path.exists(possible_path):
            cprint(f'Found WInterrupts.c: {possible_path}')
            winterrupts_path = possible_path
    return winterrupts_path


def patch_function_factory(src_path, output_suffix, replacement_list):
    """
    Creates a function that will return a filepath to a patched source file
    :param src_path: The actual source path on disk, this is different than node.get_abspath()
    :param output_suffix: The suffix for the output temporary file
    :param replacement_list: List of 'in'/'out' pairs that should be replaced
    :return: Build Middleware function
    """
    def out_func(node):
        # patch_path_key needs to be kept in sync with post_script_remove_patched_files.py
        # so that after a successful build the patched file can be removed
        project_dir_name = Path.cwd().name  # See post_script_remove_patched_files.py on why this is needed
        patch_path_key = f'_{project_dir_name}_patched_'
        with tempfile.NamedTemporaryFile(mode='w', suffix=f'{patch_path_key}{output_suffix}', delete=False) as tf:
            patched_filepath = tf.name
            cprint(f'Patching {src_path}')
            cprint(f'Replacement path: {patched_filepath}')
            cprint(f'Build path: {node.get_abspath()}')
            with open(src_path, 'r') as wint_f:
                for wint_line in wint_f.readlines():
                    # Default is to just path the line through un-replaced
                    out_line = wint_line
                    # Now we check if line is in the replacements list
                    for replacement in replacement_list:
                        if replacement['in'] in wint_line:
                            out_line = replacement['out']
                            break
                    # Write the (possibly replaced) line to the output temporary file
                    tf.write(out_line)
        return env.File(patched_filepath)
    return out_func


source_patch_dict = {
    '*WInterrupts.c': {
        'actual_src_path': get_winterrupts_path(),
        'patches': [
            {
                'in': 'IMPLEMENT_ISR(INT7_vect, EXTERNAL_INT_7)',
                'out': '''\
#if defined(OAT_DEBUG_BUILD)
  #pragma message "OAT_DEBUG_BUILD is defined, ISR 7 disabled in WInterrupts.c"
#else
  IMPLEMENT_ISR(INT7_vect, EXTERNAL_INT_7)
#endif
'''
            },
        ]
    }
}

for filepath_glob, file_patch_info in source_patch_dict.items():
    file_src_path = file_patch_info['actual_src_path']
    if not file_src_path:
        cprint(f'Could not find {filepath_glob} to patch! Skipping...')
        continue

    env.AddBuildMiddleware(
        patch_function_factory(src_path=file_src_path,
                               replacement_list=file_patch_info['patches'],
                               output_suffix='WInterrupts.c'),
        filepath_glob
    )
