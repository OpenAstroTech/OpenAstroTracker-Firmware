#pragma once

#include <stdint.h>

namespace hal
{

/// Raw time and blocking delay as the platform provides it.
///
/// This is the HAL capability behind the domain's `IClock` port: it reports what
/// the hardware thinks the time is. Anything that needs a *deadline* belongs in a
/// port or a domain controller, not here.
class ISystemClock
{
  public:
    virtual ~ISystemClock()
    {
    }

    /// Milliseconds since boot. Wraps at 2^32 on every platform this firmware targets.
    virtual uint32_t millis() const = 0;

    /// Microseconds since boot. Wraps at 2^32 on every platform this firmware targets.
    virtual uint32_t micros() const = 0;

    /// Busy/sleep delay. Blocks the calling context.
    virtual void delay(uint32_t milliseconds) const = 0;

    /// Busy delay. Blocks the calling context.
    virtual void delayMicroseconds(uint16_t microseconds) const = 0;
};

}  // namespace hal
