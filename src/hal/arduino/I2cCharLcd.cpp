#include "hal/arduino/I2cCharLcd.hpp"

#include "../../../Configuration.hpp"

// A board without a menu panel compiles none of this: the panel backends are the only place the
// display libraries are named, so leaving them out keeps a headless build free of them.
#if DISPLAY_TYPE > 0

    #include <string.h>

namespace
{

/// `LiquidTWI2`'s backlight states, spelled out because the header drops its macros.
constexpr uint8_t kBacklightOn  = 0x1;  // `RED`: the single-line expander backlight
constexpr uint8_t kBacklightOff = 0x0;

}  // namespace

namespace hal
{

I2cCharLcd::I2cCharLcd(uint8_t address, bool sixteenBitExpander) : _lcd(address), _sixteenBitExpander(sixteenBitExpander)
{
}

bool I2cCharLcd::begin(uint8_t columns, uint8_t rows)
{
    _lcd.setMCPType(_sixteenBitExpander ? LTI_TYPE_MCP23017 : LTI_TYPE_MCP23008);
    _lcd.begin(columns, rows);
    _lcd.setBacklight(kBacklightOn);
    return true;
}

void I2cCharLcd::clear()
{
    _lcd.clear();
}

void I2cCharLcd::setCursor(uint8_t column, uint8_t row)
{
    _lcd.setCursor(column, row);
}

void I2cCharLcd::print(const char *text)
{
    _lcd.print(text);
}

void I2cCharLcd::write(uint8_t value)
{
    _lcd.write(value);
}

bool I2cCharLcd::hasDefinableSymbols() const
{
    return true;
}

void I2cCharLcd::defineSymbol(CharSymbol symbol, const uint8_t bitmap[8])
{
    static uint8_t slots[8][8];
    const uint8_t slot = static_cast<uint8_t>(symbol);
    memcpy(slots[slot], bitmap, 8);
    _lcd.createChar(slot, slots[slot]);
}

void I2cCharLcd::writeSymbol(CharSymbol symbol)
{
    _lcd.write(static_cast<uint8_t>(symbol));
}

void I2cCharLcd::setBacklightLevel(uint8_t level)
{
    // The expander's backlight is a single on/off line: no dimming.
    _lcd.setBacklight(level > 0 ? kBacklightOn : kBacklightOff);
}

uint8_t I2cCharLcd::readExpanderButtons()
{
    return _lcd.readButtons();
}

}  // namespace hal

#endif
