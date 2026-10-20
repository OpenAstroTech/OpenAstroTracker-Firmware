#pragma once

#include <stdint.h>

#include "LogFormat.hpp"

namespace ports
{

/// Where diagnostics go.
///
/// `levelFlags` is the firmware's existing bit-mask of debug categories
/// (`DEBUG_SERIAL`, `DEBUG_MOUNT`, …), so the port can replace the `LOG()` macro
/// without changing which lines a build prints.
class ILogger
{
  public:
    virtual ~ILogger()
    {
    }

    /// Cheap check for callers that would otherwise format an expensive argument.
    virtual bool isEnabled(uint16_t levelFlags) const = 0;

    /// Formats and emits one line. Implementations must read `format` with their
    /// platform's flash-aware printer (see `LogFormat`).
    virtual void log(uint16_t levelFlags, LogFormat format, ...) = 0;
};

}  // namespace ports
