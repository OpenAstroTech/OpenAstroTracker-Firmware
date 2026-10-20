#pragma once

#include <stddef.h>
#include <stdint.h>

#include "hal/ISerialPort.hpp"

class Stream;

namespace hal
{

/// `ISerialPort` over any Arduino `Stream` (HardwareSerial, SerialUSB,
/// SoftwareSerial). The stream's owner configures the port; this class only moves
/// bytes, so it needs no platform `#ifdef`.
class ArduinoSerialPort final : public ISerialPort
{
  public:
    explicit ArduinoSerialPort(Stream &stream);

    int available() override;

    int read() override;

    int peek() override;

    size_t write(uint8_t value) override;

    size_t write(const uint8_t *data, size_t length) override;

    void flush() override;

  private:
    Stream &_stream;
};

}  // namespace hal
