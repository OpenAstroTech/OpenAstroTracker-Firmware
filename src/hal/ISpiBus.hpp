#pragma once

#include <stddef.h>
#include <stdint.h>

namespace hal
{

/// An SPI master bus. Settings are latched per transaction so several devices can
/// share one bus at different clocks or modes.
class ISpiBus
{
  public:
    virtual ~ISpiBus()
    {
    }

    virtual void begin() = 0;

    virtual void end() = 0;

    /// `dataMode` is the SPI mode (0..3), matching the Arduino convention.
    virtual void beginTransaction(uint32_t clockHz, uint8_t dataMode) = 0;

    virtual void endTransaction() = 0;

    virtual uint8_t transfer(uint8_t value) = 0;

    /// Full-duplex transfer of `length` bytes; `received` may be nullptr to discard.
    virtual void transfer(const uint8_t *transmit, uint8_t *received, size_t length) = 0;
};

}  // namespace hal
