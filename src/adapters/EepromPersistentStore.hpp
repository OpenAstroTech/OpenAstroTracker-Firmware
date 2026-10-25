#pragma once

#include <stddef.h>
#include <stdint.h>

#include "hal/IEeprom.hpp"
#include "ports/IPersistentStore.hpp"

namespace adapters
{

/// `ports::IPersistentStore` over a platform EEPROM backend.
///
/// The domain sees durable bytes at an address; the backend decides whether that
/// means an EEPROM cell (AVR), a flash-backed emulation that needs `begin`/`commit`
/// (ESP32, RP2040) or plain RAM (`USE_DUMMY_EEPROM`). The on-device layout — magic
/// markers, item flags, addresses and value encodings — stays with `EepromSettingsFormat`,
/// which is the single writer of this format.
class EepromPersistentStore final : public ports::IPersistentStore
{
  public:
    /// Reserves the storage. What that means is up to the backend.
    EepromPersistentStore(hal::IEeprom &eeprom, size_t size);

    void begin();

    uint8_t readByte(uint16_t address) override;

    void writeByte(uint16_t address, uint8_t value) override;

    bool commit() override;

  private:
    hal::IEeprom &_eeprom;
    size_t _size;
};

}  // namespace adapters
