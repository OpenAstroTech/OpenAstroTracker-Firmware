#include "adapters/LcdMenuDisplay.hpp"

#include <string.h>

namespace
{

using hal::CharSymbol;

/// The custom glyphs, for the panels that have user-defined character slots.
struct SymbolBitmap {
    CharSymbol symbol;
    uint8_t bitmap[8];
};

const SymbolBitmap kSymbols[] = {
    {CharSymbol::RightArrow, {0x00, 0x08, 0x0C, 0x0E, 0x0C, 0x08, 0x00, 0x00}},
    {CharSymbol::LeftArrow, {0x00, 0x02, 0x06, 0x0E, 0x06, 0x02, 0x00, 0x00}},
    {CharSymbol::UpArrow, {0x04, 0x0E, 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04}},
    {CharSymbol::DownArrow, {0x04, 0x04, 0x04, 0x04, 0x04, 0x1F, 0x0E, 0x04}},
    {CharSymbol::Degrees, {0x0C, 0x12, 0x12, 0x0C, 0x00, 0x00, 0x00, 0x00}},
    {CharSymbol::Minutes, {0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {CharSymbol::Tracking, {0x17, 0x02, 0x12, 0x02, 0x17, 0x05, 0x16, 0x05}},
    {CharSymbol::NoTracking, {0x10, 0x00, 0x10, 0x02, 0x10, 0x00, 0x10, 0x00}},
};

/// The characters the UI writes that the panel paints from its own glyph set.
bool symbolFor(char glyph, CharSymbol &symbol)
{
    switch (glyph)
    {
        case '>':
            symbol = CharSymbol::RightArrow;
            return true;
        case '<':
            symbol = CharSymbol::LeftArrow;
            return true;
        case '^':
            symbol = CharSymbol::UpArrow;
            return true;
        case '~':
            symbol = CharSymbol::DownArrow;
            return true;
        case '@':
            symbol = CharSymbol::Degrees;
            return true;
        case '\'':
            symbol = CharSymbol::Minutes;
            return true;
        case '&':
            symbol = CharSymbol::Tracking;
            return true;
        case '`':
            symbol = CharSymbol::NoTracking;
            return true;
        default:
            return false;
    }
}

uint8_t clampLevel(uint8_t level, const adapters::LcdMenuDisplay::BacklightRange &range)
{
    if (level < range.minimum)
    {
        return range.minimum;
    }
    if (level > range.maximum)
    {
        return range.maximum;
    }
    return level;
}

}  // namespace

namespace adapters
{

LcdMenuDisplay::LcdMenuDisplay(hal::ICharLcd &panel, ports::IMountSettings *settings, uint8_t columns, uint8_t rows, BacklightRange range)
    : _panel(panel), _settings(settings), _columns(columns), _rows(rows), _range(range), _brightness(range.minimum)
{
    for (uint8_t row = 0; row < 2; row++)
    {
        _lastLine[row][0] = '\0';
        _lineValid[row]   = false;
    }
}

bool LcdMenuDisplay::begin()
{
    const bool started = _panel.begin(_columns, _rows);
    _panel.clear();

    if (_panel.hasDefinableSymbols())
    {
        for (const SymbolBitmap &entry : kSymbols)
        {
            _panel.defineSymbol(entry.symbol, entry.bitmap);
        }
    }

    // Apply the stored level without writing it back: nothing changed yet.
    const uint8_t stored = (_settings != nullptr) ? _settings->getBrightness() : 10;
    applyBacklight(clampLevel(stored, _range));
    return started;
}

uint8_t LcdMenuDisplay::columns() const
{
    return _columns;
}

uint8_t LcdMenuDisplay::rows() const
{
    return _rows;
}

void LcdMenuDisplay::clear()
{
    _panel.clear();
    for (uint8_t row = 0; row < 2; row++)
    {
        _lineValid[row] = false;
    }
}

void LcdMenuDisplay::writeLine(uint8_t row, const char *text)
{
    if (row >= 2)
    {
        return;
    }

    char padded[kMaxColumns + 1];
    const size_t width = (_columns < kMaxColumns) ? _columns : kMaxColumns;
    size_t length      = (text != nullptr) ? strlen(text) : 0;
    if (length > width)
    {
        length = width;
    }
    if (length > 0)
    {
        memcpy(padded, text, length);
    }
    for (size_t i = length; i < width; i++)
    {
        padded[i] = ' ';
    }
    padded[width] = '\0';

    if (_lineValid[row] && (strcmp(_lastLine[row], padded) == 0))
    {
        return;
    }

    _panel.setCursor(0, row);
    for (size_t i = 0; i < width; i++)
    {
        writeChar(padded[i]);
    }

    memcpy(_lastLine[row], padded, width + 1);
    _lineValid[row] = true;
}

void LcdMenuDisplay::writeCell(uint8_t row, uint8_t column, char glyph)
{
    if (row >= 2 || column >= _columns)
    {
        return;
    }
    _panel.setCursor(column, row);
    writeChar(glyph);
    // The row no longer matches its copy, so the next full write must go out.
    _lineValid[row] = false;
}

void LcdMenuDisplay::setBacklight(uint8_t level, bool persist)
{
    applyBacklight(clampLevel(level, _range));
    if (persist && _settings != nullptr)
    {
        _settings->storeBrightness(_brightness);
    }
}

uint8_t LcdMenuDisplay::backlight() const
{
    return _brightness;
}

void LcdMenuDisplay::backlightRange(uint8_t *minLevel, uint8_t *maxLevel) const
{
    if (minLevel != nullptr)
    {
        *minLevel = _range.minimum;
    }
    if (maxLevel != nullptr)
    {
        *maxLevel = _range.maximum;
    }
}

void LcdMenuDisplay::writeChar(char glyph)
{
    CharSymbol symbol;
    if (symbolFor(glyph, symbol))
    {
        _panel.writeSymbol(symbol);
        return;
    }
    _panel.write(static_cast<uint8_t>(glyph));
}

void LcdMenuDisplay::applyBacklight(uint8_t level)
{
    _brightness = level;
    _panel.setBacklightLevel(level);
}

}  // namespace adapters
