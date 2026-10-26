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

/// The one log sink the firmware installs. Declared next to the port it points at and defined by the
/// composition root: `OAT_LOG` is for code that owns a logger, `LOG` is for the code that does not —
/// the sink is a property of the process, not of a component.
extern ports::ILogger *const logSink;

/// The legacy spelling, now over `OAT_LOG`: same call sites, but the format literal travels as a
/// `ports::LogFormat` handle (flash on AVR) instead of being built into a `String` at run time, and
/// the whole call compiles out when nothing is logged.
#define LOG(level, format, ...) OAT_LOG(logSink, level, format, ##__VA_ARGS__)
