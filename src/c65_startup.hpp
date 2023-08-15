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
    StartupAskIfDECHomingShouldRun,  //DEC
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
startupState_t startupState = StartupAskIfIsInHomePosition;
        #endif

    //DEC-
        #if USE_HALL_SENSOR_DEC_AUTOHOME == 1
startupState_t startupState = StartupAskIfDECHomingShouldRun;
        #else
startupState_t startupState = StartupAskIfIsInHomePosition;
        #endif
//-DEC

int isInHomePosition = NO;

void startupIsCompleted()
{
    LOG(DEBUG_INFO, "[STARTUP]: Completed!");

    startupState   = StartupCompleted;
    inStartup      = false;
    okToUpdateMenu = true;

    ////    mount.startSlewing(TRACKING);

    // Start on the RA menu
    lcdMenu.setActive(RA_Menu);
    lcdMenu.updateDisplay();
}

bool processStartupKeys()
{
    lcdButton_t key;
    bool waitForRelease = false;
    switch (startupState)
    {
        case StartupAskIfRAHomingShouldRun:   // RA
        case StartupAskIfDECHomingShouldRun:  // DEC
        case StartupAskIfIsInHomePosition:
            {
                if (lcdButtons.keyChanged(&key))
                {
                    waitForRelease = true;
                    if (key == btnLEFT)
                    {
                        isInHomePosition = adjustWrap(isInHomePosition, 1, YES, CANCEL);
                    }
                    else if (key == btnSELECT)
                    {
                        if (isInHomePosition == YES)
                        {
        #if USE_GYRO_LEVEL == 1
                            startupState = StartupSetRoll;
                            LOG(DEBUG_INFO, "[STARTUP]: State is set roll!");
        #else
                            startupState = StartupSetHATime;
        #endif
                        }
                        else if (isInHomePosition == NO)
                        {
                            startupState   = StartupWaitForPoleCompletion;
                            inStartup      = false;
                            okToUpdateMenu = false;
                            lcdMenu.setCursor(0, 0);
                            lcdMenu.printMenu("Home with ^~<>");
                            lcdMenu.setActive(Control_Menu);

                            // Skip the 'Manual control' prompt
                            setControlMode(true);
                        }
                        else if (isInHomePosition == CANCEL)
                        {
                            startupIsCompleted();
                        }
                    }
                }
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
                mount.setHome(true);
                DayTime ha(mount.HA());
                mount.setHA(ha);
                mount.targetRA() = mount.currentRA();
                startupIsCompleted();
            }
            break;

        case StartupPoleConfirmed:
            {
                isInHomePosition = YES;

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
        case StartupAskIfRAHomingShouldRun:   // RA
        case StartupAskIfDECHomingShouldRun:  // DEC

        case StartupAskIfIsInHomePosition:
            {
                //              0123456789012345
                String choices(" Yes  No  Cancl ");
                if (isInHomePosition == YES)
                {
                    choices.setCharAt(0, '>');
                    choices.setCharAt(4, '<');
                }

                if (isInHomePosition == NO)
                {
                    choices.setCharAt(5, '>');
                    choices.setCharAt(8, '<');
                }

                if (isInHomePosition == CANCEL)
                {
                    choices.setCharAt(9, '>');
                    choices.setCharAt(15, '<');
                }

                lcdMenu.setCursor(0, 0);
                lcdMenu.printMenu("Home position?");
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
