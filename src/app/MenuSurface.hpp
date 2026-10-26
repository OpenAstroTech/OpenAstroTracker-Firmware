#pragma once

#include <stdint.h>

#include "core/menu/MenuModel.hpp"
#include "ports/IDisplay.hpp"

#include "hal/WarningGuard.hpp"

PUSH_NO_WARNINGS
#include <Arduino.h>  // `String`: the legacy print call sites still hand one over
POP_NO_WARNINGS

namespace app
{

/// The menu as the loop and the composition root see it: the item table, the cursor the legacy
/// call sites still set, and the rows they paint.
///
/// This is what `LcdMenu` was, minus the object graph: the model and the display are the ones the
/// composition root builds, so the façade owns nothing and a board with no panel simply has no
/// model. The screens do not use it — they take `ports::IDisplay` directly — which is why it can
/// stay this small.
class MenuSurface
{
  public:
    /// Brings the panel up and empties the item table.
    void startup();

    /// Appends an item. A full table drops the item and logs it.
    void addItem(const char *display, uint8_t id);

    /// The active item's id, or 0 when there is no menu.
    uint8_t getActive() const;

    void setActive(uint8_t id);

    /// Moves to the next item, repaints the menu row and clears the screen's row.
    void setNextActive();

    /// Repaints the menu row and leaves the cursor on the screen's row.
    void updateDisplay();

    /// Paints a row from the cursor; a non-zero column writes cells and leaves the rest alone.
    void printMenu(const char *text);
    void printMenu(const String &text);

    void setCursor(uint8_t column, uint8_t row);

    /// Paints one character, which is how the tracking indicator is drawn.
    void printAt(uint8_t column, uint8_t row, char glyph);

    int getBacklightBrightness() const;
    void getBacklightBrightnessRange(int *minLevel, int *maxLevel) const;

  private:
    uint8_t _row = 0;
    uint8_t _col = 0;
};

}  // namespace app

/// The one instance; defined beside the object graph it reads.
extern app::MenuSurface menu;
