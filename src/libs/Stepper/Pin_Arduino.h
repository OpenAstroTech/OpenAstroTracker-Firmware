#pragma once

#ifdef ARDUINO

#include "Pin.h"

#include <Arduino.h>

template <uint8_t PIN>
void Pin<PIN>::init()
{
    pinMode(PIN, OUTPUT);
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
    digitalWrite(PIN, HIGH);
}

template <uint8_t PIN>
void inline __attribute__((always_inline)) Pin<PIN>::low()
{
    digitalWrite(PIN, LOW);
}

#endif