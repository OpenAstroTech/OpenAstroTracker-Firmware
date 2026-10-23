#pragma once

#include "../Configuration.hpp"
#include "EPROMStore.hpp"

#if DISPLAY_TYPE > 0
    #if USE_GPS == 0

bool processHAKeys()
{
    lcdButton_t key;
    bool waitForRelease = false;
    if (lcdButtons.currentState() == btnUP)
    {
        DayTime ha(ui.read->hourAngle());
        if (HAselect == 0)
            ha.addHours(1);
        if (HAselect == 1)
            ha.addMinutes(1);
        ui.siteTime->setHA(ha);

        // slow down key repetitions
        ui.delay(200);
    }
    else if (lcdButtons.currentState() == btnDOWN)
    {
        DayTime ha(ui.read->hourAngle());
        if (HAselect == 0)
            ha.addHours(-1);
        if (HAselect == 1)
            ha.addMinutes(-1);
        ui.siteTime->setHA(ha);

        // slow down key repetitions
        ui.delay(200);
    }
    else if (lcdButtons.keyChanged(&key))
    {
        waitForRelease = true;
        switch (key)
        {
            case btnLEFT:
                {
                    HAselect = adjustWrap(HAselect, 1, 0, 1);
                }
                break;

            case btnSELECT:
                {
                    settingsStore.storeHATime(ui.read->hourAngle());
                    lcdMenu.printMenu(F("Stored."));
                    ui.delay(500);

        #if SUPPORT_GUIDED_STARTUP == 1
                    if (startupState == StartupWaitForHACompletion)
                    {
                        startupState = StartupHAConfirmed;
                        inStartup    = true;
                    }
        #endif
                    ui.motion->startSlewing(TRACKING);
                }
                break;

            case btnRIGHT:
                {
        #if SUPPORT_GUIDED_STARTUP == 1
                    if (startupState != StartupWaitForHACompletion)
        #endif
                    {
                        lcdMenu.setNextActive();
                    }
                }
                break;

            default:
                break;
        }
    }

    return waitForRelease;
}

void printHASubmenu()
{
    char scratchBuffer[20];
    OAT_SNPRINTF(scratchBuffer, sizeof(scratchBuffer), " %02dh %02dm", ui.read->hourAngle().getHours(), ui.read->hourAngle().getMinutes());
    scratchBuffer[HAselect * 4] = '>';
    lcdMenu.printMenu(scratchBuffer);
}

    #endif
#endif
