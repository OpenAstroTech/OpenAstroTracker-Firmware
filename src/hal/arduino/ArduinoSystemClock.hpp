#pragma once

#include <stdint.h>

#include "hal/ISystemClock.hpp"

namespace hal
{

/// `ISystemClock` over the Arduino `millis()`/`micros()`/`delay()` family.
class ArduinoSystemClock final : public ISystemClock
{
  public:
    uint32_t millis() const override;

    uint32_t micros() const override;

    void delay(uint32_t milliseconds) const override;

    void delayMicroseconds(uint16_t microseconds) const override;
};

}  // namespace hal
