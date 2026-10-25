#pragma once

#include <stdint.h>

#include "../../../Configuration.hpp"

// A board without a menu panel compiles none of this: the panel backends are the only place the
// display libraries are named, so leaving them out keeps a headless build free of them.
#if DISPLAY_TYPE > 0

    #include "hal/WarningGuard.hpp"

PUSH_NO_WARNINGS
    #include <LiquidTWI2.h>
POP_NO_WARNINGS

    #include "hal/ICharLcd.hpp"

namespace hal
{

/// `ICharLcd` over the MCP23008/MCP23017 I2C expander driven by `LiquidTWI2`.
///
/// The same chip also scans the keypad, so `readExpanderButtons()` is the raw bitmask the button
/// backend consumes — one object serves both the display and the keys, as the wiring requires.
class I2cCharLcd final : public ICharLcd
{
  public:
    explicit I2cCharLcd(uint8_t address = 0x20, bool sixteenBitExpander = false);

    bool begin(uint8_t columns, uint8_t rows) override;

    void clear() override;

    void setCursor(uint8_t column, uint8_t row) override;

    void print(const char *text) override;

    void write(uint8_t value) override;

    bool hasDefinableSymbols() const override;

    void defineSymbol(CharSymbol symbol, const uint8_t bitmap[8]) override;

    void writeSymbol(CharSymbol symbol) override;

    void setBacklightLevel(uint8_t level) override;

    /// The raw key bitmask the expander reports.
    uint8_t readExpanderButtons();

  private:
    LiquidTWI2 _lcd;
    bool _sixteenBitExpander;
};

}  // namespace hal

#endif
