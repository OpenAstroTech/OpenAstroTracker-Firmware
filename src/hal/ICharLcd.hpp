#pragma once

#include <stdint.h>

namespace hal
{

/// The symbols the menu paints that are not plain ASCII.
///
/// One slot list for every menu panel: a character LCD defines these as custom glyphs, a panel that
/// renders from an icon font maps them to font glyphs. The eight slots match the HD44780 limit on
/// user-defined characters, so nothing is lost by sharing the enum.
enum class CharSymbol : uint8_t
{
    RightArrow,
    LeftArrow,
    UpArrow,
    DownArrow,
    Degrees,
    Minutes,
    Tracking,
    NoTracking,
};

/// A character LCD, raw or behind an I2C expander or an OLED controller.
///
/// Every menu panel the firmware supports is a character grid, which is why one contract covers
/// the parallel shield, the MCP23008/17 expanders and the 128x32 OLED. Coordinates are zero-based
/// columns and rows.
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

    /// True when the panel has user-defined character slots (`defineSymbol` does something).
    virtual bool hasDefinableSymbols() const = 0;

    /// Defines the 5x8 bitmap for one symbol slot. A no-op on panels without user glyphs.
    virtual void defineSymbol(CharSymbol symbol, const uint8_t bitmap[8]) = 0;

    /// Draws one symbol at the cursor and advances it.
    virtual void writeSymbol(CharSymbol symbol) = 0;

    /// Backlight duty cycle, 0..255. Panels that only switch the backlight treat 0 as off.
    virtual void setBacklightLevel(uint8_t level) = 0;
};

}  // namespace hal
