#pragma once

#include "Pin.h"
#include "etl/delegate.h"

template <uint8_t PIN>
class PinDelegate
{
    friend Pin<PIN>;
    static etl::delegate<void(uint8_t, bool)> _delegate;
    static bool initialized;

    static void set(bool value)
    {
        if (_delegate.is_valid())
        {
            _delegate(PIN, value);
        }
    }

public:
    PinDelegate() = delete;

    static void delegate(etl::delegate<void(uint8_t, bool)> delegate)
    {
        _delegate = delegate;
    }
};

template <uint8_t PIN>
etl::delegate<void(uint8_t, bool)> PinDelegate<PIN>::_delegate = etl::delegate<void(uint8_t, bool)>();

template <uint8_t PIN>
bool PinDelegate<PIN>::initialized = false;

template <uint8_t PIN>
void Pin<PIN>::init()
{
    PinDelegate<PIN>::initialized = true;
}

template <uint8_t PIN>
void inline __attribute__((always_inline)) Pin<PIN>::pulse()
{
    high();
    low();
}

template <uint8_t PIN>
void inline __attribute__((always_inline)) Pin<PIN>::high()
{
    PinDelegate<PIN>::set(true);
}

template <uint8_t PIN>
void inline __attribute__((always_inline)) Pin<PIN>::low()
{
    PinDelegate<PIN>::set(false);
}