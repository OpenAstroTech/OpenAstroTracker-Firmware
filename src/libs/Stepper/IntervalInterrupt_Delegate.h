#pragma once

#include "IntervalInterrupt.h"
#include "etl/delegate.h"

enum class Timer : int
{
    TIMER_1 = 1,
    TIMER_2 = 2,
    TIMER_3 = 3,
    TIMER_4 = 4,
    TIMER_5 = 5,
};

template <Timer T>
struct IntervalInterrupt_Delegate
{
    static etl::delegate<void()> init;
    static etl::delegate<void(uint32_t)> setInterval;
    static etl::delegate<void(timer_callback)> setCallback;
    static etl::delegate<void()> stop;
};

template <Timer T>
etl::delegate<void()> IntervalInterrupt_Delegate<T>::init = etl::delegate<void()>();

template <Timer T>
etl::delegate<void(uint32_t)> IntervalInterrupt_Delegate<T>::setInterval = etl::delegate<void(uint32_t)>();

template <Timer T>
etl::delegate<void(timer_callback)> IntervalInterrupt_Delegate<T>::setCallback = etl::delegate<void(timer_callback)>();

template <Timer T>
etl::delegate<void()> IntervalInterrupt_Delegate<T>::stop = etl::delegate<void()>();

template <Timer T>
const unsigned long int IntervalInterrupt<T>::FREQ = F_CPU;

template <Timer T>
void IntervalInterrupt<T>::init()
{
    if (IntervalInterrupt_Delegate<T>::init.is_valid())
    {
        IntervalInterrupt_Delegate<T>::init();
    }
}

template <Timer T>
void IntervalInterrupt<T>::setInterval(uint32_t value)
{
    if (IntervalInterrupt_Delegate<T>::setInterval.is_valid())
    {
        IntervalInterrupt_Delegate<T>::setInterval(value);
    }
}

template <Timer T>
void IntervalInterrupt<T>::setCallback(timer_callback fn)
{
    if (IntervalInterrupt_Delegate<T>::setCallback.is_valid())
    {
        IntervalInterrupt_Delegate<T>::setCallback(fn);
    }
}

template <Timer T>
void IntervalInterrupt<T>::stop()
{
    if (IntervalInterrupt_Delegate<T>::stop.is_valid())
    {
        IntervalInterrupt_Delegate<T>::stop();
    }
}