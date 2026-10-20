#pragma once

#include <stdint.h>

#include "hal/IGpioPin.hpp"

namespace hal
{

/// `IGpioPin` over the Arduino pin API for one pin number.
class ArduinoGpioPin final : public IGpioPin
{
  public:
    explicit ArduinoGpioPin(uint8_t pin);

    void configure(PinMode mode) override;

    void write(PinLevel level) override;

    PinLevel read() const override;

    uint16_t analogRead() const override;

    void writeAnalog(uint8_t duty) override;

  private:
    uint8_t _pin;
};

}  // namespace hal
