// The AVR timer interrupt vectors the interval-stepper engines own.
//
// `IntervalInterrupt_AVR` drives the step generator from TIMER1/3/4/5; these are the only
// vectors the application defines, and they exist so the vendored engine's static handlers are
// reached. They used to live at the bottom of `Core.cpp`, which is the translation unit that
// pulls in the whole app — i.e. the ISR table was defined in a file with 14 unrelated includes,
// and nothing about it was AVR-specific except by accident of which platform compiled it.
//
// Nothing else in the firmware touches timer registers directly; the RP2040 path goes through
// `hal/rp2040/Rp2040TimerService` and the ESP32 path through the FreeRTOS stepper task.
#include <stdint.h>

#ifdef NEW_STEPPER_LIB
    #ifdef ARDUINO_ARCH_AVR
        #include <Arduino.h>

        #include "IntervalInterrupt.h"

ISR(TIMER1_OVF_vect)
{
    IntervalInterrupt_AVR<Timer::TIMER_1>::handle_overflow();
}
ISR(TIMER1_COMPA_vect)
{
    IntervalInterrupt_AVR<Timer::TIMER_1>::handle_compare_match();
}
ISR(TIMER3_OVF_vect)
{
    IntervalInterrupt_AVR<Timer::TIMER_3>::handle_overflow();
}
ISR(TIMER3_COMPA_vect)
{
    IntervalInterrupt_AVR<Timer::TIMER_3>::handle_compare_match();
}
ISR(TIMER4_OVF_vect)
{
    IntervalInterrupt_AVR<Timer::TIMER_4>::handle_overflow();
}
ISR(TIMER4_COMPA_vect)
{
    IntervalInterrupt_AVR<Timer::TIMER_4>::handle_compare_match();
}
ISR(TIMER5_OVF_vect)
{
    IntervalInterrupt_AVR<Timer::TIMER_5>::handle_overflow();
}
ISR(TIMER5_COMPA_vect)
{
    IntervalInterrupt_AVR<Timer::TIMER_5>::handle_compare_match();
}

    #endif
#endif
