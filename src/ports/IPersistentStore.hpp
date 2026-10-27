#pragma once

#include <stdint.h>

namespace ports
{

/// Durable byte storage owned by the domain.
///
/// Deliberately byte-addressed: the persistent *format* (addresses, magic markers,
/// value encoding) belongs to the EEPROM adapter together with
/// `hal::eeprom`'s layout, so the domain's data model can change without a port
/// change. A domain-typed accessor layer sits on top of this in the adapter.
class IPersistentStore
{
  public:
    virtual ~IPersistentStore()
    {
    }

    /// Reads one byte. An address that has never been written reads as 0xFF (erased).
    virtual uint8_t readByte(uint16_t address) = 0;

    /// Writes one byte. Buffered writes are flushed by `commit()`.
    virtual void writeByte(uint16_t address, uint8_t value) = 0;

    /// Flushes buffered writes. Returns false when the medium rejected them.
    virtual bool commit() = 0;
};

}  // namespace ports
