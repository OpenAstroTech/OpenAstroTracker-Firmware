#pragma once

namespace ports
{

/// Hands control back to the platform while the domain waits.
///
/// A blocking wait inside the domain must not starve the platform: on AVR that is
/// `yield()` (USB/serial housekeeping), on ESP32 it lets the other core's tasks run.
/// Kept separate from `IClock` because it is a scheduling decision, not a time
/// reading.
class IScheduler
{
  public:
    virtual ~IScheduler()
    {
    }

    /// Runs the platform's pending housekeeping once: `yield()` on AVR, a task yield on ESP32.
    virtual void yield() = 0;
};

}  // namespace ports
