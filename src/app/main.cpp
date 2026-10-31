// Entry point. Start the app, then run it: the object graph lives in `app/BoardWiring.cpp`, the
// boot sequence in `app/AppBootstrap.cpp`, and the loop in `app/MenuHost.cpp`. This file is
// deliberately nothing but the two Arduino hooks.
//
// `<Arduino.h>` is not optional: it declares `setup`/`loop` with C linkage, so without it the
// definitions below would be mangled as C++ and the framework's `main()` would never call them.

#include "hal/WarningGuard.hpp"

PUSH_NO_WARNINGS
#include <Arduino.h>
POP_NO_WARNINGS

#include "AppBootstrap.hpp"
#include "MenuHost.hpp"

void setup()
{
    app::bootstrap();
}

void loop()
{
    app::menuLoop();
}
