#pragma once

#if DISPLAY_TYPE > 0
    #if SUPPORT_MANUAL_CONTROL == 1
        #include "libs/MappedDict/MappedDict.hpp"

bool setZeroPoint          = true;
long raStepperPosAfterHome = 0L;
enum ctrlState_t
{
    HIGHLIGHT_MANUAL,
    HIGHLIGHT_SERIAL,
    HIGHLIGHT_RA_AUTO_HOME,
    MANUAL_CONTROL_MODE,
    MANUAL_CONTROL_CONFIRM_HOME,
    RUNNING_RA_HOMING_MODE,
    CONFIRM_RA_AUTO_HOME_POS,
    SLEW_TO_RA_HOME_POS,
};

ctrlState_t ctrlState = HIGHLIGHT_MANUAL;

void setControlMode(bool state)
{
    ctrlState = state ? MANUAL_CONTROL_MODE : HIGHLIGHT_MANUAL;
}

/**
 * @brief Handle commanding the mount slew direction when in manual control
 * @defails Meant to be called continuously with the current key pressed,
 * a keypress is only 'valid' if it is held down for at least 10 cycles.
 * @param[in] key The current key being pressed
 * @param[in] dir The direction the mount should slew in associated with the key
 * @return true if the mount was commanded, false otherwise
 */
bool controlManualSlew(lcdButton_t key, int dir)
{
    const int LOOPS_TO_CONFIRM_KEY = 10;
    /// Static counter that is reset whenever there is a key change
    static unsigned countDown = 0;
    /// Static storage for the key that is currently commanding the mount
    static lcdButton_t currentKeyPressed = btnINVALID;

    const bool keyConfirmed = (countDown == 0);
    bool isNewSlewDirection = false;
    if (keyConfirmed && currentKeyPressed != key)
    {
        // Store the current key press as it has been confirmed
        currentKeyPressed = key;
        mount.stopSlewing(ALL_DIRECTIONS);
        mount.waitUntilStopped(ALL_DIRECTIONS);
        if (dir != 0)
        {
            // Slew the mount in the desired direction
            mount.startSlewing(dir);
        }
        isNewSlewDirection = true;
    }
    else if (currentKeyPressed != key)
    {
        countDown = LOOPS_TO_CONFIRM_KEY;
    }

    if (countDown > 0)
    {
        // Always try to count down if possible
        countDown -= 1;
    }

    return isNewSlewDirection;
}

/**
 * Slew the mount in a direction depending on the input key
 * @param[in] key The current key being pressed
 */
void processManualSlew(lcdButton_t key, bool raOnly)
{
    MappedDict<lcdButton_t, int>::DictEntry_t lookupTable[] = {
        {btnNONE, 0},
        {btnSELECT, 0},
        {btnINVALID, 0},
        {btnUP, NORTH},
        {btnDOWN, SOUTH},
        {btnLEFT, WEST},
        {btnRIGHT, EAST},
    };

    auto directionLookup = MappedDict<lcdButton_t, int>(lookupTable, ARRAY_SIZE(lookupTable));
    int slewDirection;
    const bool directionInTable = directionLookup.tryGet(key, &slewDirection);
    if (!directionInTable)
    {
        LOG(DEBUG_MOUNT, "[SYSTEM]: Unknown LCD button value: %i", key);
        return;
    }

    if (raOnly && ((slewDirection == SOUTH) || (slewDirection == NORTH)))
    {
        slewDirection = 0;
    }

    // Slew the mount in the desired direction
    (void) controlManualSlew(key, slewDirection);
}

bool processControlKeys()
{
    lcdButton_t key;
    bool waitForRelease = false;

    // User must use SELECT to enter manual control.
    switch (ctrlState)
    {
        case HIGHLIGHT_MANUAL:
            if (lcdButtons.keyChanged(&key))
            {
                waitForRelease = true;
                if (key == btnSELECT)
                {
                    ctrlState = MANUAL_CONTROL_MODE;
                    mount.stopSlewing(ALL_DIRECTIONS);
                }
                else if (key == btnDOWN)
                {
        #if USE_HALL_SENSOR_RA_AUTOHOME == 1
                    ctrlState = HIGHLIGHT_RA_AUTO_HOME;
        #else
                    ctrlState = HIGHLIGHT_SERIAL;
        #endif
                }
                else if (key == btnUP)
                {
                    ctrlState = HIGHLIGHT_SERIAL;
                }
                else if (key == btnRIGHT)
                {
                    lcdMenu.setNextActive();
                }
            }
            break;

        #if USE_HALL_SENSOR_RA_AUTOHOME == 1

        case HIGHLIGHT_RA_AUTO_HOME:
            if (lcdButtons.keyChanged(&key))
            {
                waitForRelease = true;
                if (key == btnSELECT)
                {
                    okToUpdateMenu = false;
                    LOG(DEBUG_MOUNT, "[CTRLMENU]: Select pressed, running homing");
                    ctrlState = RUNNING_RA_HOMING_MODE;
                    lcdMenu.setCursor(0, 0);
                    lcdMenu.printMenu("RA Homing...");
                    mount.stopSlewing(ALL_DIRECTIONS);
                    mount.findRAHomeByHallSensor(-1, 2);  // Search 2hrs by default
                }
                else if (key == btnUP)
                {
                    ctrlState = HIGHLIGHT_MANUAL;
                }
                else if (key == btnDOWN)
                {
                    ctrlState = HIGHLIGHT_SERIAL;
                }
                else if (key == btnRIGHT)
                {
                    lcdMenu.setNextActive();
                }
            }
            break;

        case RUNNING_RA_HOMING_MODE:
            {
                if (!mount.isFindingHome())
                {
                    LOG(DEBUG_MOUNT, "[CTRLMENU]: Homing complete, asking confirmation");
                    ctrlState = CONFIRM_RA_AUTO_HOME_POS;
                    lcdMenu.setCursor(0, 0);
                    lcdMenu.printMenu("Is RA home?");
                }
            }
            break;

        case CONFIRM_RA_AUTO_HOME_POS:
            if (lcdButtons.keyChanged(&key))
            {
                waitForRelease = true;
                if (key == btnSELECT)
                {
                    if (!setZeroPoint)  // No selected
                    {
                        okToUpdateMenu = false;
                        lcdMenu.setCursor(0, 0);
                        lcdMenu.printMenu("Use <> to home");
                        lcdMenu.setCursor(0, 1);
                        lcdMenu.printMenu("SEL to confirm");
                        ctrlState             = SLEW_TO_RA_HOME_POS;
                        raStepperPosAfterHome = mount.getCurrentStepperPosition(WEST);
                        LOG(DEBUG_MOUNT,
                            "[CTRLMENU]: Homing confirmation negative. Slewing activated. Start RA stepper at %l",
                            raStepperPosAfterHome);
                        setZeroPoint = true;
                    }
                    else  // Yes selected
                    {
                        okToUpdateMenu = true;
                        LOG(DEBUG_MOUNT, "[CTRLMENU]: Homing confirmation positive. Set home.");
                        mount.setHome(true);
                        ctrlState = HIGHLIGHT_RA_AUTO_HOME;
                    }
                }
                else if (key == btnRIGHT || key == btnLEFT)
                {
                    setZeroPoint = !setZeroPoint;
                }
            }
            break;

        case SLEW_TO_RA_HOME_POS:
            {
                key = lcdButtons.currentState();
                processManualSlew(key, true);  // Do the slewing
                if (key == btnSELECT)
                {
                    waitForRelease = true;
                    okToUpdateMenu = true;

                    LOG(DEBUG_MOUNT,
                        "[CTRLMENU]: Manual slewing complete. Current RA stepper at %l. ",
                        mount.getCurrentStepperPosition(WEST));
                    long offset = raStepperPosAfterHome - mount.getCurrentStepperPosition(WEST);
                    LOG(DEBUG_MOUNT,
                        "[CTRLMENU]: Manually slewed by %l. EEPROM offset is %l",
                        offset,
                        mount.getHomingOffset(StepperAxis::RA_STEPS));
                    long newRAOffset = mount.getHomingOffset(StepperAxis::RA_STEPS) + offset;
                    LOG(DEBUG_MOUNT, "[CTRLMENU]: New RA offset is %l. Set home.", newRAOffset);
                    mount.setHomingOffset(StepperAxis::RA_STEPS, newRAOffset);
                    mount.setHome(true);
                    ctrlState = HIGHLIGHT_RA_AUTO_HOME;
                }
                else if ((key == btnUP) || (key == btnDOWN))
                {
                    okToUpdateMenu = true;
                    ctrlState      = CONFIRM_RA_AUTO_HOME_POS;
                }
            }
            break;

        #endif

        case HIGHLIGHT_SERIAL:
            if (lcdButtons.keyChanged(&key))
            {
                waitForRelease = true;
                if (key == btnSELECT)
                {
                    inSerialControl = !inSerialControl;
                }
                else if (key == btnDOWN)
                {
                    ctrlState = HIGHLIGHT_MANUAL;
                }
                else if (key == btnUP)
                {
        #if USE_HALL_SENSOR_RA_AUTOHOME == 1
                    ctrlState = HIGHLIGHT_RA_AUTO_HOME;
        #else
                    ctrlState = HIGHLIGHT_MANUAL;
        #endif
                }
                else if (key == btnRIGHT)
                {
                    inSerialControl = false;
                    lcdMenu.setNextActive();
                }
            }
            break;

        case MANUAL_CONTROL_CONFIRM_HOME:
            if (lcdButtons.keyChanged(&key))
            {
                waitForRelease = true;
                if (key == btnSELECT)
                {
                    if (setZeroPoint)
                    {
                        // Leaving Control Menu, so set stepper motor positions to zero.
                        LOG(DEBUG_GENERAL, "[CTRLMENU]: Calling setHome(true)!");
                        mount.setHome(true);
                        LOG(DEBUG_GENERAL,
                            "[CTRLMENU]: setHome(true) returned: RA Current %s, Target: %f",
                            mount.RAString(CURRENT_STRING | COMPACT_STRING).c_str(),
                            mount.RAString(TARGET_STRING | COMPACT_STRING).c_str());
                        mount.startSlewing(TRACKING);
                    }

        #if SUPPORT_GUIDED_STARTUP == 1
                    if (startupState == StartupWaitForPoleCompletion)
                    {
                        startupState = StartupPoleConfirmed;
                        inStartup    = true;
                    }
                    else
        #endif
                    {
                        lcdMenu.setNextActive();
                    }

                    ctrlState      = HIGHLIGHT_MANUAL;
                    okToUpdateMenu = true;
                    setZeroPoint   = true;
                }
                else if (key == btnLEFT)
                {
                    setZeroPoint = !setZeroPoint;
                }
            }
            break;

        case MANUAL_CONTROL_MODE:
            key = lcdButtons.currentState();
            processManualSlew(key, false);  // Do the slewing

            if (key == btnSELECT)
            {
                    // User wants to set the current position as home
        #if SUPPORT_GUIDED_STARTUP == 1
                if (startupState == StartupWaitForPoleCompletion)
                {
                    startupState   = StartupPoleConfirmed;
                    ctrlState      = HIGHLIGHT_MANUAL;
                    waitForRelease = true;
                    inStartup      = true;
                }
                else
        #endif
                {
                    okToUpdateMenu = false;
                    lcdMenu.setCursor(0, 0);
                    lcdMenu.printMenu("Set home pos?");
                    ctrlState      = MANUAL_CONTROL_CONFIRM_HOME;
                    waitForRelease = true;
                }
            }
            break;
    }

    return waitForRelease;
}

void printControlSubmenu()
{
    switch (ctrlState)
    {
        case HIGHLIGHT_MANUAL:
            lcdMenu.printMenu(">Manual slewing");
            break;
        case HIGHLIGHT_SERIAL:
            lcdMenu.printMenu(">Serial display");
            break;
        case HIGHLIGHT_RA_AUTO_HOME:
            lcdMenu.printMenu(">Run RA Homing");
            break;
        case MANUAL_CONTROL_CONFIRM_HOME:
        case CONFIRM_RA_AUTO_HOME_POS:
            {
                String disp = " Yes  No  ";
                disp.setCharAt(setZeroPoint ? 0 : 5, '>');
                disp.setCharAt(setZeroPoint ? 4 : 8, '<');
                lcdMenu.printMenu(disp);
            }
            break;
        default:
            mount.displayStepperPositionThrottled();
            break;
    }
}

    #endif
#endif
