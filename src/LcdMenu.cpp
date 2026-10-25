#include "LcdMenu.hpp"

#include "../Configuration.hpp"
#include "Utility.hpp"
#include "core/menu/MenuLine.hpp"

#if DISPLAY_TYPE > 0

    LcdMenu::LcdMenu(ports::IDisplay *display, core::MenuEntry *storage, byte capacity)
        : _display(display), _model(storage, capacity), _activeRow(0), _activeCol(0)
    {
    }

    void LcdMenu::startup()
    {
        LOG(DEBUG_INFO, "[LCD]: LcdMenu startup");

        _model.clear();
        _activeRow = 0;
        _activeCol = 0;

        if (_display != nullptr)
        {
            _display->begin();
        }
    }

    // Add a new menu item to the list (order matters)
    void LcdMenu::addItem(const char *disp, byte id)
    {
        if (!_model.add(disp, id))
        {
            LOG(DEBUG_ERROR, "[LCD]: Menu item table is full, dropping %s", disp);
        }
    }

    // Get the currently active item ID
    byte LcdMenu::getActive()
    {
        return _model.activeId();
    }

    // Set the active menu item
    void LcdMenu::setActive(byte id)
    {
        _model.setActive(id);
    }

    void LcdMenu::setCursor(byte col, byte row)
    {
        _activeRow = row;
        _activeCol = col;
    }

    void LcdMenu::clear()
    {
        if (_display != nullptr)
        {
            _display->clear();
        }
    }

    void LcdMenu::setBacklightBrightness(int level, bool persist)
    {
        if (_display == nullptr)
        {
            return;
        }

        const uint8_t clamped = (level < 0) ? 0 : static_cast<uint8_t>(level);
        _display->setBacklight(clamped, persist);

        if (persist)
        {
            LOG(DEBUG_INFO, "[LCD]: Saving %d as brightness", _display->backlight());
        }
    }

    int LcdMenu::getBacklightBrightness() const
    {
        return (_display != nullptr) ? _display->backlight() : 0;
    }

    void LcdMenu::getBacklightBrightnessRange(int *minPtr, int *maxPtr) const
    {
        uint8_t minimum = 0;
        uint8_t maximum = 0;
        if (_display != nullptr)
        {
            _display->backlightRange(&minimum, &maximum);
        }
        if (minPtr != nullptr)
        {
            *minPtr = minimum;
        }
        if (maxPtr != nullptr)
        {
            *maxPtr = maximum;
        }
    }

    void LcdMenu::setNextActive()
    {
        _model.next();

        updateDisplay();

        // Clear the submenu line, in case the new menu does not print anything there.
        setCursor(0, 1);
        printMenu(String(""));
    }

    void LcdMenu::updateDisplay()
    {
        if (_display == nullptr)
        {
            return;
        }

        // Room for the longest supported row plus its terminator; the composer truncates to the panel.
        char bufMenu[26];
        core::composeMenuRow(bufMenu, sizeof(bufMenu), _model, _display->columns());

        setCursor(0, 0);
        printMenu(String(bufMenu));
        setCursor(0, 1);
    }

    void LcdMenu::printMenu(String line)
    {
        if (_display == nullptr)
        {
            return;
        }

        if (_activeCol == 0)
        {
            _display->writeLine(_activeRow, line.c_str());
            return;
        }

        for (unsigned int i = 0; i < line.length(); i++)
        {
            _display->writeCell(_activeRow, static_cast<uint8_t>(_activeCol + i), line[i]);
        }
    }

    void LcdMenu::printAt(byte col, byte row, char ch)
    {
        if (_display != nullptr)
        {
            _display->writeCell(row, col, ch);
        }
    }

#else  // Headless boards carry no menu at all.

LcdMenu::LcdMenu(ports::IDisplay *display, core::MenuEntry *storage, byte capacity)
{
    (void) display;
    (void) storage;
    (void) capacity;
}

void LcdMenu::startup()
{
}

void LcdMenu::addItem(const char *disp, byte id)
{
    (void) disp;
    (void) id;
}

byte LcdMenu::getActive()
{
    return 0;
}

void LcdMenu::setActive(byte id)
{
    (void) id;
}

void LcdMenu::setCursor(byte col, byte row)
{
    (void) col;
    (void) row;
}

void LcdMenu::clear()
{
}

void LcdMenu::setBacklightBrightness(int level, bool persist)
{
    (void) level;
    (void) persist;
}

int LcdMenu::getBacklightBrightness() const
{
    return 0;
}

void LcdMenu::getBacklightBrightnessRange(int *minPtr, int *maxPtr) const
{
    if (minPtr != nullptr)
    {
        *minPtr = 0;
    }
    if (maxPtr != nullptr)
    {
        *maxPtr = 0;
    }
}

void LcdMenu::setNextActive()
{
}

void LcdMenu::updateDisplay()
{
}

void LcdMenu::printMenu(String line)
{
    (void) line;
}

void LcdMenu::printAt(byte col, byte row, char ch)
{
    (void) col;
    (void) row;
    (void) ch;
}

#endif
