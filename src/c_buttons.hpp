#pragma once

#include "LcdButtons.hpp"
#include "b_setup.hpp"
#include "c65_startup.hpp"
#include "c70_menuRA.hpp"
#include "c71_menuDEC.hpp"
#include "c722_menuPOI.hpp"
#include "c72_menuHA.hpp"
#include "c72_menuHA_GPS.hpp"
#include "c75_menuCTRL.hpp"
#include "c76_menuCAL.hpp"
#include "c77_menuFOC.hpp"
#include "c78_menuINFO.hpp"

#if SUPPORT_SERIAL_CONTROL == 1
    #include "f_serial.hpp"
#endif

#if DISPLAY_TYPE > 0
    #if LCD_BUTTON_TEST == 1
lcdButton_t lastKey = btnNONE;
    #endif

lcdButton_t lcd_key;
unsigned long lastTrackingStatusPrint = 0;

void loop()
{
    #if LCD_BUTTON_TEST == 1
    int adc_key_in;

    lcdMenu.setCursor(0, 0);
    lcdMenu.printMenu("Key Diagnostic");
    lcd_key      = lcdButtons.currentState();
    int key      = lcdButtons.currentKey();
    bool changed = lcdButtons.keyChanged(&lastKey);

    adc_key_in = lcdButtons.currentAnalogState();

    lcdMenu.setCursor(0, 1);
    char buf[128];
    sprintf(buf, "A:%4d %d ", adc_key_in, key);
    String state = String(buf);
    switch (lcd_key)
    {
        case btnNONE:
            state += "None";
            break;
        case btnSELECT:
            state += "Select";
            break;
        case btnLEFT:
            state += "Left";
            break;
        case btnRIGHT:
            state += "Right";
            break;
        case btnUP:
            state += "Up";
            break;
        case btnDOWN:
            state += "Down";
            break;
        default:
            state += "Invalid";
            break;
    }

    lcdMenu.printMenu(state);
    if (changed)
    {
        Serial.println(lastKey);
    }

    return;

    #endif

    // Give the mount a time slice to do its thing...
    mount.loop();

    // Update the LCD display
    unsigned long now = millis();
    if (!inSerialControl && okToUpdateMenu && !inStartup && !mount.isSlewingRAorDEC())
    {
        // Main menu display
        lcdMenu.updateDisplay();
    }

    // Tracking marker
    if ((mount.isBootComplete()) && (now - lastTrackingStatusPrint > 200))
    {
        lcdMenu.printAt(15, 0, mount.isSlewingTRK() ? '&' : '`');
        lastTrackingStatusPrint = now;
    }

    lcdMenu.setCursor(0, 1);

    #if SUPPORT_SERIAL_CONTROL == 1
    if (inSerialControl)
    {
        if (lcdButtons.keyChanged(&lcd_key))
        {
            if (lcd_key == btnSELECT)
            {
                quitSerialOnNextButtonRelease = true;
            }
            else if ((lcd_key == btnNONE) && quitSerialOnNextButtonRelease)
            {
                MeadeCommandProcessor::instance()->processCommand(":Qq#");
                quitSerialOnNextButtonRelease = false;
            }
        }
        serialLoop();
    }
    else
    #endif
    {
        bool waitForButtonRelease = false;

    // Handle the keys
    #if SUPPORT_GUIDED_STARTUP == 1
        if (inStartup)
        {
            waitForButtonRelease = processStartupKeys();
        }
        else
    #endif
        {
            switch (lcdMenu.getActive())
            {
                case RA_Menu:
                    waitForButtonRelease = processRAKeys();
                    break;
                case DEC_Menu:
                    waitForButtonRelease = processDECKeys();
                    break;
    #if SUPPORT_POINTS_OF_INTEREST == 1
                case POI_Menu:
                    waitForButtonRelease = processPOIKeys();
                    break;
    #else
                case Home_Menu:
                    waitForButtonRelease = processHomeKeys();
                    break;
    #endif

                case HA_Menu:
                    waitForButtonRelease = processHAKeys();
                    break;

    #if SUPPORT_CALIBRATION == 1
                case Calibration_Menu:
                    waitForButtonRelease = processCalibrationKeys();
                    break;
    #endif

    #if FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE
                case Focuser_Menu:
                    waitForButtonRelease = processFocuserKeys();
                    break;
    #endif

    #if SUPPORT_MANUAL_CONTROL == 1
                case Control_Menu:
                    waitForButtonRelease = processControlKeys();
                    break;
    #endif

    #if SUPPORT_INFO_DISPLAY == 1
                case Status_Menu:
                    waitForButtonRelease = processStatusKeys();
                    break;
    #endif
            }
        }

        if (waitForButtonRelease)
        {
            if (lcdButtons.currentState() != btnNONE)
            {
                do
                {
                    lcdButton_t waitKey;
                    if (lcdButtons.keyChanged(&waitKey))
                    {
                        if (waitKey == btnNONE)
                        {
                            break;
                        }
                    }

                    // Make sure tracker can still run while fiddling with menus....
                    mount.loop();
                } while (true);
            }
        }

        // Input handled, do output
        lcdMenu.setCursor(0, 1);

    #if SUPPORT_GUIDED_STARTUP == 1
        if (inStartup)
        {
            printStartupMenu();
        }
        else
    #endif
        {
            if (!inSerialControl)
            {
                // For some strange reason, a switch statement here causes a crash and reboot....
                int activeMenu = lcdMenu.getActive();
                if (activeMenu == RA_Menu)
                {
                    printRASubmenu();
                }
                else if (activeMenu == DEC_Menu)
                {
                    printDECSubmenu();
                }
    #if SUPPORT_POINTS_OF_INTEREST == 1
                else if (activeMenu == POI_Menu)
                {
                    printPOISubmenu();
                }
    #else
                else if (activeMenu == Home_Menu)
                {
                    printHomeSubmenu();
                }
    #endif
                else if (activeMenu == HA_Menu)
                {
                    printHASubmenu();
                }
    #if SUPPORT_MANUAL_CONTROL == 1
                else if (activeMenu == Control_Menu)
                {
                    printControlSubmenu();
                }
    #endif
    #if SUPPORT_CALIBRATION == 1
                else if (activeMenu == Calibration_Menu)
                {
                    printCalibrationSubmenu();
                }
    #endif

    #if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
                else if (activeMenu == Focuser_Menu)
                {
                    printFocusSubmenu();
                }
    #endif

    #if SUPPORT_INFO_DISPLAY == 1
                else if (activeMenu == Status_Menu)
                {
                    printStatusSubmenu();
                }
    #endif
            }
        }
    }
}

#else

// No display present.
void loop()
{
    serialLoop();
}

#endif
