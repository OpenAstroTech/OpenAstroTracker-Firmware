#include "app/MenuSurface.hpp"

#include "Configuration.hpp"
#include "ports/ILogger.hpp"
#include "app/BoardWiring.hpp"
#include "core/menu/MenuLine.hpp"

PUSH_NO_WARNINGS
#include <Arduino.h>
POP_NO_WARNINGS

namespace app
{
void MenuSurface::startup()
{
    LOG(DEBUG_INFO, "[LCD]: menu startup");

    _row = 0;
    _col = 0;

    if (menuModelPort != nullptr)
    {
        menuModelPort->clear();
    }
    if (menuDisplayPort != nullptr)
    {
        menuDisplayPort->begin();
    }
}

void MenuSurface::showSplash()
{
    setCursor(0, 0);
#ifdef OAM
    printMenu(F(" OpenAstroMount"));
#elif defined(OAE)
    printMenu(F("OpenAstroExplorer"));
#else
    printMenu(F("OpenAstroTracker"));
#endif
    setCursor(5, 1);
    printMenu(VERSION);
    delay(1000);  // Pause on splash screen
}

bool MenuSurface::awaitFactoryResetRequest()
{
    if ((menuButtons == nullptr) || (menuButtons->pressed() != types::Button::Down))
    {
        return false;
    }

    const long checkStart = millis();
    setCursor(13, 1);
    printMenu(F("CLR"));
    LOG(DEBUG_INFO, "[SYSTEM]: DOWN pressed, waiting for button release!");
    while ((menuButtons != nullptr) && (menuButtons->pressed() != types::Button::None))
    {
        delay(10);
        // Wait 3s maximum for the button to be released.
        if (millis() - checkStart > 3000)
        {
            LOG(DEBUG_INFO, "[SYSTEM]: Timedout waiting for button release. LCD not installed?");
            return false;
        }
    }
    return true;
}

void MenuSurface::addItem(const char *display, uint8_t id)
{
    if ((menuModelPort != nullptr) && !menuModelPort->add(display, id))
    {
        LOG(DEBUG_INFO, "[LCD]: menu item table is full, dropping %s", display);
    }
}

uint8_t MenuSurface::getActive() const
{
    return (menuModelPort != nullptr) ? menuModelPort->activeId() : 0;
}

void MenuSurface::setActive(uint8_t id)
{
    if (menuModelPort != nullptr)
    {
        menuModelPort->setActive(id);
    }
}

void MenuSurface::setNextActive()
{
    if (menuModelPort != nullptr)
    {
        menuModelPort->next();
    }

    updateDisplay();

    // Clear the screen's row: the new screen may not print anything there.
    setCursor(0, 1);
    printMenu("");
}

void MenuSurface::updateDisplay()
{
    if ((menuDisplayPort == nullptr) || (menuModelPort == nullptr))
    {
        return;
    }

    char row[26];
    core::composeMenuRow(row, sizeof(row), *menuModelPort, menuDisplayPort->columns());
    menuDisplayPort->writeLine(0, row);
    setCursor(0, 1);
}

void MenuSurface::printMenu(const char *text)
{
    if (menuDisplayPort == nullptr)
    {
        return;
    }

    if (_col == 0)
    {
        menuDisplayPort->writeLine(_row, text);
        return;
    }

    for (uint8_t i = 0; (text[i] != '\0') && ((_col + i) < menuDisplayPort->columns()); i++)
    {
        menuDisplayPort->writeCell(_row, static_cast<uint8_t>(_col + i), text[i]);
    }
}

void MenuSurface::printMenu(const String &text)
{
    printMenu(text.c_str());
}

void MenuSurface::setCursor(uint8_t column, uint8_t row)
{
    _row = row;
    _col = column;
}

void MenuSurface::printAt(uint8_t column, uint8_t row, char glyph)
{
    if (menuDisplayPort != nullptr)
    {
        menuDisplayPort->writeCell(row, column, glyph);
    }
}

int MenuSurface::getBacklightBrightness() const
{
    return (menuDisplayPort != nullptr) ? menuDisplayPort->backlight() : 0;
}

void MenuSurface::getBacklightBrightnessRange(int *minLevel, int *maxLevel) const
{
    uint8_t minimum = 0;
    uint8_t maximum = 0;
    if (menuDisplayPort != nullptr)
    {
        menuDisplayPort->backlightRange(&minimum, &maximum);
    }
    if (minLevel != nullptr)
    {
        *minLevel = minimum;
    }
    if (maxLevel != nullptr)
    {
        *maxLevel = maximum;
    }
}

}  // namespace app

app::MenuSurface menu;
