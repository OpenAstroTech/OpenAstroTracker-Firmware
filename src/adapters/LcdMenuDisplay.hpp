#pragma once

#include <stdint.h>

#include "hal/ICharLcd.hpp"
#include "ports/IDisplay.hpp"
#include "ports/IMountSettings.hpp"

namespace adapters
{

/// `ports::IDisplay` over any character LCD.
///
/// Every menu panel the firmware supports is a character grid, so the panel-family differences stay
/// in the HAL backend and this class owns what is common: the symbol substitution table, the
/// backlight policy (including the shields that can only switch it) and the "do not redraw an
/// unchanged row" rule that keeps an I2C frame off the bus when nothing moved.
class LcdMenuDisplay final : public ports::IDisplay
{
  public:
    /// The levels the panel accepts. A shield with the known-bad backlight circuit only has two.
    struct BacklightRange {
        uint8_t minimum;
        uint8_t maximum;
    };

    /// The widest row this adapter keeps a copy of, for the redraw comparison.
    static constexpr uint8_t kMaxColumns = 24;

    LcdMenuDisplay(hal::ICharLcd &panel, ports::IMountSettings *settings, uint8_t columns, uint8_t rows, BacklightRange range);

    bool begin() override;

    uint8_t columns() const override;

    uint8_t rows() const override;

    void clear() override;

    void writeLine(uint8_t row, const char *text) override;

    void writeCell(uint8_t row, uint8_t column, char glyph) override;

    void setBacklight(uint8_t level, bool persist) override;

    uint8_t backlight() const override;

    void backlightRange(uint8_t *minLevel, uint8_t *maxLevel) const override;

  private:
    void writeChar(char glyph);
    void applyBacklight(uint8_t level);

    hal::ICharLcd &_panel;
    ports::IMountSettings *_settings;
    uint8_t _columns;
    uint8_t _rows;
    BacklightRange _range;
    uint8_t _brightness;
    char _lastLine[2][kMaxColumns + 1];
    bool _lineValid[2];
};

}  // namespace adapters
