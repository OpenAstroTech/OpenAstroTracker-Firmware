#pragma once

#include <stddef.h>

namespace ports
{

/// The retained tail of the debug log.
///
/// `:D#` hands a host the last few kilobytes of what the firmware logged, which is the one thing a
/// logger has to keep rather than emit. `ILogger` stays write-only for that reason: a sink that must
/// retain implements this as well, and the command layer holds a nullable pointer to it.
class ILogRetention
{
  public:
    virtual ~ILogRetention()
    {
    }

    /// Copies the retained log into `out`, NUL-terminated, and returns the length written. The
    /// copy is translated for the wire: `#` ends a Meade reply, so any `#` in the log becomes `%`.
    virtual size_t readLog(char *out, size_t capacity) = 0;
};

}  // namespace ports
