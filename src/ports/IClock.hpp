#pragma once

#include <stdint.h>

namespace ports
{

/// The domain's view of time.
///
/// `now()` is a free-running millisecond counter that wraps at 2^32. Always compare
/// with unsigned subtraction (`now - deadline`) so a wrap cannot reverse a deadline.
class IClock
{
  public:
    virtual ~IClock()
    {
    }

    virtual uint32_t now() = 0;

    /// Blocks the caller. Only used where the domain genuinely has to wait for
    /// hardware to settle; scheduling waits belong to controllers.
    virtual void delay(uint32_t milliseconds) = 0;
};

}  // namespace ports
