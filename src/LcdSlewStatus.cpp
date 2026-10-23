#include "LcdSlewStatus.hpp"

#include "Configuration.hpp"
#include "hal/WarningGuard.hpp"

PUSH_NO_WARNINGS
#include <Arduino.h>
POP_NO_WARNINGS
#include "LcdMenu.hpp"
#include "app/UiPorts.hpp"
#include "inc/Globals.hpp"

#if DISPLAY_TYPE > 0

namespace
{
char scratchBuffer[40];
unsigned long lastUpdate = 0;

/// The old code took five branches on the two planned-move totals; they are read once here so the
/// branches below read the way they did in `Mount`.
bool hasRaMove(app::UiPorts &ui)
{
    return fabsf(ui.read->plannedRaSteps()) > 0.001f;
}

bool hasDecMove(app::UiPorts &ui)
{
    return fabsf(ui.read->plannedDecSteps()) > 0.001f;
}

}  // namespace
#endif

void displayLcdSlewStatus(app::UiPorts &ui, LcdMenu &menu)
{
#if DISPLAY_TYPE > 0

    // The percentages are the ones the mount already computes for the progress bar; asking twice
    // would be two more axis reads for the same answer.
    int raPercent  = 100;
    int decPercent = 100;
    ui.read->getStepperProgress(raPercent, decPercent);

    if (hasRaMove(ui) && hasDecMove(ui))
    {
        // Both axes moving to target
        OAT_SNPRINTF(scratchBuffer, sizeof(scratchBuffer), "R %s %d%%", ui.read->raText(LCD_STRING | CURRENT_STRING), raPercent);
        menu.setCursor(0, 0);
        menu.printMenu(String(scratchBuffer));
        OAT_SNPRINTF(scratchBuffer, sizeof(scratchBuffer), "D%s %d%%", ui.read->decText(LCD_STRING | CURRENT_STRING), decPercent);
        menu.setCursor(0, 1);
        menu.printMenu(String(scratchBuffer));
        return;
    }

    if (hasDecMove(ui))
    {
        // Only DEC moving to target
        OAT_SNPRINTF(scratchBuffer, sizeof(scratchBuffer), "D%s %d%%", ui.read->decText(LCD_STRING | CURRENT_STRING), decPercent);
        menu.setCursor(0, 1);
        menu.printMenu(String(scratchBuffer));
    }
    else if (hasRaMove(ui))
    {
        // Only RA moving to target
        OAT_SNPRINTF(scratchBuffer, sizeof(scratchBuffer), "R %s %d%%", ui.read->raText(LCD_STRING | CURRENT_STRING), raPercent);
        menu.setCursor(0, inSerialControl ? 0 : 1);
        menu.printMenu(String(scratchBuffer));
    }
    else
    {
        // Nothing moving
    #if SUPPORT_SERIAL_CONTROL == 1
        if (inSerialControl)
        {
            OAT_SNPRINTF(scratchBuffer, sizeof(scratchBuffer), " RA: %s", ui.read->raText(LCD_STRING | CURRENT_STRING));
            menu.setCursor(0, 0);
            menu.printMenu(scratchBuffer);
            OAT_SNPRINTF(scratchBuffer, sizeof(scratchBuffer), "DEC: %s", ui.read->decText(LCD_STRING | CURRENT_STRING));
            menu.setCursor(0, 1);
            menu.printMenu(scratchBuffer);
        }
        else
        {
            OAT_SNPRINTF(scratchBuffer,
                         sizeof(scratchBuffer),
                         "R%s D%s",
                         ui.read->raText(COMPACT_STRING | CURRENT_STRING),
                         ui.read->decText(COMPACT_STRING | CURRENT_STRING));
            menu.setCursor(0, 1);
            menu.printMenu(scratchBuffer);
        }
    #else
        OAT_SNPRINTF(scratchBuffer,
                     sizeof(scratchBuffer),
                     "R%s D%s",
                     ui.read->raText(COMPACT_STRING | CURRENT_STRING),
                     ui.read->decText(COMPACT_STRING | CURRENT_STRING));
        menu.setCursor(0, 1);
        menu.printMenu(scratchBuffer);
    #endif
    }

#endif
}

void displayLcdSlewStatusThrottled(app::UiPorts &ui, LcdMenu &menu)
{
#if DISPLAY_TYPE > 0
    const unsigned long now = millis();
    if ((now - lastUpdate) > DISPLAY_UPDATE_TIME)
    {
        displayLcdSlewStatus(ui, menu);
        lastUpdate = millis();
    }
#endif
}
