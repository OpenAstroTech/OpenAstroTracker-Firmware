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
    #elif defined(ARDUINO_ARCH_RP2040)
        // Earle Philhower arduino-pico core exposes the Pico SDK alarm API.
        // add_repeating_timer_us() fires a hardware alarm callback at a precise
        // interval without relying on FreeRTOS or a second core.
        #include <hardware/timer.h>
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

    #elif defined(ARDUINO_ARCH_RP2040)

// Storage for the repeating timer handle and the user callback+payload.
static repeating_timer_t _rp2040_timer;
static interrupt_callback_p _rp2040_callback = nullptr;
static void *_rp2040_payload                 = nullptr;

// The Pico SDK alarm callback signature returns bool; returning true keeps
// the timer repeating.
static bool rp2040_timer_isr(repeating_timer_t * /* rt */)
{
    if (_rp2040_callback)
    {
        _rp2040_callback(_rp2040_payload);
    }
    return true;  // Keep repeating
}

bool InterruptCallback::setInterval(float intervalMs, interrupt_callback_p callback, void *payload)
{
    _rp2040_callback = callback;
    _rp2040_payload  = payload;

    // Positive delay_us means the period is measured from callback-start to
    // callback-start (fixed wall-clock rate), which is what the stepper
    // controller needs for consistent timing.
    int32_t intervalUs = static_cast<int32_t>(intervalMs * 1000.0f);
    return add_repeating_timer_us(intervalUs, rp2040_timer_isr, nullptr, &_rp2040_timer);
}

void InterruptCallback::stop()
{
    cancel_repeating_timer(&_rp2040_timer);
}

void InterruptCallback::start()
{
    // Re-arm with the same interval. The stored callback/payload are still set.
    int32_t intervalUs = static_cast<int32_t>(_rp2040_timer.delay_us);
    add_repeating_timer_us(intervalUs, rp2040_timer_isr, nullptr, &_rp2040_timer);
}

    #endif
#endif
