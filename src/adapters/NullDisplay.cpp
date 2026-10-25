#include "adapters/NullDisplay.hpp"

namespace adapters
{

NullDisplay::NullDisplay(uint8_t columns, uint8_t rows) : _columns(columns), _rows(rows), _brightness(0)
{
}

bool NullDisplay::begin()
{
    return true;
}

uint8_t NullDisplay::columns() const
{
    return _columns;
}

uint8_t NullDisplay::rows() const
{
    return _rows;
}

void NullDisplay::clear()
{
}

void NullDisplay::writeLine(uint8_t row, const char *text)
{
    (void) row;
    (void) text;
}

void NullDisplay::writeCell(uint8_t row, uint8_t column, char glyph)
{
    (void) row;
    (void) column;
    (void) glyph;
}

void NullDisplay::setBacklight(uint8_t level, bool persist)
{
    (void) persist;
    _brightness = level;
}

uint8_t NullDisplay::backlight() const
{
    return _brightness;
}

void NullDisplay::backlightRange(uint8_t *minLevel, uint8_t *maxLevel) const
{
    if (minLevel != nullptr)
    {
        *minLevel = 0;
    }
    if (maxLevel != nullptr)
    {
        *maxLevel = 0;
    }
}

}  // namespace adapters
