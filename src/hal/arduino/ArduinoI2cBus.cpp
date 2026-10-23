#include "ArduinoI2cBus.hpp"

namespace hal
{

void ArduinoI2cBus::begin()
{
    Wire.begin();
}

void ArduinoI2cBus::setClock(uint32_t clockHz)
{
    Wire.setClock(clockHz);
}

bool ArduinoI2cBus::write(uint8_t address, const uint8_t *data, size_t length)
{
    Wire.beginTransmission(address);
    for (size_t i = 0; i < length; i++)
    {
        Wire.write(data[i]);
    }
    return Wire.endTransmission(true) == 0;
}

bool ArduinoI2cBus::read(uint8_t address, uint8_t *data, size_t length)
{
    const size_t received = Wire.requestFrom(address, static_cast<uint8_t>(length), static_cast<uint8_t>(1));
    if (received != length)
    {
        return false;
    }
    for (size_t i = 0; i < length; i++)
    {
        if (Wire.available() == 0)
        {
            return false;
        }
        data[i] = static_cast<uint8_t>(Wire.read());
    }
    return true;
}

bool ArduinoI2cBus::writeRegister(uint8_t address, uint8_t reg, uint8_t value)
{
    const uint8_t payload[2] = {reg, value};
    return write(address, payload, sizeof(payload));
}

bool ArduinoI2cBus::readRegisters(uint8_t address, uint8_t reg, uint8_t *data, size_t length)
{
    Wire.beginTransmission(address);
    Wire.write(reg);
    // `false` keeps the bus for the repeated start the read needs.
    if (Wire.endTransmission(false) != 0)
    {
        return false;
    }
    return read(address, data, length);
}

}  // namespace hal
