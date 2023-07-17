#include "../Configuration.hpp"
#include "Utility.hpp"
#include "InterruptCallback.hpp"

//////////////////////////////////////
// This is an hardware-independent abstraction layer over
// whatever timer is used for the hardware being run
//////////////////////////////////////

#ifndef NEW_STEPPER_LIB

    #if defined ESP32
    // We don't support ESP32 boards in interrupt mode
    #elif defined __AVR_ATmega2560__  // Arduino Mega
        #define USE_TIMER_1 true
        #define USE_TIMER_2 true
        #define USE_TIMER_3 false
        #define USE_TIMER_4 false
        #define USE_TIMER_5 false
PUSH_NO_WARNINGS
        #include "libs/TimerInterrupt/TimerInterrupt.h"
POP_NO_WARNINGS
    #else
        #error Unrecognized board selected. Either implement interrupt code or define the board here.
    #endif

    #if defined(ESP32)

    #elif defined __AVR_ATmega2560__

bool InterruptCallback::setInterval(float intervalMs, interrupt_callback_p callback, void *payload)
{
    // We have requested to use Timer2 (see above)
    ITimer2.init();

    // This timer supports the callback with payload
    return ITimer2.attachInterruptInterval<void *>(intervalMs, callback, payload, 0UL);
}

void InterruptCallback::stop()
{
    ITimer2.stopTimer();
}

void InterruptCallback::start()
{
    ITimer2.restartTimer();
}

    #endif
#endif