#pragma once

#if DISPLAY_TYPE > 0
bool showTargetRA = true;
bool processRAKeys()
{
    lcdButton_t key;
    bool waitForRelease = false;
    if (lcdButtons.currentState() == btnUP)
    {
        if (RAselect == 0)
        {
            ui.read->targetRA().addHours(1);
            showTargetRA = true;
        }
        if (RAselect == 1)
        {
            ui.read->targetRA().addMinutes(1);
            showTargetRA = true;
        }
        if (RAselect == 2)
        {
            ui.read->targetRA().addSeconds(1);
            showTargetRA = true;
        }
        if (RAselect == 3)
        {
            showTargetRA   = !showTargetRA;
            waitForRelease = true;
        }

        // slow down key repetitions
        ui.delay(200);
    }
    else if (lcdButtons.currentState() == btnDOWN)
    {
        if (RAselect == 0)
        {
            ui.read->targetRA().addHours(-1);
            showTargetRA = true;
        }
        if (RAselect == 1)
        {
            ui.read->targetRA().addMinutes(-1);
            showTargetRA = true;
        }
        if (RAselect == 2)
        {
            ui.read->targetRA().addSeconds(-1);
            showTargetRA = true;
        }
        if (RAselect == 3)
        {
            showTargetRA   = !showTargetRA;
            waitForRelease = true;
        }

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
                    RAselect = adjustWrap(RAselect, 1, 0, 3);
                }
                break;

            case btnSELECT:
                {
                    if (ui.motion->isSlewingRAorDEC())
                    {
                        ui.motion->stopSlewing(ALL_DIRECTIONS);
                        ui.waitUntilStopped(ALL_DIRECTIONS);
                    }

                    ui.motion->startSlewingToTarget();
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

void printRASubmenu()
{
    if (ui.read->isSlewingIdle())
    {
        String ra = String(ui.read->raText(LCDMENU_STRING | (showTargetRA ? TARGET_STRING : CURRENT_STRING), RAselect)).substring(0, 12);
        ra += (RAselect == 3) ? ">" : " ";
        ra += showTargetRA ? "Ta" : "Cu";
        lcdMenu.printMenu(ra);
    }
}
#endif
