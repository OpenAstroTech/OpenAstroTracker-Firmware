#pragma once

#include <stdint.h>

namespace hal
{

enum class PinMode : uint8_t
{
    Input,
    InputPullup,
    Output,
};

enum class PinLevel : uint8_t
{
    Low  = 0,
    High = 1,
};

/// A single addressable GPIO pin, digital and (where the platform has one) analog.
///
/// One instance wraps one pin number. Edge interrupts are deliberately absent:
/// nothing in the firmware needs them yet, and the polling loops that use pins
/// today (`HallSensorHoming`, `EndSwitch`) are driven from the main loop.
class IGpioPin
{
  public:
    virtual ~IGpioPin()
    {
    }

    virtual void configure(PinMode mode) = 0;

    virtual void write(PinLevel level) = 0;

    virtual PinLevel read() const = 0;

    /// Raw ADC reading, 0..1023 on AVR and 0..4095 on ESP32/RP2040.
    virtual uint16_t analogRead() const = 0;

    /// PWM duty cycle as the platform understands it (8 bit on AVR and RP2040).
    virtual void writeAnalog(uint8_t duty) = 0;
};

}  // namespace hal
