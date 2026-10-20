#pragma once

#include <stdint.h>

namespace hal
{

/// Periodic and one-shot callbacks driven by a hardware timer.
///
/// One instance owns exactly one hardware timer, because timers are shared
/// resources, not per-axis: Timer3 steps RA, Timer4 steps DEC, Timer1 AZ and
/// Timer5 both ALT and Focus on AVR. Which timer an instance owns is decided in the
/// composition root, so the domain never arbitrates timer numbers.
///
/// Callbacks run in interrupt context on every backend. They must not block, must
/// not allocate, and must not call anything that is not documented as ISR-safe.
class ITimerService
{
  public:
    using Callback = void (*)(void *context);

    virtual ~ITimerService()
    {
    }

    /// Starts a repeating callback. Returns false when the timer cannot be started
    /// (already running, or the hardware does not allow the requested period).
    virtual bool startPeriodic(uint32_t periodMicroseconds, Callback callback, void *context) = 0;

    /// Starts a single callback `delayMicroseconds` from now.
    virtual bool startOneShot(uint32_t delayMicroseconds, Callback callback, void *context) = 0;

    virtual void stop() = 0;

    virtual bool isRunning() const = 0;
};

}  // namespace hal
