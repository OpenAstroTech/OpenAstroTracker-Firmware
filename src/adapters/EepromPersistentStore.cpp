#include "adapters/EepromPersistentStore.hpp"

namespace adapters
{

EepromPersistentStore::EepromPersistentStore(hal::IEeprom &eeprom, size_t size) : _eeprom(eeprom), _size(size)
{
}

void EepromPersistentStore::begin()
{
    _eeprom.begin(_size);
}

uint8_t EepromPersistentStore::readByte(uint16_t address)
{
    return _eeprom.read(address);
}

void EepromPersistentStore::writeByte(uint16_t address, uint8_t value)
{
    _eeprom.write(address, value);
}

bool EepromPersistentStore::commit()
{
    return _eeprom.commit();
}

}  // namespace adapters
