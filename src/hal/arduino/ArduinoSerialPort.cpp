#include "hal/WarningGuard.hpp"

PUSH_NO_WARNINGS
#include <Arduino.h>
POP_NO_WARNINGS

#include "hal/arduino/ArduinoSerialPort.hpp"

namespace hal
{

ArduinoSerialPort::ArduinoSerialPort(Stream &stream) : _stream(stream)
{
}

int ArduinoSerialPort::available()
{
    return _stream.available();
}

int ArduinoSerialPort::read()
{
    return _stream.read();
}

int ArduinoSerialPort::peek()
{
    return _stream.peek();
}

size_t ArduinoSerialPort::write(uint8_t value)
{
    return _stream.write(value);
}

size_t ArduinoSerialPort::write(const uint8_t *data, size_t length)
{
    return _stream.write(data, length);
}

void ArduinoSerialPort::flush()
{
#if defined(ARDUINO_ARCH_RP2040)
    // The RP2040 port is USB CDC, where flush() can stall when no host is reading.
    // The log path deliberately skipped it there before this backend existed.
#else
    // UART output drains from a ring buffer; flushing makes sure the tail of a log
    // survives a crash immediately after it (matters when debugging on AVR).
    _stream.flush();
#endif
}

}  // namespace hal
