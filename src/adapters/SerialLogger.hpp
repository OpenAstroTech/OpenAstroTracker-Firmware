#pragma once

#include <stdint.h>

#include "hal/ISerialPort.hpp"
#include "hal/ISystemClock.hpp"
#include "ports/ILogRetention.hpp"
#include "ports/ILogger.hpp"

namespace adapters
{

/// `ports::ILogger` writing `[uptime]{delta}ms: message` lines to a serial port.
///
/// One line is one `write()` call, and the platform decides whether it is flushed
/// (`hal::ArduinoSerialPort::flush()` skips the RP2040 USB CDC, where it can stall).
///
/// The format string is read through `ports::LogFormat`, so on AVR it is consumed
/// straight out of flash (`vsnprintf_P`) and never lands in `.data`. Call sites that
/// pass a plain literal still get `F()`-equivalent behaviour through
/// `OAT_LOG_FORMAT()`.
class SerialLogger final : public ports::ILogger, public ports::ILogRetention
{
  public:
    /// `levelMask` is the configured `DEBUG_LEVEL`: a line is emitted when any of
    /// its category bits is enabled.
    SerialLogger(hal::ISerialPort &port, const hal::ISystemClock &clock, uint16_t levelMask);

    bool isEnabled(uint16_t levelFlags) const override;

    void log(uint16_t levelFlags, ports::LogFormat format, ...) override;

    /// The retained tail `:D#` hands out, in bytes. The same size the legacy ring used.
    static constexpr size_t LOG_BUFFER_SIZE = 4096;

    /// Copies the retained tail out; see `ports::ILogRetention`.
    size_t readLog(char *out, size_t capacity) override;

    /// Longest line (including the timestamp prefix and terminator) that is emitted.
    static constexpr size_t LINE_BUFFER_SIZE = 192;

  private:
    /// Appends one formatted line to the ring, dropping the oldest whole lines when it fills.
    void retain(const char *line, size_t length);

    hal::ISerialPort &_port;
    const hal::ISystemClock &_clock;
    uint16_t _levelMask;
    uint32_t _lastLogTime;

    char _ring[LOG_BUFFER_SIZE] = {};
    size_t _ringWrite           = 0;
    size_t _ringFilled          = 0;
};

}  // namespace adapters
