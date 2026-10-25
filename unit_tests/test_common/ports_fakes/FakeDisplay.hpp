#pragma once

#include <stdint.h>
#include <string.h>

#include "ports/IDisplay.hpp"

namespace fakes
{

/// A `ports::IDisplay` that records the rows a screen writes.
class FakeDisplay final : public ports::IDisplay
{
  public:
    static constexpr uint8_t kMaxColumns = 24;
    static constexpr uint8_t kMaxRows    = 2;

    bool begin() override
    {
        return true;
    }

    uint8_t columns() const override
    {
        return columnCount;
    }

    uint8_t rows() const override
    {
        return kMaxRows;
    }

    void clear() override
    {
        clearCalls++;
    }

    void writeLine(uint8_t row, const char *text) override
    {
        lineWrites++;
        if (row >= kMaxRows)
        {
            return;
        }
        uint8_t i = 0;
        for (; text != nullptr && text[i] != '\0' && i < columnCount; i++)
        {
            _rows[row][i] = text[i];
        }
        for (; i < columnCount; i++)
        {
            _rows[row][i] = ' ';
        }
        _rows[row][columnCount] = '\0';
    }

    void writeCell(uint8_t row, uint8_t column, char glyph) override
    {
        cellWrites++;
        if (row < kMaxRows && column < columnCount)
        {
            _rows[row][column] = glyph;
        }
    }

    void setBacklight(uint8_t level, bool persist) override
    {
        (void) persist;
        backlightLevel = level;
    }

    uint8_t backlight() const override
    {
        return backlightLevel;
    }

    void backlightRange(uint8_t *minLevel, uint8_t *maxLevel) const override
    {
        *minLevel = 0;
        *maxLevel = 255;
    }

    const char *row(uint8_t index) const
    {
        return (index < kMaxRows) ? _rows[index] : "";
    }

    uint8_t columnCount    = 16;
    uint8_t lineWrites     = 0;
    uint8_t cellWrites     = 0;
    uint8_t clearCalls     = 0;
    uint8_t backlightLevel = 0;

  private:
    char _rows[kMaxRows][kMaxColumns + 1] = {};
};

}  // namespace fakes
