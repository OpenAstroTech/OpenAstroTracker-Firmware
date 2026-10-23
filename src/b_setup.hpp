// Compatibility shim. The composition root now lives in `app/BoardWiring.*` and the entry
// point in `app/main.cpp`; this header remains only while the menu screens still include it
// for the `mount` global. It goes away with them (PLAN.md Phase 7, step 4).

#pragma once

#include "app/BoardWiring.hpp"

// The debugger hooks, the test menu and the non-interrupt stepper timer are still reached
// through this header by the menu screens.
#if defined(OAT_DEBUG_BUILD)
    #if BOARD < 1000
        #include "avr8-stub.h"
    #else
        #error "Debugging not supported on this platform"
    #endif
#endif

#if TEST_VERIFY_MODE == 1
    #include "testmenu.hpp"
#endif

#ifndef NEW_STEPPER_LIB
    #include "hal/InterruptCallback.hpp"
#endif
