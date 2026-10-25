#pragma once

#include <stdint.h>
#include <string.h>

#include "hal/ICharLcd.hpp"

namespace fakes
{

/// A character panel that records what the adapter draws.
///
/// The adapter's job is the symbol substitution, the padding and the "do not redraw" rule, so the
/// fake keeps a character grid plus counters rather than pretending to be a display.
class FakeCharLcd final : public hal::ICharLcd
{
  public:
    static constexpr uint8_t kMaxColumns = 24;
    static constexpr uint8_t kMaxRows    = 2;

    bool begin(uint8_t columns, uint8_t rows) override
    {
        beginCalls++;
        beginColumns = columns;
        beginRows    = rows;
        return beginResult;
    }

    void clear() override
    {
        clearCalls++;
        for (uint8_t row = 0; row < kMaxRows; row++)
        {
            memset(grid[row], ' ', kMaxColumns);
            grid[row][kMaxColumns] = '\0';
        }
        cursorRow    = 0;
        cursorColumn = 0;
    }

    void setCursor(uint8_t column, uint8_t row) override
    {
        cursorColumn = column;
        cursorRow    = row;
    }

    void print(const char *text) override
    {
        for (const char *p = text; p != nullptr && *p != '\0'; p++)
        {
            write(static_cast<uint8_t>(*p));
        }
    }

    void write(uint8_t value) override
    {
        place(static_cast<char>(value));
    }

    bool hasDefinableSymbols() const override
    {
        return definable;
    }

    void defineSymbol(hal::CharSymbol symbol, const uint8_t bitmap[8]) override
    {
        (void) bitmap;
        definedSymbols[static_cast<uint8_t>(symbol)] = true;
    }

    void writeSymbol(hal::CharSymbol symbol) override
    {
        symbolWrites++;
        place(symbolMarker(symbol));
    }

    void setBacklightLevel(uint8_t level) override
    {
        backlightLevel = level;
        backlightCalls++;
    }

    /// The character each symbol paints as, so a test can read the row back as text.
    static char symbolMarker(hal::CharSymbol symbol)
    {
        switch (symbol)
        {
            case hal::CharSymbol::RightArrow:
                return '>';
            case hal::CharSymbol::LeftArrow:
                return '<';
            case hal::CharSymbol::UpArrow:
                return '^';
            case hal::CharSymbol::DownArrow:
                return '~';
            case hal::CharSymbol::Degrees:
                return '@';
            case hal::CharSymbol::Minutes:
                return '\'';
            case hal::CharSymbol::Tracking:
                return '&';
            case hal::CharSymbol::NoTracking:
            default:
                return '`';
        }
    }

    /// One row as text, trimmed of the padding the adapter added.
    const char *row(uint8_t index) const
    {
        return grid[index];
    }

    uint8_t charWrites     = 0;
    uint8_t symbolWrites   = 0;
    uint8_t backlightCalls = 0;
    uint8_t beginCalls     = 0;
    uint8_t clearCalls     = 0;
    uint8_t beginColumns   = 0;
    uint8_t beginRows      = 0;
    uint8_t backlightLevel = 0;
    bool beginResult       = true;
    bool definable         = true;
    bool definedSymbols[8] = {};

  private:
    void place(char value)
    {
        charWrites++;
        if (cursorRow < kMaxRows && cursorColumn < kMaxColumns)
        {
            grid[cursorRow][cursorColumn] = value;
        }
        cursorColumn++;
    }

    char grid[kMaxRows][kMaxColumns + 1] = {};
    uint8_t cursorColumn                 = 0;
    uint8_t cursorRow                    = 0;
};

}  // namespace fakes
