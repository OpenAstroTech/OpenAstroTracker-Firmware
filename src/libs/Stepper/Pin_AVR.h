#pragma once

#ifdef ARDUINO_ARCH_AVR

#include "Pin.h"

#include <Arduino.h>

template <uint8_t PIN>
struct PinRegisters
{
    static volatile uint8_t *_out_reg;
    static uint8_t _bit;
    static uint8_t _neg;

    static void init()
    {
        _out_reg = portOutputRegister(digitalPinToPort(PIN));
        _bit = digitalPinToBitMask(PIN);
        _neg = ~_bit;
        *portModeRegister(digitalPinToPort(PIN)) |= _bit;
    }
};

template <uint8_t PIN>
volatile uint8_t *PinRegisters<PIN>::_out_reg;

template <uint8_t PIN>
uint8_t PinRegisters<PIN>::_bit;

template <uint8_t PIN>
uint8_t PinRegisters<PIN>::_neg;

template <uint8_t PIN>
void Pin<PIN>::init()
{
    PinRegisters<PIN>::init();
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
    *PinRegisters<PIN>::_out_reg |= PinRegisters<PIN>::_bit;
}

template <uint8_t PIN>
void inline __attribute__((always_inline)) Pin<PIN>::low()
{
    *PinRegisters<PIN>::_out_reg &= PinRegisters<PIN>::_neg;
}

#endif