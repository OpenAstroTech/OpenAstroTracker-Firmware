#pragma once

#include <stddef.h>
#include <stdint.h>

namespace hal
{

/// Non-volatile byte storage, one cell per address.
///
/// The interface is byte-level on purpose: the on-device layout, the magic markers
/// and the typed accessors live in `EepromLayout.hpp` and the persistent-store
/// adapter. `begin`/`commit` are no-ops on platforms whose EEPROM is written
/// through directly (AVR).
class IEeprom
{
  public:
    virtual ~IEeprom()
    {
    }

    /// Reserves `size` bytes of emulated EEPROM. No-op where not required.
    virtual void begin(size_t size) = 0;

    virtual uint8_t read(size_t address) = 0;

    /// Writes unconditionally, even when the value is unchanged (wears the cell).
    virtual void write(size_t address, uint8_t value) = 0;

    /// Writes only when the value differs.
    virtual void update(size_t address, uint8_t value) = 0;

    /// Flushes buffered writes to the medium. Returns false when the commit failed.
    virtual bool commit() = 0;

    /// Total number of addressable cells.
    virtual size_t length() const = 0;
};

}  // namespace hal
