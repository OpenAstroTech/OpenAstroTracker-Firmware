#pragma once

#include <stdint.h>

namespace ports
{

/// The user-facing menu display.
///
/// The port is a text grid plus backlight control: menu structure, selection arrows and glyph tricks
/// stay in the adapter, which knows the panel's capabilities. The domain only decides *what* each
/// line says.
///
/// There is no cursor: a screen writes whole rows, and `writeCell()` covers the one character the
/// UI paints on its own (the tracking indicator). That keeps row ordering out of the port.
class IDisplay
{
  public:
    virtual ~IDisplay()
    {
    }

    /// Brings the panel up and applies the persisted backlight level.
    virtual bool begin() = 0;

    /// Width of the text grid in characters.
    virtual uint8_t columns() const = 0;

    /// Height of the text grid in rows.
    virtual uint8_t rows() const = 0;

    /// Blanks the whole panel.
    virtual void clear() = 0;

    /// Replaces one row with `text`, padded or truncated to `columns()`.
    virtual void writeLine(uint8_t row, const char *text) = 0;

    /// Writes one character at a cell without touching the rest of the row.
    virtual void writeCell(uint8_t row, uint8_t column, char glyph) = 0;

    /// Sets the backlight level, persisting it when `persist` is set. The menu's brightness screen
    /// previews live and only stores the level the operator settles on.
    virtual void setBacklight(uint8_t level, bool persist) = 0;

    /// The current backlight level, in the panel's own units.
    virtual uint8_t backlight() const = 0;

    /// The levels this panel accepts, so the menu's brightness screen knows its range.
    virtual void backlightRange(uint8_t *minLevel, uint8_t *maxLevel) const = 0;
};

}  // namespace ports
