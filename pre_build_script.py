Import("env")

import platform

extra_macros = {
    # PP indirection needed
    'DO_PRAGMA_(x)': '_Pragma(#x)',
    'DO_PRAGMA(x)': 'DO_PRAGMA_(x)',

    # Useful for disabling warnings on system headers that we don't care about
    'PUSH_NO_WARNINGS': ''.join([
        'DO_PRAGMA(GCC diagnostic push);',
        'DO_PRAGMA(GCC diagnostic ignored "-Wconversion");',
        'DO_PRAGMA(GCC diagnostic ignored "-Wdouble-promotion");',
        'DO_PRAGMA(GCC diagnostic ignored "-Wshadow");',
        'DO_PRAGMA(GCC diagnostic ignored "-Wsign-conversion");',
        'DO_PRAGMA(GCC diagnostic ignored "-Wsign-compare");',
        'DO_PRAGMA(GCC diagnostic ignored "-Wignored-qualifiers");',
        'DO_PRAGMA(GCC diagnostic ignored "-Wuseless-cast");',
        'DO_PRAGMA(GCC diagnostic ignored "-Wall");',
        'DO_PRAGMA(GCC diagnostic ignored "-Wextra");',
        'DO_PRAGMA(GCC diagnostic ignored "-Wpedantic");',
    ]),
    'POP_NO_WARNINGS': 'DO_PRAGMA(GCC diagnostic pop);'
}

def escape_str(s):
    if 'windows' not in platform.system().lower():
        # This stuff breaks windows builds
        if ' ' not in s:
            s = s.replace('(', '\\(')
            s = s.replace(')', '\\)')
    s = s.replace('"', '\\"')
    return s

env.Append(CPPDEFINES={escape_str(k): escape_str(v) for k, v in extra_macros.items()})
