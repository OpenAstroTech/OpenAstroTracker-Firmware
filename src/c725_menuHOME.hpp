#pragma once

#include "b_setup.hpp"

#if DISPLAY_TYPE > 0
byte subGoIndex = 0;

bool processHomeKeys()
{
    lcdButton_t key;
    bool waitForRelease = false;

    if (lcdButtons.keyChanged(&key))
    {
        waitForRelease = true;
        switch (key)
        {
            case btnSELECT:
                {
                    if (subGoIndex == 0)
                    {
                        mount.startSlewingToHome();
                    }
                    else if (mount.isSlewingTRK())
                    {
                        mount.park();
                    }
                    else
                    {
                        mount.startSlewing(TRACKING);
                    }
                }
                break;

            case btnUP:
            case btnDOWN:
            case btnLEFT:
                {
                    subGoIndex = 1 - subGoIndex;
                }
                break;

            case btnRIGHT:
                {
                    lcdMenu.setNextActive();
                }
                break;

            default:
                break;
        }
    }

    return waitForRelease;
}

void printHomeSubmenu()
{
    char scratchBuffer[16];
    if (mount.isSlewingTRK())
    {
        strcpy(scratchBuffer, " Home  Park");
    }
    else
    {
        strcpy(scratchBuffer, " Home  Unpark");
    }
    scratchBuffer[subGoIndex * 6] = '>';
    lcdMenu.printMenu(scratchBuffer);
}

#endif
