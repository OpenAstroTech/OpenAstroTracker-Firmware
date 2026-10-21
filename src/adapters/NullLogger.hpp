#pragma once

#include <stdint.h>

#include "ports/ILogger.hpp"

namespace adapters
{
/// `ports::ILogger` that discards everything — the null object for builds with
/// logging compiled out (`DEBUG_LEVEL == DEBUG_NONE`).
///
/// Its purpose is RAM: a `SerialLogger` and the serial port behind it cost a vtable
/// and a few bytes each, which the ATmega2560 cannot spare for a sink that
/// `OAT_LOG()` will never call. It also gives code that owns a logger pointer
/// something safe to default to.
class NullLogger final : public ports::ILogger
{
  public:
    bool isEnabled(uint16_t) const override
    {
        return false;
    }

    void log(uint16_t, ports::LogFormat, ...) override
    {
    }
};

}  // namespace adapters
