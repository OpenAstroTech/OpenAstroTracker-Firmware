#pragma once

#include <stdint.h>

#include "hal/ISystemClock.hpp"
#include "ports/IClock.hpp"

namespace adapters
{

/// `ports::IClock` over the platform clock.
///
/// Kept as a separate object from `hal::ISystemClock` because the domain asks a
/// different question ("what time is it") than the HAL answers ("what does the
/// hardware counter say") — and so a test can substitute one without the other.
class ArduinoClock final : public ports::IClock
{
  public:
    explicit ArduinoClock(const hal::ISystemClock &systemClock);

    uint32_t now() override;

    void delay(uint32_t milliseconds) override;

  private:
    const hal::ISystemClock &_systemClock;
};

}  // namespace adapters
