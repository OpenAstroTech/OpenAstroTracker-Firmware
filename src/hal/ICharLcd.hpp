#pragma once

#include <stdint.h>

namespace hal
{

/// A character LCD, raw or behind an I2C expander.
///
/// Coordinates are zero-based columns and rows; text is always RAM-resident (see
/// `IOledPanel` for why). Custom glyphs are addressed by slot number, matching the
/// Arduino character-LCD convention.
class ICharLcd
{
  public:
    virtual ~ICharLcd()
    {
    }

    virtual bool begin(uint8_t columns, uint8_t rows) = 0;

    virtual void clear() = 0;

    virtual void setCursor(uint8_t column, uint8_t row) = 0;

    virtual void print(const char *text) = 0;

    virtual void write(uint8_t value) = 0;

    /// Defines a 5x8 glyph in slot 0..7.
    virtual void defineGlyph(uint8_t slot, const uint8_t glyph[8]) = 0;

    virtual void backlight(bool on) = 0;

    /// Backlight duty cycle, 0..255.
    virtual void setBacklightLevel(uint8_t level) = 0;
};

}  // namespace hal
