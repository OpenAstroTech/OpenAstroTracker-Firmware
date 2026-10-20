#pragma once

#include <stddef.h>
#include <stdint.h>

namespace hal
{

/// An open bidirectional byte stream: hardware UART, USB CDC or software serial.
///
/// Opening the port (baud rate, pins) is a construction-time concern of the backend;
/// this interface only moves bytes. It is byte-oriented on purpose: `String`,
/// `printf`-style formatting and line framing belong to adapters, which know whether
/// text has to go through flash-aware printing on AVR.
class ISerialPort
{
  public:
    virtual ~ISerialPort()
    {
    }

    /// Bytes waiting in the receive buffer.
    virtual int available() = 0;

    /// Next byte 0..255, or -1 when nothing is buffered.
    virtual int read() = 0;

    /// Next byte without consuming it, or -1 when nothing is buffered.
    virtual int peek() = 0;

    virtual size_t write(uint8_t value) = 0;

    virtual size_t write(const uint8_t *data, size_t length) = 0;

    /// Waits for the transmit buffer to drain where the platform supports it.
    virtual void flush() = 0;
};

}  // namespace hal
