#pragma once

#include "../Configuration.hpp"
#include "core/menu/MenuModel.hpp"
#include "hal/WarningGuard.hpp"
#include "ports/IDisplay.hpp"

PUSH_NO_WARNINGS
#include <Arduino.h>
POP_NO_WARNINGS

// The menu screens' view of the menu: the item table, the selection, and the rows they paint.
//
// The panel itself is behind `ports::IDisplay`, so nothing here names a display library. What is
// left is the transitional shim the screens still call; it disappears with them (PLAN.md 8.8).
class LcdMenu
{
  public:
    LcdMenu(ports::IDisplay *display, core::MenuEntry *storage, byte capacity);

    /// Brings the panel up and applies the persisted backlight level.
    void startup();

    // Add a new menu item to the list (order matters)
    void addItem(const char *disp, byte id);

    // Get the currently active item ID
    byte getActive();

    // Set the active menu item
    void setActive(byte id);

    // The row and column the next printMenu() paints from
    void setCursor(byte col, byte row);

    // Set and get the brightness of the backlight
    void setBacklightBrightness(int level, bool persist = true);
    int getBacklightBrightness() const;
    void getBacklightBrightnessRange(int *minPtr, int *maxPtr) const;

    void clear();

    // Go to the next menu item from currently active one
    void setNextActive();

    // Repaint the menu row for the current selection
    void updateDisplay();

    // Print a line at the cursor, substituting the special arrows and padding with spaces to the end
    void printMenu(String line);

    // Print a single character at a specific position
    void printAt(byte col, byte row, char ch);

  private:
    // A headless board carries no menu: the object is a shell, as it was before the refactor, so
    // nothing of the UI lands in an ATmega2560's RAM or flash.
#if DISPLAY_TYPE > 0
    ports::IDisplay *_display;
    core::MenuModel _model;
    byte _activeRow;
    byte _activeCol;
#endif
};
