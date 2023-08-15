#pragma once

#if DISPLAY_TYPE > 0
    #if SUPPORT_MANUAL_CONTROL == 1
        #include "libs/MappedDict/MappedDict.hpp"

bool setZeroPoint = true;

enum ctrlState_t
{
    HIGHLIGHT_MANUAL,
    HIGHLIGHT_SERIAL,
    HIGHLIGHT_RA_AUTO_HOME,
    HIGHLIGHT_DEC_AUTO_HOME,  //DEC
    MANUAL_CONTROL_MODE,
    MANUAL_CONTROL_CONFIRM_HOME,
    RUNNING_RA_HOMING_MODE,
    RUNNING_DEC_HOMING_MODE,  //DEC
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
void processManualSlew(lcdButton_t key)
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
                //DEC-
                else if (key == btnDOWN)
                {
        #if USE_HALL_SENSOR_DEC_AUTOHOME == 1
                    ctrlState = HIGHLIGHT_DEC_AUTO_HOME;
        #else
                    ctrlState = HIGHLIGHT_SERIAL;
        #endif
                }
                //-DEC
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
                    ctrlState = RUNNING_RA_HOMING_MODE;
                    lcdMenu.printMenu("RA Homing...");
                    mount.stopSlewing(ALL_DIRECTIONS);
                    mount.findHomeByHallSensor(StepperAxis::RA_STEPS, -1, 2);  // Search 2hrs by default
                }
                else if (key == btnUP)
                {
                    ctrlState = HIGHLIGHT_MANUAL;
                }
                //DEC-
                else if (key == btnDOWN)
                {
                    ctrlState = HIGHLIGHT_DEC_AUTO_HOME;
                }
                //-DEC
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
                    ctrlState = HIGHLIGHT_RA_AUTO_HOME;
                }
            }
            break;
        #endif

                //DEC-
        #if USE_HALL_SENSOR_DEC_AUTOHOME == 1

        case HIGHLIGHT_DEC_AUTO_HOME:
            if (lcdButtons.keyChanged(&key))
            {
                waitForRelease = true;
                if (key == btnSELECT)
                {
                    ctrlState = RUNNING_DEC_HOMING_MODE;
                    lcdMenu.printMenu("DEC Homing...");
                    mount.stopSlewing(ALL_DIRECTIONS);
                    mount.findHomeByHallSensor(StepperAxis::DEC_STEPS, -1, 2);  // Search 2hrs by default
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

        case RUNNING_DEC_HOMING_MODE:
            {
                if (!mount.isFindingHome())
                {
                    ctrlState = HIGHLIGHT_DEC_AUTO_HOME;
                }
            }
            break;
        #endif
            //-DEC
        case HIGHLIGHT_SERIAL:
            if (lcdButtons.keyChanged(&key))
            {
                waitForRelease = true;
                if (key == btnSELECT)
                {
                    inSerialControl = !inSerialControl;
                }
                else if ((key == btnDOWN))
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
                //DEC-
                else if (key == btnUP)
                {
        #if USE_HALL_SENSOR_DEC_AUTOHOME == 1
                    ctrlState = HIGHLIGHT_DEC_AUTO_HOME;
        #else
                    ctrlState = HIGHLIGHT_MANUAL;
        #endif
                }
                //-DEC
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
                        LOG(DEBUG_GENERAL, "[CTRL]: Calling setHome(true)!");
                        mount.setHome(true);
                        LOG(DEBUG_GENERAL,
                            "[CTRL]: setHome(true) returned: RA Current %s, Target: %f",
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

                    ctrlState = HIGHLIGHT_MANUAL;
                    mount.stopSlewing(TRACKING);
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
            processManualSlew(key);  // Do the slewing

            if (key == btnSELECT)
            {
                    // User wants to set the current position as home
        #if SUPPORT_GUIDED_STARTUP == 1
                if (startupState == StartupWaitForPoleCompletion)
                {
                    startupState = StartupPoleConfirmed;
                    ctrlState    = HIGHLIGHT_MANUAL;
                    mount.stopSlewing(TRACKING);
                    waitForRelease = true;
                    inStartup      = true;
                }
                else
        #endif
                {
                    okToUpdateMenu = false;
                    lcdMenu.setCursor(0, 0);
                    lcdMenu.printMenu("Set home pos?");
                    ctrlState = MANUAL_CONTROL_CONFIRM_HOME;
                    mount.stopSlewing(TRACKING);
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
        case HIGHLIGHT_RA_AUTO_HOME:
            lcdMenu.printMenu(">Run RA Homing");
            break;
            //DEC-
        case HIGHLIGHT_DEC_AUTO_HOME:
            lcdMenu.printMenu(">Run DEC Homing");
            break;
            //-DEC
        case HIGHLIGHT_SERIAL:
            lcdMenu.printMenu(">Serial display");
            break;
        case MANUAL_CONTROL_CONFIRM_HOME:
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
