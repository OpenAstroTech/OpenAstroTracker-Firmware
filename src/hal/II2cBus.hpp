#pragma once

#include <stddef.h>
#include <stdint.h>

namespace hal
{

/// An I2C master bus. `address` is the 7-bit device address.
class II2cBus
{
  public:
    virtual ~II2cBus()
    {
    }

    virtual void begin() = 0;

    virtual void setClock(uint32_t clockHz) = 0;

    /// Writes `length` bytes and reports whether the device acknowledged.
    virtual bool write(uint8_t address, const uint8_t *data, size_t length) = 0;

    /// Reads exactly `length` bytes and reports whether the device responded.
    virtual bool read(uint8_t address, uint8_t *data, size_t length) = 0;

    /// Writes one register address followed by one value.
    virtual bool writeRegister(uint8_t address, uint8_t reg, uint8_t value) = 0;

    /// Writes a register address, then reads `length` bytes back (repeated start).
    virtual bool readRegisters(uint8_t address, uint8_t reg, uint8_t *data, size_t length) = 0;
};

}  // namespace hal
