#pragma once

#include <stddef.h>
#include <stdint.h>

#include "hal/WarningGuard.hpp"

PUSH_NO_WARNINGS
#include <EEPROM.h>
POP_NO_WARNINGS

#include "hal/IEeprom.hpp"

namespace hal
{

/// `IEeprom` over the Arduino `EEPROM` object.
///
/// ESP32 and RP2040 emulate EEPROM in flash behind a RAM buffer, so `begin()` reserves
/// bytes and `commit()` is what makes them durable. AVR writes through to the cell
/// immediately, so both are no-ops there.
class ArduinoEeprom final : public IEeprom
{
  public:
    explicit ArduinoEeprom(EEPROMClass &eeprom);

    void begin(size_t size) override;

    uint8_t read(size_t address) override;

    void write(size_t address, uint8_t value) override;

    void update(size_t address, uint8_t value) override;

    bool commit() override;

    size_t length() const override;

  private:
    EEPROMClass &_eeprom;
};

}  // namespace hal
