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
    _stream.flush();
}

}  // namespace hal
