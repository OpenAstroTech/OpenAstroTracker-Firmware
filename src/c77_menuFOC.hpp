#pragma once

#if (DISPLAY_TYPE > 0) && (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)

// HIGHLIGHT states allow you to pick one of the sub functions.
enum FocusMenuItem
{
    HIGHLIGHT_FOCUS_FIRST      = 1,
    HIGHLIGHT_FOCUS_ADJUSTMENT = 1,

    HIGHLIGHT_FOCUS_LAST = HIGHLIGHT_FOCUS_ADJUSTMENT,

    FOCUS_ADJUSTMENT,
};

FocusMenuItem focState = HIGHLIGHT_FOCUS_FIRST;
byte rateIndex         = 3;

bool processFocuserKeys()
{
    lcdButton_t key;
    bool waitForRelease    = false;
    bool checkForKeyChange = true;

    lcdButton_t currentButtonState = lcdButtons.currentState();

    if (focState == FOCUS_ADJUSTMENT)
    {
        if (currentButtonState == btnUP)
        {
            if (!mount.isRunningFocus())
            {
                mount.focusContinuousMove(FOCUS_BACKWARD);
            }
        }
        else if (currentButtonState == btnDOWN)
        {
            if (!mount.isRunningFocus())
            {
                mount.focusContinuousMove(FOCUS_FORWARD);
            }
        }
    }

    if (currentButtonState == btnNONE)
    {
        if (mount.isRunningFocus())
        {
            mount.focusStop();
        }
    }

    if (checkForKeyChange && lcdButtons.keyChanged(&key))
    {
        waitForRelease = true;

        switch (focState)
        {
            case HIGHLIGHT_FOCUS_ADJUSTMENT:
                if (key == btnSELECT)
                {
                    focState = FOCUS_ADJUSTMENT;
                }
                if (key == btnRIGHT)
                {
                    lcdMenu.setNextActive();
                }

                break;

            case FOCUS_ADJUSTMENT:
                {
                    // UP and DOWN are handled above
                    if (key == btnSELECT)
                    {
                        focState = HIGHLIGHT_FOCUS_ADJUSTMENT;
                    }
                    else if (key == btnRIGHT)
                    {
                        rateIndex = adjustClamp(rateIndex, 1, 0, 3);
                        mount.focusSetSpeedByRate(rateIndex + 1);
                    }
                    else if (key == btnLEFT)
                    {
                        rateIndex = adjustClamp(rateIndex, -1, 0, 3);
                        mount.focusSetSpeedByRate(rateIndex + 1);
                    }
                }
                break;
        }
    }
    return waitForRelease;
}

void printFocusSubmenu()
{
    char scratchBuffer[20];
    if (focState == HIGHLIGHT_FOCUS_ADJUSTMENT)
    {
        lcdMenu.printMenu(">Focus Adjust");
    }
    else if (focState == FOCUS_ADJUSTMENT)
    {
        strcpy(scratchBuffer, "Rate:  1 2 3 4 *");
        scratchBuffer[6 + rateIndex * 2] = '>';
        scratchBuffer[8 + rateIndex * 2] = '<';
        if (!mount.isRunningFocus())
        {
            scratchBuffer[15] = '-';
        }
        else
        {
            scratchBuffer[15] = mount.getFocusSpeed() < 0 ? '~' : '^';
        }

        lcdMenu.printMenu(scratchBuffer);
    }
}

#endif
