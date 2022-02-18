#include "IntervalInterrupt_AVR.h"

constexpr volatile uint8_t *TCCRA(Timer timer)
{
    switch (timer)
    {
        case Timer::TIMER_1:
            return &TCCR1A;
        case Timer::TIMER_3:
            return &TCCR3A;
        case Timer::TIMER_4:
            return &TCCR4A;
        case Timer::TIMER_5:
            return &TCCR5A;
        default:
            return 0;
    }
}

constexpr volatile uint8_t *TCCRB(Timer timer)
{
    switch (timer)
    {
        case Timer::TIMER_1:
            return &TCCR1B;
        case Timer::TIMER_3:
            return &TCCR3B;
        case Timer::TIMER_4:
            return &TCCR4B;
        case Timer::TIMER_5:
            return &TCCR5B;
        default:
            return 0;
    }
}

constexpr volatile uint8_t *TIMSK(Timer timer)
{
    switch (timer)
    {
        case Timer::TIMER_1:
            return &TIMSK1;
        case Timer::TIMER_3:
            return &TIMSK3;
        case Timer::TIMER_4:
            return &TIMSK4;
        case Timer::TIMER_5:
            return &TIMSK5;
        default:
            return 0;
    }
}

constexpr volatile uint8_t *TIFR(Timer timer)
{
    switch (timer)
    {
        case Timer::TIMER_1:
            return &TIFR1;
        case Timer::TIMER_3:
            return &TIFR3;
        case Timer::TIMER_4:
            return &TIFR4;
        case Timer::TIMER_5:
            return &TIFR5;
        default:
            return 0;
    }
}

constexpr volatile uint16_t *OCRA(Timer timer)
{
    switch (timer)
    {
        case Timer::TIMER_1:
            return &OCR1A;
        case Timer::TIMER_3:
            return &OCR3A;
        case Timer::TIMER_4:
            return &OCR4A;
        case Timer::TIMER_5:
            return &OCR5A;
        default:
            return 0;
    }
}

constexpr volatile uint16_t *TCNT(Timer timer)
{
    switch (timer)
    {
        case Timer::TIMER_1:
            return &TCNT1;
        case Timer::TIMER_3:
            return &TCNT3;
        case Timer::TIMER_4:
            return &TCNT4;
        case Timer::TIMER_5:
            return &TCNT5;
        default:
            return 0;
    }
}