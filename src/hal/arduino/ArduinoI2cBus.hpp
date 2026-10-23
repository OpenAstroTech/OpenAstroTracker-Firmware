#pragma once

#include <stddef.h>
#include <stdint.h>

#include "hal/WarningGuard.hpp"

PUSH_NO_WARNINGS
#include <Wire.h>
POP_NO_WARNINGS

#include "hal/II2cBus.hpp"

namespace hal
{

/// `II2cBus` over the Arduino `Wire` library.
///
/// The library exposes a transaction API rather than a buffer API, so each call here is one
/// begin/write/end sequence. `Wire` is a per-platform singleton; this wrapper keeps the
/// library name inside `hal/` and gives adapters something a fake can stand in for.
class ArduinoI2cBus final : public II2cBus
{
  public:
    void begin() override;
    void setClock(uint32_t clockHz) override;

    bool write(uint8_t address, const uint8_t *data, size_t length) override;
    bool read(uint8_t address, uint8_t *data, size_t length) override;

    bool writeRegister(uint8_t address, uint8_t reg, uint8_t value) override;
    bool readRegisters(uint8_t address, uint8_t reg, uint8_t *data, size_t length) override;
};

}  // namespace hal
