#pragma once

#include <stdint.h>

#include "ports/IDisplay.hpp"

namespace adapters
{

/// The display a headless board gets: the UI keeps working, and every write is dropped.
///
/// A null object rather than an `#if DISPLAY_TYPE`: the screens ask for the geometry and paint
/// rows, and a board without a panel simply does not show them.
class NullDisplay final : public ports::IDisplay
{
  public:
    NullDisplay(uint8_t columns = 16, uint8_t rows = 2);

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
    uint8_t _columns;
    uint8_t _rows;
    uint8_t _brightness;
};

}  // namespace adapters
