#include "../Configuration.hpp"
#include "Utility.hpp"
#include "Mount.hpp"

#ifdef TEST_VERIFY_MODE

    #include "testmenu.hpp"

extern Mount mount;

TestMenu *TestMenu::_currentMenu                 = nullptr;
TestMenuItem *TestMenu::_backItem                = nullptr;
testMenuState_t TestMenu::_menuState             = testMenuState_t::CLEAR;
testMenuInternalState_t TestMenu::_internalState = testMenuInternalState_t::IDLE;

TestMenuItem::TestMenuItem(String label, String action, TestMenu *subMenu)
{
    _key       = -1;
    _label     = label;
    _action    = action;
    _isSubMenu = subMenu != nullptr;
    _subMenu   = subMenu;
}

void TestMenuItem::display() const
{
    Serial.print("  [");
    Serial.print(_key);
    Serial.print("] ");
    Serial.println(_label);
}

int TestMenuItem::getKey() const
{
    return _key;
}

void TestMenuItem::setKey(int key)
{
    _key = key;
}

String TestMenuItem::getAction() const
{
    return _action;
}

TestMenu *TestMenuItem::getSubMenu() const
{
    return _subMenu;
}

void TestMenu::setParentMenu(TestMenu *parentMenu)
{
    _parentMenu = parentMenu;
}

TestMenu::TestMenu(int level, String name, String parent, TestMenuItem *choices, int numChoices, TestMenu *parentMenu)
{
    _lastTick   = 0;
    _level      = level;
    _name       = name;
    _parent     = parent;
    _choices    = choices;
    _numChoices = numChoices;
    _parentMenu = parentMenu;
    if (_currentMenu == nullptr)
    {
        _backItem = new TestMenuItem("Back", "Action:Back");
        TestMenu::_backItem->setKey(0);
    }
    _currentMenu = this;  // Set the last created menu as the current menu
    for (int i = 0; i < _numChoices; i++)
    {
        if (_choices[i].getSubMenu() != nullptr)
        {
            _choices[i].getSubMenu()->setParentMenu(this);
        }
    }
}
String getComponent(String comp)
{
    if (comp == "AUTO_AZ_ALT")
    {
        return F("AZ and ALT steppers (AutoPA)");
    }
    if (comp == "AUTO_AZ")
    {
        return F("AZ stepper");
    }
    if (comp == "AUTO_ALT")
    {
        return F("ALT stepper");
    }
    if (comp == "GYRO")
    {
        return F("Digital Level");
    }
    if (comp == "LCD_KEYPAD")
    {
        return F("LCD display and keypad");
    }

    if (comp == "LCD_I2C_MCP23008")
    {
        return F("LCD display (MCP23008)");
    };
    if (comp == "LCD_I2C_MCP23017")
    {
        return F("LCD display (MCP23017)");
    };
    if (comp == "LCD_JOY_I2C_SSD1306")
    {
        return F("LCD display (SSD1306) with joystick");
    };

    if (comp == "INFO_I2C_SSD1306_128x64")
    {
        return F("Info display (SSD1306)");
    };
    if (comp == "INFO_UNKNOWN")
    {
        return F("Info display (unknown type)");
    };

    if (comp == "FOC")
    {
        return F("Focuser stepper");
    };

    if (comp == "HSAH")
    {
        return F("RA Hall Sensor Auto-Homing");
    };
    if (comp == "HSAV")
    {
        return F("DEC Hall Sensor Auto-Homing");
    };

    if (comp == "ENDSW_RA")
    {
        return F("End switches on RA");
    };
    if (comp == "ENDSW_DEC")
    {
        return F("End switches on DEC");
    };
    if (comp == "ENDSW_RA_DEC")
    {
        return F("End switches on RA and DEC");
    };
    return F("Unknown component");
}

void printStepperInfo(StepperAxis axis, String info)
{
    String *splitInfo = splitStringBy(info, '|');
    String *stp       = splitInfo;
    Serial.println(axis == RA_STEPS ? "RA Info" : "DEC Info");
    Serial.println(F("--------"));
    Serial.print(F("       Stepper type: "));
    Serial.println(*stp);
    stp++;
    Serial.print(F("               Gear: "));
    Serial.print(*stp);
    Serial.println(F("-tooth"));
    stp++;
    if (*stp == "400")
    {
        Serial.println(F("         Resolution: 0.9 deg (400 steps/revolution)"));
    }
    else if (*stp == "200")
    {
        Serial.println(F("         Resolution: 1.8 deg (200 steps/revolution)"));
    }
    else
    {
        Serial.print(F("         Resolution: "));
        Serial.print(*stp);
        Serial.println(F(" steps/revolution"));
    }
    Serial.print(F("    Slew Microsteps: "));
    Serial.println(axis == RA_STEPS ? RA_SLEW_MICROSTEPPING : DEC_SLEW_MICROSTEPPING);
    if (axis == RA_STEPS)
    {
        Serial.print(F("Tracking Microsteps: "));
        Serial.println(RA_TRACKING_MICROSTEPPING);
        Serial.print(F("     Tracking Speed: "));
        Serial.println(mount.getSpeed(TRACKING));
    }
    else
    {
        Serial.print(F(" Guiding Microsteps: "));
        Serial.println(DEC_GUIDE_MICROSTEPPING);
    }

    Serial.print(F("       Steps/degree: "));
    Serial.println(mount.getStepsPerDegree(axis));
    delete[] splitInfo;
}

void TestMenu::listHardware() const
{
    Serial.println(F("Firmware is configured to support these hardware components:"));
    String *hw = splitStringBy(mount.getMountHardwareInfo(), ',');
    String *p  = hw;
    int index  = 0;
    Serial.print(F("                Mount: "));
    #ifdef OAM
    Serial.println(F("OpenAstroMount (OAM)"));
    #else
    Serial.println(F("OpenAstroTracker (OAT)"));
    #endif

    while (p->length() > 0)
    {
        switch (index)
        {
            case 0:
                Serial.print(F("                Board: "));
                Serial.println(*p);
                Serial.print(F("      Stepper library: "));
    #ifdef NEW_STEPPER_LIB
                Serial.println(F("InterruptAccelStepper (new)"));
    #else
                Serial.println(F("AccelStepper (old)"));
    #endif
                break;
            case 1:
                printStepperInfo(RA_STEPS, *p);
                break;
            case 2:
                printStepperInfo(DEC_STEPS, *p);
                Serial.println(F("Add-Ons"));
                Serial.println(F("--------"));
                break;
            default:
                if (!p->startsWith("NO_"))
                {
                    String component = getComponent(*p);
                    Serial.print(F("          Component: "));
                    Serial.println(component);
                }
                break;
        }
        p++;
        index++;
    }
    delete[] hw;
}

void TestMenu::connectDriver(String axisStr)
{
    #if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART || DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART                                          \
        || AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART || ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART                                       \
        || FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    Serial.print("Connecting to " + axisStr + " driver....");
    Serial.println(mount.connectToDriver(axisStr) ? "OK" : "FAIL");
    #else
    Serial.print(F("ERROR: Can only connect to TMC2209 UART drivers."));
    #endif
}

void TestMenu::onKeyPressed(int key)
{
    if ((key == TestMenu::_backItem->getKey()) && (_parentMenu))
    {
        _currentMenu = _parentMenu;
        _currentMenu->display();
        return;
    }

    Serial.println();
    for (int i = 0; i < _numChoices; i++)
    {
        if (_choices[i].getKey() == key)
        {
            if (_choices[i].getAction().startsWith("Submenu:"))
            {
                _currentMenu = _choices[i].getSubMenu();
                _currentMenu->display();
                return;
            }
            String cmd       = _choices[i].getAction();
            int sep          = cmd.indexOf(':');
            String verb      = cmd.substring(0, sep);
            String action    = cmd.substring(sep + 1);
            String actionArg = "";
            int argSep       = action.indexOf('|');
            if (argSep > 0)
            {
                action    = action.substring(0, argSep);
                actionArg = action.substring(argSep + 1);
            }
            if (verb == "Action")
            {
                if (action == "ListHardware")
                {
                    listHardware();
                }
                else if (action.startsWith("Connect"))
                {
                    connectDriver(actionArg);
                }
                else if (action == "MoveRAAxis")
                {
                    long stepsPerDegree = mount.getStepsPerDegree(RA_STEPS);
                    String output       = F("Moving RA axis by 1hr (15 degrees, ");
                    output += stepsPerDegree * 15;
                    output += " steps) " + actionArg;
                    Serial.println(output);
                    _startRA  = mount.getCurrentStepperPosition(RA_STEPS);
                    _targetRA = _startRA + (actionArg == "CCW" ? -1 : 1) * stepsPerDegree * 15;
                    mount.moveStepperBy(RA_STEPS, stepsPerDegree * 15);
                    _internalState = DISPLAY_RA;
                }
                else if (action == "MoveDECAxis")
                {
                    long stepsPerDegree = mount.getStepsPerDegree(DEC_STEPS);
                    String output       = F("Moving DEC axis by 15 degrees (");
                    output += stepsPerDegree * 15;
                    output += " steps) " + actionArg;
                    Serial.println(output);
                    _startDEC  = mount.getCurrentStepperPosition(DEC_STEPS);
                    _targetDEC = _startDEC + (actionArg == "DOWN" ? -1 : 1) * stepsPerDegree * 15;
                    mount.moveStepperBy(DEC_STEPS, stepsPerDegree * 15);
                    _internalState = DISPLAY_DEC;
                }
                else if (action == "ToggleTRK")
                {
                    if (mount.isSlewingTRK())
                    {
                        mount.stopSlewing(TRACKING);
                        Serial.println(F("Tracking stopped."));
                    }
                    else
                    {
                        mount.startSlewing(TRACKING);
                        Serial.println(F("Tracking started."));
                    }
                }
                else if (action == "FactoryReset")
                {
                    mount.clearConfiguration();
                    Serial.println(F("Mount reset, EEPROM erased."));
                }
            }
            if (TestMenu::getMenuState() == testMenuState_t::CLEAR)
            {
                _currentMenu->display();
            }
            return;
        }
    }
    Serial.println(F("Invalid key pressed."));
    _currentMenu->display();
}

void TestMenu::display() const
{
    Serial.println("");
    Serial.println("");
    String statusRaDec;
    statusRaDec = "*  RA: ";
    statusRaDec += mount.isAxisRunning(RA_STEPS) ? "^ " : ". ";
    statusRaDec += rightJustify(String(mount.getCurrentStepperPosition(RA_STEPS)), 7);
    statusRaDec += "    DEC: ";
    statusRaDec += mount.isAxisRunning(DEC_STEPS) ? "^ " : ". ";
    statusRaDec += rightJustify(String(mount.getCurrentStepperPosition(DEC_STEPS)), 7);
    statusRaDec += "    TRK: ";
    statusRaDec += mount.isSlewingTRK() ? "^ " : ". ";
    statusRaDec += rightJustify(String(mount.getCurrentStepperPosition(TRACKING)), 7);

    String statusAltAz;
    statusAltAz = "* ALT: ";
    statusAltAz += mount.isAxisRunning(ALTITUDE_STEPS) ? "^ " : ". ";
    statusAltAz += rightJustify(String(mount.getCurrentStepperPosition(ALTITUDE_STEPS)), 7);
    statusAltAz += "     AZ: ";
    statusAltAz += mount.isAxisRunning(AZIMUTH_STEPS) ? "^ " : ". ";
    statusAltAz += rightJustify(String(mount.getCurrentStepperPosition(AZIMUTH_STEPS)), 7);
    statusAltAz += "    FOC: ";
    statusAltAz += mount.isAxisRunning(FOCUS_STEPS) ? "^ " : ". ";
    statusAltAz += rightJustify(String(mount.getCurrentStepperPosition(FOCUS_STEPS)), 7);

    if (_level == 0)
    {
    #ifdef OAM
        Serial.println(F("**************************************"));
        Serial.println(F("*** OpenAstroMount (OAM) Test Menu ***"));
    #else
        Serial.println(F("** OpenAstroTracker (OAT) Test Menu **"));
    #endif
        Serial.println(F("**************************************"));
        Serial.print(F("* Mem: "));
        Serial.print(freeMemory());
        Serial.println(F(" bytes"));
        Serial.println(statusRaDec);
        Serial.println(statusAltAz);
        Serial.println(F("**************************************"));
    }
    else
    {
        Serial.print(F("--------------- "));
        Serial.print(freeMemory());
        Serial.println(F(" bytes"));
        Serial.print("  ");
        Serial.print(_name);
        Serial.println(F(" Menu"));
        Serial.println(F("--------------------------"));
    }

    Serial.println(F("Please choose:"));
    for (int i = 0; i < _numChoices; i++)
    {
        _choices[i].setKey(i + 1);
        _choices[i].display();
    }

    if (_parentMenu)
    {
        Serial.println();
        TestMenu::_backItem->display();
    }
    Serial.print(F("Your choice:"));
}

void TestMenu::tick()
{
    if (millis() > _lastTick + 250)
    {
        _lastTick = millis();
        switch (_internalState)
        {
            case DISPLAY_RA:
                Serial.print("RA : ");
                Serial.print(mount.getCurrentStepperPosition(RA_STEPS));
                Serial.print(" (");
                Serial.print(100 * (mount.getCurrentStepperPosition(RA_STEPS) - _startRA) / (_targetRA - _startRA));
                Serial.println("%)");
                if (!mount.isAxisRunning(RA_STEPS))
                {
                    _internalState = IDLE;
                }
                break;

            case DISPLAY_DEC:
                Serial.print("DEC: ");
                Serial.print(mount.getCurrentStepperPosition(DEC_STEPS));
                Serial.print(" (");
                Serial.print(100 * (mount.getCurrentStepperPosition(DEC_STEPS) - _startDEC) / (_targetDEC - _startDEC));
                Serial.println("%)");
                if (!mount.isAxisRunning(DEC_STEPS))
                {
                    _internalState = IDLE;
                }
                break;
            default:
                break;
        }
    }
}

testMenuState_t TestMenu::getMenuState()
{
    return TestMenu::_menuState;
}

void TestMenu::setMenuState(testMenuState_t state)
{
    TestMenu::_menuState = state;
}

TestMenu *TestMenu::getCurrentMenu()
{
    return TestMenu::_currentMenu;
}

#endif  // TEST_VERIFY_MODE