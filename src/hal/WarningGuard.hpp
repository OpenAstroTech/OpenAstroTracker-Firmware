#pragma once

/// Fallbacks for the warning pragmas that PlatformIO force-includes from
/// `macros/gcc/Macros.hpp`. Arduino IDE builds have no such flag, and the backends
/// in this directory are the first to include third-party headers that are not
/// warning-clean under our flag set (`-Wshadow`, `-Wuseless-cast`, ...).
#if !defined(PUSH_NO_WARNINGS)
    #define PUSH_NO_WARNINGS ;
#endif
#if !defined(POP_NO_WARNINGS)
    #define POP_NO_WARNINGS ;
#endif
