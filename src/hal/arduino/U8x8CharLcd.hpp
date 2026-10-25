#pragma once

#include <stdint.h>

#include "../../../Configuration.hpp"

// A board without a menu panel compiles none of this: the panel backends are the only place the
// display libraries are named, so leaving them out keeps a headless build free of them.
#if DISPLAY_TYPE > 0

    #include "hal/WarningGuard.hpp"

PUSH_NO_WARNINGS
    #include <U8x8lib.h>
POP_NO_WARNINGS

    #include "hal/ICharLcd.hpp"

namespace hal
{

/// `ICharLcd` over the 128x32 SSD1306 menu panel driven by `U8g2`'s character API.
///
/// One character is a 7x14 font cell, i.e. two 8-pixel rows, which is why `setCursor()` doubles the
/// row. The panel has no user-defined characters: the menu's arrows and degree signs come from
/// U8g2's icon fonts, mapped in `writeSymbol()`.
class U8x8CharLcd final : public ICharLcd
{
  public:
    U8x8CharLcd();

    bool begin(uint8_t columns, uint8_t rows) override;

    void clear() override;

    void setCursor(uint8_t column, uint8_t row) override;

    void print(const char *text) override;

    void write(uint8_t value) override;

    bool hasDefinableSymbols() const override;

    void defineSymbol(CharSymbol symbol, const uint8_t bitmap[8]) override;

    void writeSymbol(CharSymbol symbol) override;

    void setBacklightLevel(uint8_t level) override;

  private:
    U8X8_SSD1306_128X32_UNIVISION_HW_I2C _lcd;
};

}  // namespace hal

#endif
