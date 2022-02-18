#pragma once

#include <IntervalInterrupt.h>
#include <stdint.h>
#include "HardwareTimer.h"

#define DEBUG_INTERRUPT_TIMING_PIN 0
#if DEBUG_INTERRUPT_TIMING_PIN != 0 && UNIT_TEST != 1
#include <Pin.h>
#define INTERRUPT_TIMING_START() Pin<DEBUG_INTERRUPT_TIMING_PIN>::high()
#define INTERRUPT_TIMING_END() Pin<DEBUG_INTERRUPT_TIMING_PIN>::low()
#else
#define INTERRUPT_TIMING_START()
#define INTERRUPT_TIMING_END()
#endif

#define DEBUG_INTERRUPT_SET_INTERVAL_PIN 0
#if DEBUG_INTERRUPT_SET_INTERVAL_PIN != 0 && UNIT_TEST != 1
#include <Pin.h>
#define SET_INTERVAL_TIMING_START() Pin<DEBUG_INTERRUPT_SET_INTERVAL_PIN>::high()
#define SET_INTERVAL_TIMING_END() Pin<DEBUG_INTERRUPT_SET_INTERVAL_PIN>::low()
#else
#define SET_INTERVAL_TIMING_START()
#define SET_INTERVAL_TIMING_END()
#endif

/**
 * @brief Enumeration of 16 bit timers on atmega2560
 */
enum class Timer : int
{
    TIMER_16 = 16,
    TIMER_17 = 17
};

constexpr TIM_TypeDef *timer_def_from_enum(const Timer timer)
{
    switch (timer)
    {
    case Timer::TIMER_16:
        return TIM16;
    case Timer::TIMER_17:
        return TIM17;
    default:
        return nullptr;
    }
}

template <Timer T>
struct IntervalInterrupt_STM32
{
    static HardwareTimer timer;

    static volatile uint16_t ovf_cnt;
    static volatile uint16_t ovf_left;
    static volatile uint16_t cmp_cnt;

    static volatile timer_callback callback;

    static inline __attribute__((always_inline)) void init()
    {
        timer.setPreloadEnable(false);
        timer.setPrescaleFactor(1);
    }

    static inline __attribute__((always_inline)) void set_interval(uint32_t value)
    {
        SET_INTERVAL_TIMING_START();

        ovf_cnt = (value >> 16);

        if (ovf_cnt)
        {
            ovf_left = ovf_cnt;
            timer.setOverflow(UINT16_MAX);
            timer.attachInterrupt(handle_overflow);
        }
        else
        {
            ovf_left = 0;
            timer.setOverflow((value & 0xFFFF) - 1);
            timer.attachInterrupt(handle_compare_match);
        }

        timer.refresh();
        timer.resume();

        SET_INTERVAL_TIMING_END();
    }

    static inline __attribute__((always_inline)) void handle_overflow()
    {
        INTERRUPT_TIMING_START();

        // decrement amount of left overflows
        ovf_left--;

        // check if this was the last overflow and we need to count the rest
        if (ovf_left == 0)
        {
            timer.attachInterrupt(handle_compare_match);
        }
        INTERRUPT_TIMING_END();
    }

    static inline __attribute__((always_inline)) void handle_compare_match()
    {
        INTERRUPT_TIMING_START();

        // check if we need to switch to overflows again (slow frequency)
        if (ovf_cnt)
        {
            // reset overflow countdown
            ovf_left = ovf_cnt;
            timer.attachInterrupt(handle_overflow);
        }

        // execute the callback
        callback();

        INTERRUPT_TIMING_END();
    }
};

template <Timer T>
HardwareTimer IntervalInterrupt_STM32<T>::timer = HardwareTimer(timer_def_from_enum(T));

template <Timer T>
volatile uint16_t IntervalInterrupt_STM32<T>::ovf_cnt = 0;

template <Timer T>
volatile uint16_t IntervalInterrupt_STM32<T>::ovf_left = 0;

template <Timer T>
volatile timer_callback IntervalInterrupt_STM32<T>::callback = nullptr;

template <Timer T>
void IntervalInterrupt<T>::init()
{
#if DEBUG_INTERRUPT_SET_INTERVAL_PIN
    Pin<DEBUG_INTERRUPT_SET_INTERVAL_PIN>::init();
#endif
#if DEBUG_INTERRUPT_TIMING_PIN
    Pin<DEBUG_INTERRUPT_TIMING_PIN>::init();
#endif

    IntervalInterrupt_STM32<T>::init();
}

template <Timer T>
inline __attribute__((always_inline)) void IntervalInterrupt<T>::setInterval(uint32_t value)
{
    IntervalInterrupt_STM32<T>::set_interval(value);
}

template <Timer T>
void inline __attribute__((always_inline)) IntervalInterrupt<T>::setCallback(timer_callback fn)
{
    IntervalInterrupt_STM32<T>::callback = fn;
}

template <Timer T>
inline __attribute__((always_inline)) void IntervalInterrupt<T>::stop()
{
    IntervalInterrupt_STM32<T>::timer.pause();
}

template <Timer T>
const unsigned long int IntervalInterrupt<T>::FREQ = F_CPU;