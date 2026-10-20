#pragma once

#include <stdint.h>

#include "hal/ISerialPort.hpp"
#include "hal/ISystemClock.hpp"
#include "ports/ILogger.hpp"

namespace adapters
{

/// `ports::ILogger` writing `[uptime]{delta}ms: message` lines to a serial port.
///
/// The format string is read through `ports::LogFormat`, so on AVR it is consumed
/// straight out of flash (`vsnprintf_P`) and never lands in `.data`. Call sites that
/// pass a plain literal still get `F()`-equivalent behaviour through
/// `OAT_LOG_FORMAT()`.
class SerialLogger final : public ports::ILogger
{
  public:
    /// `levelMask` is the configured `DEBUG_LEVEL`: a line is emitted when any of
    /// its category bits is enabled.
    SerialLogger(hal::ISerialPort &port, const hal::ISystemClock &clock, uint16_t levelMask);

    bool isEnabled(uint16_t levelFlags) const override;

    void log(uint16_t levelFlags, ports::LogFormat format, ...) override;

    /// Longest line (including the timestamp prefix and terminator) that is emitted.
    static constexpr size_t LINE_BUFFER_SIZE = 192;

  private:
    hal::ISerialPort &_port;
    const hal::ISystemClock &_clock;
    uint16_t _levelMask;
    uint32_t _lastLogTime;
};

}  // namespace adapters
