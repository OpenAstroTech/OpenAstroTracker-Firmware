#pragma once

#include <stdint.h>

template <uint32_t T_SPR, typename T_PIN_STEP, typename T_PIN_DIR, bool T_INVERT_DIR = false>
class Driver
{
private:
    Driver() = delete;

public:
    constexpr static auto SPR = T_SPR;

    static void init()
    {
        T_PIN_STEP::init();
        T_PIN_DIR::init();
    }

    static inline __attribute__((always_inline)) void step();

    static inline __attribute__((always_inline)) void dir(bool cw);
};

#ifndef DRIVER_CUSTOM_IMPL

template <uint32_t T_SPR, typename T_PIN_STEP, typename T_PIN_DIR, bool T_INVERT_DIR>
inline __attribute__((always_inline)) void Driver<T_SPR, T_PIN_STEP, T_PIN_DIR, T_INVERT_DIR>::step()
{
    T_PIN_STEP::pulse();
}

template <uint32_t T_SPR, typename T_PIN_STEP, typename T_PIN_DIR, bool T_INVERT_DIR>
inline __attribute__((always_inline)) void Driver<T_SPR, T_PIN_STEP, T_PIN_DIR, T_INVERT_DIR>::dir(bool cw)
{
    if (T_INVERT_DIR)
    {
        if (cw)
        {
            T_PIN_DIR::high();
        }
        else
        {
            T_PIN_DIR::low();
        }
    }
    else
    {
        if (cw)
        {
            T_PIN_DIR::low();
        }
        else
        {
            T_PIN_DIR::high();
        }
    }
}

#endif