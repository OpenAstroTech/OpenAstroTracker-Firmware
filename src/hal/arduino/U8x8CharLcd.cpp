#include "hal/arduino/U8x8CharLcd.hpp"

#include "../../../Configuration.hpp"

// A board without a menu panel compiles none of this: the panel backends are the only place the
// display libraries are named, so leaving them out keeps a headless build free of them.
#if DISPLAY_TYPE > 0

namespace
{

/// Where one symbol lives in U8g2's fonts: the menu's arrows and marks are icon-font glyphs.
struct SymbolGlyph {
    const uint8_t *font;
    uint8_t encoding;
};

SymbolGlyph glyphFor(hal::CharSymbol symbol)
{
    switch (symbol)
    {
        case hal::CharSymbol::RightArrow:
            return {u8x8_font_open_iconic_arrow_1x1, 64 + 14};
        case hal::CharSymbol::LeftArrow:
            return {u8x8_font_open_iconic_arrow_1x1, 64 + 13};
        case hal::CharSymbol::UpArrow:
            return {u8x8_font_open_iconic_arrow_1x1, 64 + 15};
        case hal::CharSymbol::DownArrow:
            return {u8x8_font_open_iconic_arrow_1x1, 64 + 12};
        case hal::CharSymbol::Degrees:
            return {u8x8_font_7x14_1x2_f, 176};
        case hal::CharSymbol::Minutes:
            return {u8x8_font_7x14_1x2_f, '\''};
        case hal::CharSymbol::Tracking:
            return {u8x8_font_open_iconic_thing_1x1, 64 + 15};
        case hal::CharSymbol::NoTracking:
        default:
            return {u8x8_font_open_iconic_thing_1x1, 64 + 4};
    }
}

}  // namespace

namespace hal
{

U8x8CharLcd::U8x8CharLcd() : _lcd(U8X8_PIN_NONE)
{
}

bool U8x8CharLcd::begin(uint8_t columns, uint8_t rows)
{
    (void) columns;
    (void) rows;

    _lcd.begin();
    _lcd.setPowerSave(0);
    _lcd.clear();
    _lcd.setFont(u8x8_font_7x14_1x2_f);  // Each 7x14 character takes up two 8-pixel rows.
    return true;
}

void U8x8CharLcd::clear()
{
    _lcd.clear();
}

void U8x8CharLcd::setCursor(uint8_t column, uint8_t row)
{
    _lcd.setCursor(column, static_cast<uint8_t>(row * 2));
}

void U8x8CharLcd::print(const char *text)
{
    for (const char *p = text; *p != '\0'; p++)
    {
        write(static_cast<uint8_t>(*p));
    }
}

void U8x8CharLcd::write(uint8_t value)
{
    _lcd.setFont(u8x8_font_7x14_1x2_f);
    _lcd.drawGlyph(_lcd.tx, _lcd.ty, value);
    _lcd.tx++;
}

bool U8x8CharLcd::hasDefinableSymbols() const
{
    return false;
}

void U8x8CharLcd::defineSymbol(CharSymbol symbol, const uint8_t bitmap[8])
{
    (void) symbol;
    (void) bitmap;
}

void U8x8CharLcd::writeSymbol(CharSymbol symbol)
{
    const SymbolGlyph glyph = glyphFor(symbol);
    _lcd.setFont(glyph.font);
    _lcd.drawGlyph(_lcd.tx, _lcd.ty, glyph.encoding);
    _lcd.setFont(u8x8_font_7x14_1x2_f);
    _lcd.tx++;
}

void U8x8CharLcd::setBacklightLevel(uint8_t level)
{
    _lcd.setContrast(level);
}

}  // namespace hal

#endif
