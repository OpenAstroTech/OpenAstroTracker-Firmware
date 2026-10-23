#include "hal/WarningGuard.hpp"

PUSH_NO_WARNINGS
#include <Arduino.h>
POP_NO_WARNINGS

#include "hal/arduino/ArduinoEeprom.hpp"

namespace hal
{

ArduinoEeprom::ArduinoEeprom(EEPROMClass &eeprom) : _eeprom(eeprom)
{
}

void ArduinoEeprom::begin(size_t size)
{
#if defined(ARDUINO_ARCH_AVR)
    (void) size;  // AVR EEPROM is always mapped, nothing to reserve
#else
    _eeprom.begin(size);
#endif
}

uint8_t ArduinoEeprom::read(size_t address)
{
    return _eeprom.read(static_cast<int>(address));
}

void ArduinoEeprom::write(size_t address, uint8_t value)
{
    _eeprom.write(static_cast<int>(address), value);
}

void ArduinoEeprom::update(size_t address, uint8_t value)
{
    // The RP2040 core only offers the templated update(); comparing first gives the
    // same wear-saving behaviour on every platform.
    if (_eeprom.read(static_cast<int>(address)) != value)
    {
        _eeprom.write(static_cast<int>(address), value);
    }
}

bool ArduinoEeprom::commit()
{
#if defined(ARDUINO_ARCH_AVR)
    return true;  // writes are already durable
#else
    return _eeprom.commit();
#endif
}

size_t ArduinoEeprom::length() const
{
    return _eeprom.length();
}

}  // namespace hal
