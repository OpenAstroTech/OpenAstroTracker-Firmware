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
        DayTime ha(mount.HA());
        if (HAselect == 0)
            ha.addHours(1);
        if (HAselect == 1)
            ha.addMinutes(1);
        mount.setHA(ha);

        // slow down key repetitions
        mount.delay(200);
    }
    else if (lcdButtons.currentState() == btnDOWN)
    {
        DayTime ha(mount.HA());
        if (HAselect == 0)
            ha.addHours(-1);
        if (HAselect == 1)
            ha.addMinutes(-1);
        mount.setHA(ha);

        // slow down key repetitions
        mount.delay(200);
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
                    EEPROMStore::storeHATime(mount.HA());
                    lcdMenu.printMenu("Stored.");
                    mount.delay(500);

        #if SUPPORT_GUIDED_STARTUP == 1
                    if (startupState == StartupWaitForHACompletion)
                    {
                        startupState = StartupHAConfirmed;
                        inStartup    = true;
                    }
        #endif
                    mount.startSlewing(TRACKING);
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
    sprintf(scratchBuffer, " %02dh %02dm", mount.HA().getHours(), mount.HA().getMinutes());
    scratchBuffer[HAselect * 4] = '>';
    lcdMenu.printMenu(scratchBuffer);
}

    #endif
#endif
