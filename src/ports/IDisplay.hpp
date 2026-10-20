#pragma once

#include <stdint.h>

namespace ports
{

/// The user-facing menu display.
///
/// The port is a text grid plus backlight control: menu structure, selection
/// arrows and glyph tricks stay in the display adapter, which knows the panel's
/// capabilities. The domain only decides *what* each line says.
class IDisplay
{
  public:
    virtual ~IDisplay()
    {
    }

    virtual uint8_t columns() const = 0;

    virtual uint8_t rows() const = 0;

    virtual void clear() = 0;

    /// Replaces one row with `text`, padded or truncated to `columns()`.
    virtual void writeLine(uint8_t row, const char *text) = 0;

    virtual void setBacklight(uint8_t level) = 0;

    virtual uint8_t backlight() const = 0;
};

}  // namespace ports
