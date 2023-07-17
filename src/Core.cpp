#include "inc/Globals.hpp"
#include "../Configuration.hpp"
#include "Utility.hpp"
#include "Mount.hpp"

#include "a_inits.hpp"
#include "b_setup.hpp"
#include "c65_startup.hpp"
#include "c70_menuRA.hpp"
#include "c71_menuDEC.hpp"
#include "c722_menuPOI.hpp"
#include "c725_menuHOME.hpp"
#include "c72_menuHA.hpp"
#include "c72_menuHA_GPS.hpp"
#include "c75_menuCTRL.hpp"
#include "c76_menuCAL.hpp"
#include "c78_menuINFO.hpp"
#include "c_buttons.hpp"
#include "f_serial.hpp"

#ifdef NEW_STEPPER_LIB
    #ifdef ARDUINO_ARCH_AVR
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