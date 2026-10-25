#pragma once

#include "LcdButtons.hpp"
#include "app/UiScreens.hpp"
#include "app/BoardWiring.hpp"
#include "c76_menuCAL.hpp"

#include "app/ui/screens/DecMenu.hpp"
#include "app/ui/screens/FocusMenu.hpp"
#include "app/ui/screens/HomeMenu.hpp"
#include "app/ui/SlewStatusView.hpp"
#include "app/ui/screens/InfoMenu.hpp"
#include "app/ui/screens/PoiMenu.hpp"
#include "app/ui/screens/RaMenu.hpp"

#if SUPPORT_SERIAL_CONTROL == 1
    #include "app/SerialPump.hpp"
#endif

#if DISPLAY_TYPE > 0
    #if LCD_BUTTON_TEST == 1
lcdButton_t lastKey = btnNONE;
    #endif

lcdButton_t lcd_key;
unsigned long lastTrackingStatusPrint = 0;

/// The converted screens own their own selection and field cursor.
app::HomeMenu homeMenu;
app::RaMenu raMenu;
app::DecMenu decMenu;

    #if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
app::FocusMenu focusMenu;
    #endif

/// True while the guided startup waits for the hour angle, which is the only state in which the HA
/// screen reports back to it.
bool startupAwaitingHa()
{
    #if SUPPORT_GUIDED_STARTUP == 1
    return app::startupWizard.step() == app::StartupWizard::Step::WaitForHaCompletion;
    #else
    return false;
    #endif
}

/// True while the guided startup waits for the pole alignment, which changes what SELECT means in
/// the control menu.
bool startupAwaitingPole()
{
    #if SUPPORT_GUIDED_STARTUP == 1
    return app::startupWizard.step() == app::StartupWizard::Step::WaitForPoleCompletion;
    #else
    return false;
    #endif
}

/// Applies what the startup wizard asked for. The wizard knows the sequence; the menu, the flags and
/// the tracking toggle are the loop's, so it says what it wants and this does it.
void applyStartupOutcome(const app::StartupOutcome &outcome)
{
    switch (outcome.action)
    {
        case app::StartupAction::HandOverToControlMenu:
            inStartup      = false;
            okToUpdateMenu = false;
            if (outcome.caption != nullptr && menuDisplayPort != nullptr)
            {
                menuDisplayPort->writeLine(0, outcome.caption);
            }
            lcdMenu.setActive(Control_Menu);
            app::controlMenu.startManual();
            break;

        case app::StartupAction::HandOverToCalibration:
            inStartup = false;
            if (outcome.caption != nullptr && menuDisplayPort != nullptr)
            {
                menuDisplayPort->writeLine(0, outcome.caption);
            }
            lcdMenu.setActive(Calibration_Menu);
            break;

        case app::StartupAction::HandOverToHaMenu:
            inStartup = false;
            if (outcome.caption != nullptr && menuDisplayPort != nullptr)
            {
                menuDisplayPort->writeLine(0, outcome.caption);
            }
            lcdMenu.setActive(HA_Menu);
            break;

        case app::StartupAction::Completed:
            inStartup      = false;
            okToUpdateMenu = true;
    #if TRACK_ON_BOOT == 1
            ui.motion->startSlewing(TRACKING);
    #endif
            lcdMenu.setActive(RA_Menu);
            lcdMenu.updateDisplay();
            break;

        case app::StartupAction::None:
        default:
            break;
    }
}

/// What the loop hands a converted screen: the key that just settled and the key held now.
app::MenuKeyInput screenInput()
{
    lcdButton_t edgeKey;
    const bool changed = lcdButtons.keyChanged(&edgeKey);
    return app::MenuKeyInput {changed ? toCoreButton(edgeKey) : core::Button::None, toCoreButton(lcdButtons.currentState())};
}

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
    ui.tick();
    serviceInfoDisplay();

    // The slew progress view owns the screen only while an axis is moving; the rest of the time
    // the menu draws it. This used to be a call from inside `Mount::loop()`.
    if ((ui.read->isAxisRunning(RA_STEPS) || ui.read->isAxisRunning(DEC_STEPS)) && (menuDisplayPort != nullptr))
    {
        app::slewStatusView.renderThrottled(*ui.read, app::serialHostOwnsDisplay(), ui.clock->now(), *menuDisplayPort);
    }

    // Update the LCD display
    unsigned long now = millis();
    if (!inSerialControl && okToUpdateMenu && !inStartup && !ui.motion->isSlewingRAorDEC())
    {
        // Main menu display
        lcdMenu.updateDisplay();
    }

    // Tracking marker
    if ((ui.read->isBootComplete()) && (now - lastTrackingStatusPrint > 200))
    {
        lcdMenu.printAt(15, 0, ui.motion->isSlewingTRK() ? '&' : '`');
        lastTrackingStatusPrint = now;
    }

    lcdMenu.setCursor(0, 1);

    #if SUPPORT_SERIAL_CONTROL == 1
        #if defined(ARDUINO_ARCH_RP2040)
    // On RP2040, the Philhower core does not reliably call serialEvent() for the
    // USB CDC Serial port when a host application opens the port and immediately
    // sends data (e.g. OATControl). Poll explicitly on every loop() iteration so
    // that commands are always processed regardless of inSerialControl state.
    processSerialData();
        #endif
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
                MeadeCommandAdapter::instance()->processCommand(":Qq#");
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
            const app::StartupOutcome outcome = app::startupWizard.handleKey(screenInput(), ui);
            applyStartupOutcome(outcome);
            if (outcome.result == app::MenuKeyResult::WaitForRelease)
            {
                waitForButtonRelease = true;
            }
        }
        else
    #endif
        {
            switch (lcdMenu.getActive())
            {
                case RA_Menu:
                    {
                        const app::MenuKeyResult result = raMenu.handleKey(screenInput(), ui);
                        if (result == app::MenuKeyResult::NextScreen)
                        {
                            lcdMenu.setNextActive();
                        }
                        else if (result == app::MenuKeyResult::WaitForRelease)
                        {
                            waitForButtonRelease = true;
                        }
                    }
                    break;
                case DEC_Menu:
                    {
                        const app::MenuKeyResult result = decMenu.handleKey(screenInput(), ui);
                        if (result == app::MenuKeyResult::NextScreen)
                        {
                            lcdMenu.setNextActive();
                        }
                        else if (result == app::MenuKeyResult::WaitForRelease)
                        {
                            waitForButtonRelease = true;
                        }
                    }
                    break;
    #if SUPPORT_POINTS_OF_INTEREST == 1
                case POI_Menu:
                    {
                        const app::MenuKeyResult result = app::poiMenu.handleKey(screenInput(), *ui.read, *ui.motion, inNorthernHemisphere);
                        if (result == app::MenuKeyResult::NextScreen)
                        {
                            lcdMenu.setNextActive();
                        }
                        else if (result == app::MenuKeyResult::WaitForRelease)
                        {
                            waitForButtonRelease = true;
                        }
                    }
                    break;
    #else
                case Home_Menu:
                    {
                        lcdButton_t homeKey;
                        if (lcdButtons.keyChanged(&homeKey))
                        {
                            const app::MenuKeyResult result = homeMenu.handleKey(toCoreButton(homeKey), *ui.motion);
                            if (result == app::MenuKeyResult::NextScreen)
                            {
                                lcdMenu.setNextActive();
                            }
                            else if (result == app::MenuKeyResult::WaitForRelease)
                            {
                                waitForButtonRelease = true;
                            }
                        }
                    }
                    break;
    #endif

                case HA_Menu:
                    {
                        // The receiver decides which shape the screen has, and the board only knows
                        // that once it is wired, so the first visit puts the screen in its mode.
                        static bool haInitialised = false;
                        if (!haInitialised)
                        {
                            app::haMenu.enter(gpsPort != nullptr);
                            haInitialised = true;
                        }

                        app::HaMenuOutcome haOutcome;
                        const app::MenuKeyResult result = app::haMenu.handleKey(screenInput(), ui, gpsPort, startupAwaitingHa(), haOutcome);

                        if (haOutcome.haStored)
                        {
                            if (menuDisplayPort != nullptr)
                            {
                                menuDisplayPort->writeLine(1, "Stored.");
                            }
                            ui.delay(500);
    #if USE_GPS == 0
                            // Without a receiver, storing the hour angle is also what makes the
                            // mount start tracking; with one, the receiver is already running.
                            ui.motion->startSlewing(TRACKING);
    #endif
                        }
                        if (haOutcome.resumeStartup)
                        {
    #if SUPPORT_GUIDED_STARTUP == 1
                            app::startupWizard.haConfirmed();
                            inStartup = true;
    #endif
                        }

                        if (result == app::MenuKeyResult::NextScreen)
                        {
                            lcdMenu.setNextActive();
                        }
                        else if (result == app::MenuKeyResult::WaitForRelease)
                        {
                            waitForButtonRelease = true;
                        }
                    }
                    break;

    #if SUPPORT_CALIBRATION == 1
                case Calibration_Menu:
                    waitForButtonRelease = processCalibrationKeys();
                    break;
    #endif

    #if FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE
                case Focuser_Menu:
                    {
                        const app::MenuKeyResult result = focusMenu.handleKey(screenInput(), *ui.axes);
                        if (result == app::MenuKeyResult::NextScreen)
                        {
                            lcdMenu.setNextActive();
                        }
                        else if (result == app::MenuKeyResult::WaitForRelease)
                        {
                            waitForButtonRelease = true;
                        }
                    }
                    break;
    #endif

    #if SUPPORT_MANUAL_CONTROL == 1
                case Control_Menu:
                    {
                        const app::ControlMenuOutcome outcome
                            = app::controlMenu.handleKey(screenInput(), *ui.motion, app::serialHostOwnsDisplay(), startupAwaitingPole());
                        if (outcome.serialChanged)
                        {
                            inSerialControl = outcome.serialActive;
                        }
                        if (outcome.suspendMenuUpdate)
                        {
                            okToUpdateMenu = false;
                        }
                        if (outcome.resumeMenuUpdate)
                        {
                            okToUpdateMenu = true;
                        }
                        if (outcome.resumeStartup)
                        {
        #if SUPPORT_GUIDED_STARTUP == 1
                            app::startupWizard.poleConfirmed();
                            inStartup = true;
        #endif
                        }
                        if (outcome.result == app::MenuKeyResult::NextScreen)
                        {
                            lcdMenu.setNextActive();
                        }
                        else if (outcome.result == app::MenuKeyResult::WaitForRelease)
                        {
                            waitForButtonRelease = true;
                        }
                    }
                    break;
    #endif

    #if SUPPORT_INFO_DISPLAY == 1
                case Status_Menu:
                    {
                        const app::MenuKeyResult result = app::infoMenu.handleKey(screenInput());
                        if (result == app::MenuKeyResult::NextScreen)
                        {
                            lcdMenu.setNextActive();
                        }
                        else if (result == app::MenuKeyResult::WaitForRelease)
                        {
                            waitForButtonRelease = true;
                        }
                    }
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
                    ui.tick();
                    serviceInfoDisplay();
                } while (true);
            }
        }

        // Input handled, do output
        lcdMenu.setCursor(0, 1);

    #if SUPPORT_GUIDED_STARTUP == 1
        if (inStartup)
        {
            if (menuDisplayPort != nullptr)
            {
                app::startupWizard.render(*menuDisplayPort);
            }
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
                    if (menuDisplayPort != nullptr)
                    {
                        raMenu.render(ui, *menuDisplayPort);
                    }
                }
                else if (activeMenu == DEC_Menu)
                {
                    if (menuDisplayPort != nullptr)
                    {
                        decMenu.render(ui, *menuDisplayPort);
                    }
                }
    #if SUPPORT_POINTS_OF_INTEREST == 1
                else if (activeMenu == POI_Menu)
                {
                    if (menuDisplayPort != nullptr)
                    {
                        app::poiMenu.render(*ui.read, *menuDisplayPort);
                    }
                }
    #else
                else if (activeMenu == Home_Menu)
                {
                    if (menuDisplayPort != nullptr)
                    {
                        homeMenu.render(*ui.motion, *menuDisplayPort);
                    }
                }
    #endif
                else if (activeMenu == HA_Menu)
                {
                    if (menuDisplayPort != nullptr)
                    {
                        app::haMenu.render(*ui.read, *menuDisplayPort);
                    }
                }
    #if SUPPORT_MANUAL_CONTROL == 1
                else if (activeMenu == Control_Menu)
                {
                    if (menuDisplayPort != nullptr)
                    {
                        app::controlMenu.render(*ui.read, app::serialHostOwnsDisplay(), ui.clock->now(), *menuDisplayPort);
                    }
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
                    if (menuDisplayPort != nullptr)
                    {
                        focusMenu.render(*ui.axes, *menuDisplayPort);
                    }
                }
    #endif

    #if SUPPORT_INFO_DISPLAY == 1
                else if (activeMenu == Status_Menu)
                {
                    if (menuDisplayPort != nullptr)
                    {
                        const app::InfoMenuEnvironment environment = app::infoEnvironment();
                        app::infoMenu.render(*ui.read, *ui.config, *ui.siteTime, environment, ui.clock->now(), *menuDisplayPort);
                    }
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
