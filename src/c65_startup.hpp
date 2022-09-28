#pragma once
#include "../Configuration.hpp"

#include "Sidereal.hpp"

#if USE_GYRO_LEVEL == 1
    #include "Gyro.hpp"
#endif

#if DISPLAY_TYPE > 0
    #if SUPPORT_GUIDED_STARTUP == 1

//////////////////////////////////////////////////////////////
// This file contains the Starup 'wizard' that guides you through initial setup

void setControlMode(bool);  // In CTRL menu

enum startupState_t
{
    StartupAskIfRAHomingShouldRun,
    StartupWaitForRAHomingCompletion,
    StartupAskIfDECOffsetHomingShouldRun,
    StartupWaitForDECOffsetHomingCompletion,
    StartupAskIfIsInHomePosition,
    StartupSetRoll,
    StartupWaitForRollCompletion,
    StartupRollConfirmed,
    StartupSetHATime,
    StartupWaitForHACompletion,
    StartupHAConfirmed,
    StartupWaitForPoleCompletion,
    StartupPoleConfirmed,
    StartupCompleted,
};

        #define YES    1
        #define NO     2
        #define CANCEL 3

        #if USE_HALL_SENSOR_RA_AUTOHOME == 1
startupState_t startupState = StartupAskIfRAHomingShouldRun;
        #else
startupState_t startupState = StartupAskIfDECOffsetHomingShouldRun;
        #endif

int answerPosition = YES;

void startupIsCompleted()
{
    startupState   = StartupCompleted;
    inStartup      = false;
    okToUpdateMenu = true;

        #if TRACK_ON_BOOT == 1
    // Start tracking.
    LOG(DEBUG_ANY, "[STARTUP]: Start Tracking.");
    mount.startSlewing(TRACKING);
        #endif

    // Start on the RA menu
    lcdMenu.setActive(RA_Menu);
    lcdMenu.updateDisplay();
    LOG(DEBUG_INFO, "[STARTUP]: Completed!");
}

bool processStartupKeys()
{
    lcdButton_t key;
    bool waitForRelease = false;
    switch (startupState)
    {
        case StartupAskIfDECOffsetHomingShouldRun:
            if (mount.getDecParkingOffset() == 0)
            {
                startupState   = StartupAskIfIsInHomePosition;
                answerPosition = NO;
            }
        // Fallthrough
        case StartupAskIfRAHomingShouldRun:
        case StartupAskIfIsInHomePosition:
            {
                if (lcdButtons.keyChanged(&key))
                {
                    waitForRelease = true;
                    if (key == btnLEFT)
                    {
                        answerPosition = adjustWrap(answerPosition, 1, YES, CANCEL);
                    }
                    else if (key == btnSELECT)
                    {
                        if (answerPosition == YES)
                        {
                            if (startupState == StartupAskIfRAHomingShouldRun)
                            {
                                LOG(DEBUG_INFO, "[STARTUP]: Requested RA auto-home!");
                                mount.stopSlewing(ALL_DIRECTIONS);
                                mount.findRAHomeByHallSensor(-1, 2);  // Search 2hrs by default
                                startupState = StartupWaitForRAHomingCompletion;
                                break;
                            }
                            else if (startupState == StartupAskIfDECOffsetHomingShouldRun)
                            {
                                LOG(DEBUG_INFO, "[STARTUP]: Requested DEC Offset homing!");
                                mount.stopSlewing(ALL_DIRECTIONS);
                                long offset = mount.getDecParkingOffset();
                                mount.moveStepperBy(DEC_STEPS, offset);
                                LOG(DEBUG_INFO, "[STARTUP]: Moving DEC to %l", offset);
                                startupState = StartupWaitForDECOffsetHomingCompletion;
                                break;
                            }
        #if USE_GYRO_LEVEL == 1
                            startupState = StartupSetRoll;
                            LOG(DEBUG_INFO, "[STARTUP]: State set to roll!");
        #else
                            startupState = StartupSetHATime;
                            LOG(DEBUG_INFO, "[STARTUP]: State set to HA!");
        #endif
                        }
                        else if (answerPosition == NO)
                        {
                            if (startupState == StartupAskIfRAHomingShouldRun)
                            {
                                startupState = StartupAskIfDECOffsetHomingShouldRun;
                                break;
                            }
                            else if (startupState == StartupAskIfDECOffsetHomingShouldRun)
                            {
                                startupState = StartupAskIfIsInHomePosition;
                                break;
                            }

                            startupState   = StartupWaitForPoleCompletion;
                            inStartup      = false;
                            okToUpdateMenu = false;
                            lcdMenu.setCursor(0, 0);
                            lcdMenu.printMenu("Home with ^~<>");
                            lcdMenu.setActive(Control_Menu);

                            // Skip the 'Manual control' prompt
                            setControlMode(true);
                        }
                        else if (answerPosition == CANCEL)
                        {
                            startupIsCompleted();
                        }
                    }
                }
            }
            break;

        case StartupWaitForRAHomingCompletion:
            {
                if (!mount.isFindingHome())
                {
                    LOG(DEBUG_MOUNT, "[STARTUP]: RA Auto-Homing complete");
                    if (mount.getDecParkingOffset() != 0)
                    {
                        LOG(DEBUG_INFO, "[STARTUP]: State set to ask for DEC Homing!");
                        startupState = StartupAskIfDECOffsetHomingShouldRun;
                        break;
                    }
        #if USE_GYRO_LEVEL == 1
                    startupState = StartupSetRoll;
                    LOG(DEBUG_INFO, "[STARTUP]: State set to roll!");
        #else
                    startupState = StartupSetHATime;
                    LOG(DEBUG_INFO, "[STARTUP]: State set to HA!");
        #endif
                }
            }
            break;

        case StartupWaitForDECOffsetHomingCompletion:
            {
                if (!mount.isSlewingRAorDEC())
                {
                    LOG(DEBUG_MOUNT, "[STARTUP]: DEC Offset-Homing complete");
                    mount.setHome(false);
                }
        #if USE_GYRO_LEVEL == 1
                startupState = StartupSetRoll;
                LOG(DEBUG_INFO, "[STARTUP]: State set to roll!");
        #else
                startupState = StartupSetHATime;
                LOG(DEBUG_INFO, "[STARTUP]: State set to HA!");
        #endif
            }
            break;

        #if USE_GYRO_LEVEL == 1
        case StartupSetRoll:
            {
                inStartup = false;
                LOG(DEBUG_INFO, "[STARTUP]: Switching to CAL menu!");

                lcdMenu.setCursor(0, 0);
                lcdMenu.printMenu("Level front");
                lcdMenu.setActive(Calibration_Menu);

                startupState = StartupWaitForRollCompletion;
            }
            break;

        case StartupRollConfirmed:
            {
                LOG(DEBUG_INFO, "[STARTUP]: Roll confirmed!");
                startupState = StartupSetHATime;
            }
            break;
        #endif

        case StartupSetHATime:
            {
                inStartup = false;
                LOG(DEBUG_INFO, "[STARTUP]: Switching to HA menu!");

        #if USE_GPS == 0
                // Jump to the HA menu
                okToUpdateMenu = false;
                lcdMenu.setCursor(0, 0);
                lcdMenu.printMenu("Set current HA");
                lcdMenu.setActive(HA_Menu);
                startupState = StartupWaitForHACompletion;
        #else
                lcdMenu.setCursor(0, 0);
                lcdMenu.printMenu("Finding GPS...");
                lcdMenu.setActive(HA_Menu);
                startupState = StartupWaitForHACompletion;
        #endif
            }
            break;

        case StartupHAConfirmed:
            {
                LOG(DEBUG_INFO, "[STARTUP]: HA is confirmed!");
                mount.setHome(true);
                DayTime ha(mount.HA());
                mount.setHA(ha);
                mount.targetRA() = mount.currentRA();
                startupIsCompleted();
            }
            break;

        case StartupPoleConfirmed:
            {
                answerPosition = YES;

                // Ask again to confirm
                startupState = StartupAskIfIsInHomePosition;
            }
            break;

        default:
            break;
    }

    return waitForRelease;
}

void printStartupMenu()
{
    switch (startupState)
    {
        case StartupAskIfDECOffsetHomingShouldRun:
        case StartupAskIfRAHomingShouldRun:
        case StartupAskIfIsInHomePosition:
            {
                //              0123456789012345
                String choices(" Yes  No  Cancl ");
                if (answerPosition == YES)
                {
                    choices.setCharAt(0, '>');
                    choices.setCharAt(4, '<');
                }

                if (answerPosition == NO)
                {
                    choices.setCharAt(5, '>');
                    choices.setCharAt(8, '<');
                }

                if (answerPosition == CANCEL)
                {
                    choices.setCharAt(9, '>');
                    choices.setCharAt(15, '<');
                }

                lcdMenu.setCursor(0, 0);
                switch (startupState)
                {
                    case StartupAskIfDECOffsetHomingShouldRun:
                        lcdMenu.printMenu("Offs-home DEC?");
                        break;
                    case StartupAskIfRAHomingShouldRun:
                        lcdMenu.printMenu("Auto-home RA?");
                        break;
                    case StartupAskIfIsInHomePosition:
                        lcdMenu.printMenu("Home position?");
                        break;
                    default:
                        break;
                }
                lcdMenu.setCursor(0, 1);
                lcdMenu.printMenu(choices);
            }
            break;

        default:
            break;
    }
}
    #endif
#endif
