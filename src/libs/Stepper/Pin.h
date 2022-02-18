#pragma once

#include <stdint.h>

template <uint8_t PIN>
class Pin
{
public:
    static void init();

    static inline __attribute__((always_inline)) void pulse();

    static inline __attribute__((always_inline)) void high();

    static inline __attribute__((always_inline)) void low();

private:
    Pin() = delete;
};

#ifndef PIN_CUSTOM_IMPL

#if defined(ARDUINO_ARCH_AVR)
#include "Pin_AVR.h"
#elif defined(ARDUINO)
#include "Pin_Arduino.h"
#else
#include "Pin_Delegate.h"
#endif

#endif // PIN_CUSTOM_IMPL