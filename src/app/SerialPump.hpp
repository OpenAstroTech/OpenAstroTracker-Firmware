#pragma once

/// The serial-control path and the Arduino serial hooks.
///
/// Declarations only: this was a header that defined `serialLoop()`, `serialEvent()` and
/// `processSerialData()`, so every translation unit that included it emitted its own copies. With
/// `MenuHost.cpp` and `main.cpp` now both including it — the loop moved out of the old
/// define-everything `MenuHost.hpp` — that became a duplicate-symbol link error, which is the
/// failure mode R3 exists to remove.

#if TEST_VERIFY_MODE == 1
    #include "app/test/TestMenu.hpp"
    #include "app/test/TestMenuDefinitions.hpp"
#endif

#include "types/MathUtil.hpp"
#include "app/BoardWiring.hpp"

#if SUPPORT_SERIAL_CONTROL == 1
    #include "adapters/MeadeCommandAdapter.hpp"
    #include "adapters/SerialTransport.hpp"
    #include "hal/arduino/ArduinoSerialPort.hpp"

/// Drains whatever the transport has buffered into the Meade adapter. ESP32 calls it in a loop.
void processSerialData();

/// The main loop when under serial control.
void serialLoop();

    #ifndef ESP32
/// Called by the Arduino core when the port has data.
void serialEvent();
    #endif
#endif
