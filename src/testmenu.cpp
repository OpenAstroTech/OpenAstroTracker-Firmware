#include "../Configuration.hpp"
#include "Utility.hpp"
#include "Mount.hpp"

#ifdef TEST_VERIFY_MODE

    #include "testmenu.hpp"

extern Mount mount;

TestMenu *TestMenu::_currentMenu               = nullptr;
TestMenuItem *TestMenu::_backItem              = nullptr;
testMenuState_t TestMenu::_menuState           = testMenuState_t::CLEAR;
testMenuInternalState TestMenu::_internalState = testMenuInternalState::IDLE;

inline testMenuInternalState operator|=(testMenuInternalState &a, testMenuInternalState b)
{
    return a = static_cast<testMenuInternalState>(static_cast<int>(a) | static_cast<int>(b));
};

inline testMenuInternalState operator|=(testMenuInternalState &a, int b)
{
    return a = static_cast<testMenuInternalState>(static_cast<int>(a) | b);
};

long TestMenu::_targetRA  = 0;
long TestMenu::_startRA   = 0;
long TestMenu::_targetDEC = 0;
long TestMenu::_startDEC  = 0;
long TestMenu::_startAZ   = 0;
long TestMenu::_targetAZ  = 0;
long TestMenu::_startALT  = 0;
long TestMenu::_targetALT = 0;

String getMenuLabel(menuText_t labelId)
{
    switch (labelId)
    {
        case MENU_BACK:
            return F("Back");
        case MENU_CONNECT_RA:
            return F("Connect to RA Driver");
        case MENU_CONNECT_DEC:
            return F("Connect to DEC Driver");
        case MENU_CONNECT_ALT:
            return F("Connect to ALT Driver");
        case MENU_CONNECT_AZ:
            return F("Connect to AZ Driver");
        case MENU_CONNECT_FOC:
            return F("Connect to FOCUS Driver");
        case MENU_PRIMARY_RA_CW:
            return F("Move RA Axis 1h clockwise");
        case MENU_PRIMARY_RA_CCW:
            return F("Move RA Axis 1h counter-clockwise");
        case MENU_PRIMARY_DEC_UP:
            return F("Move DEC Axis 15deg up");
        case MENU_PRIMARY_DEC_DOWN:
            return F("Move DEC Axis 15deg down");
        case MENU_TOGGLE_TRK:
            return F("Stop/Start Tracking");
        case MENU_SECONDARY_RATE_1:
            return F("Set distance to 0.1 arcmin");
        case MENU_SECONDARY_RATE_2:
            return F("Set distance to 0.5 arcmin");
        case MENU_SECONDARY_RATE_3:
            return F("Set distance to 2 arcmin");
        case MENU_SECONDARY_RATE_4:
            return F("Set distance to 5 arcmin");
        case MENU_SECONDARY_RATE_5:
            return F("Set distance to 15 arcmin");
        case MENU_SECONDARY_ALT_UP:
            return F("Move ALT Axis Up");
        case MENU_SECONDARY_ALT_DOWN:
            return F("Move ALT Axis Down");
        case MENU_SECONDARY_AZ_LEFT:
            return F("Move AZ Axis Left");
        case MENU_SECONDARY_AZ_RIGHT:
            return F("Move AZ Axis Right");
        case MENU_FACTORY_RESET:
            return F("Factory Reset (Erase EEPROM)");
        case MENU_MAIN_LIST_HARDWARE:
            return F("List Hardware");
        case MENU_MAIN_CONNECT_DRIVERS:
            return F("Connect Drivers");
        case MENU_MAIN_PRIMARY_AXIS_MOVES:
            return F("Primary Axis Moves (RA/DEC)");
        case MENU_MAIN_SECONDARY_AXIS_MOVES:
            return F("Secondary Axis Moves (ALT/AZ)");
        case MENU_PRIMARY_SET_HOME:
            return F("Set current as Home");
        case MENU_PRIMARY_GO_HOME:
            return F("Go Home");
        default:
            return F("Unknown");
    }
}

String getMenuAction(menuText_t labelId)
{
    switch (labelId)
    {
        case MENU_BACK:
            return F("Action:Back");
        case MENU_CONNECT_RA:
            return F("Action:Connect|RA");
        case MENU_CONNECT_DEC:
            return F("Action:Connect|DEC");
        case MENU_CONNECT_ALT:
            return F("Action:Connect|ALT");
        case MENU_CONNECT_AZ:
            return F("Action:Connect|AZ");
        case MENU_CONNECT_FOC:
            return F("Action:Connect|FOC");
        case MENU_PRIMARY_RA_CW:
            return F("Action:MoveRAAxis|CW");
        case MENU_PRIMARY_RA_CCW:
            return F("Action:MoveRAAxis|CCW");
        case MENU_PRIMARY_SET_HOME:
            return F("Action:SetHome");
        case MENU_PRIMARY_GO_HOME:
            return F("Action:GoHome");
        case MENU_PRIMARY_DEC_UP:
            return F("Action:MoveDECAxis|UP");
        case MENU_PRIMARY_DEC_DOWN:
            return F("Action:MoveDECAxis|DOWN");
        case MENU_TOGGLE_TRK:
            return F("Action:ToggleTRK");

        case MENU_SECONDARY_RATE_1:
            return F("Action:SetSecDist|0.1");
        case MENU_SECONDARY_RATE_2:
            return F("Action:SetSecDist|0.5");
        case MENU_SECONDARY_RATE_3:
            return F("Action:SetSecDist|2");
        case MENU_SECONDARY_RATE_4:
            return F("Action:SetSecDist|5");
        case MENU_SECONDARY_RATE_5:
            return F("Action:SetSecDist|15");

        case MENU_SECONDARY_ALT_UP:
            return F("Action:MoveALTAxis|UP");
        case MENU_SECONDARY_ALT_DOWN:
            return F("Action:MoveALTAxis|DOWN");
        case MENU_SECONDARY_AZ_LEFT:
            return F("Action:MoveAZAxis|LEFT");
        case MENU_SECONDARY_AZ_RIGHT:
            return F("Action:MoveAZAxis|RIGHT");
        case MENU_FACTORY_RESET:
            return F("Action:FactoryReset");
        case MENU_MAIN_LIST_HARDWARE:
            return F("Action:ListHardware");
        case MENU_MAIN_CONNECT_DRIVERS:
            return F("Submenu:ConnectDrivers");
        case MENU_MAIN_PRIMARY_AXIS_MOVES:
            return F("Submenu:PrimaryAxisMoves");
        case MENU_MAIN_SECONDARY_AXIS_MOVES:
            return F("Submenu:SecondaryAxisMoves");
        default:
            return F("Unknown");
    }
}
TestMenuItem::TestMenuItem(menuText_t labelId, TestMenu *subMenu)
{
    _key       = -1;
    _label     = getMenuLabel(labelId);
    _action    = getMenuAction(labelId);
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
    _lastTick          = 0;
    _level             = level;
    _name              = name;
    _parent            = parent;
    _choices           = choices;
    _numChoices        = numChoices;
    _parentMenu        = parentMenu;
    _secondaryDistance = 1;
    if (_currentMenu == nullptr)
    {
        _backItem = new TestMenuItem(MENU_BACK);
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
    Serial.print(F("              Mount: "));
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
                Serial.print(F("              Board: "));
                Serial.println(*p);
                Serial.print(F("    Stepper library: "));
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
    uint16_t current = 0;
    Serial.print("Connecting to " + axisStr + " driver....");
    bool connected = mount.connectToDriver(axisStr, &current);
    Serial.println(connected ? "OK" : "FAIL");
    if (connected)
    {
        Serial.print(F("Stepper is configured to use: "));
        Serial.println(String(current) + " mA");
    }
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
                actionArg = action.substring(argSep + 1);
                action    = action.substring(0, argSep);
            }
            if (verb == "Action")
            {
                if (action == "ListHardware")
                {
                    listHardware();
                    _currentMenu->display();
                }
                else if (action.startsWith("Connect"))
                {
                    connectDriver(actionArg);
                }
                else if (action == "SetHome")
                {
                    mount.setHome(false);
                    _currentMenu->display();
                }
                else if (action == "GoHome")
                {
                    _startDEC  = mount.getCurrentStepperPosition(DEC_STEPS);
                    _startRA   = mount.getCurrentStepperPosition(RA_STEPS);
                    _targetDEC = 0;
                    _targetRA  = 0;
                    mount.startSlewingToHome();
                    _internalState |= (DISPLAY_RA | DISPLAY_DEC);
                }
                else if (action == "SetSecDist")
                {
                    _secondaryDistance = actionArg.toFloat();
                    _currentMenu->display();
                }
                else if (action == "MoveRAAxis")
                {
                    long stepsPerDegree = mount.getStepsPerDegree(RA_STEPS);
                    String output       = F("Moving RA axis by 1hr (15 degrees, ");
                    output += stepsPerDegree * 15;
                    output += " steps) " + actionArg;
                    Serial.println(output);
                    TestMenu::_startRA  = mount.getCurrentStepperPosition(RA_STEPS);
                    long steps          = (actionArg == "CCW" ? -1 : 1) * stepsPerDegree * 15;
                    TestMenu::_targetRA = TestMenu::_startRA + steps;
                    mount.moveStepperBy(RA_STEPS, steps);
                    _internalState |= DISPLAY_RA;
                }
                else if (action == "MoveDECAxis")
                {
                    long stepsPerDegree = mount.getStepsPerDegree(DEC_STEPS);
                    String output       = F("Moving DEC axis by 15 degrees (");
                    output += stepsPerDegree * 15;
                    output += " steps) " + actionArg;
                    Serial.println(output);
                    TestMenu::_startDEC  = mount.getCurrentStepperPosition(DEC_STEPS);
                    long steps           = (actionArg == "DOWN" ? -1 : 1) * stepsPerDegree * 15;
                    TestMenu::_targetDEC = TestMenu::_startDEC + steps;
                    mount.moveStepperBy(DEC_STEPS, steps);
                    _internalState |= DISPLAY_DEC;
                }
                else if (action == "MoveAZAxis")
                {
                    String output = String(F("Moving AZ axis by ")) + String(_secondaryDistance, 1) + String(F(" arcMins ("));
                    output += String(AZIMUTH_STEPS_PER_ARC_MINUTE * _secondaryDistance, 0) + " steps) " + actionArg;
                    Serial.println(output);
                    float arcmins       = actionArg == "LEFT" ? _secondaryDistance : -_secondaryDistance;
                    TestMenu::_startAZ  = mount.getCurrentStepperPosition(AZIMUTH_STEPS);
                    TestMenu::_targetAZ = TestMenu::_startAZ + arcmins * AZIMUTH_STEPS_PER_ARC_MINUTE;
                    mount.moveBy(AZIMUTH_STEPS, arcmins);
                    _internalState |= DISPLAY_AZ;
                }
                else if (action == "MoveALTAxis")
                {
                    String output = String(F("Moving ALT axis by ")) + String(_secondaryDistance, 1) + String(F(" arcMins ("));
                    output += String(ALTITUDE_STEPS_PER_ARC_MINUTE * _secondaryDistance, 0) + " steps) " + actionArg;
                    Serial.println(output);
                    float arcmins        = actionArg == "UP" ? _secondaryDistance : -_secondaryDistance;
                    TestMenu::_startALT  = mount.getCurrentStepperPosition(ALTITUDE_STEPS);
                    TestMenu::_targetALT = TestMenu::_startALT + arcmins * ALTITUDE_STEPS_PER_ARC_MINUTE;
                    mount.moveBy(ALTITUDE_STEPS, arcmins);
                    _internalState |= DISPLAY_ALT;
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
                    _currentMenu->display();
                }
                else if (action == "FactoryReset")
                {
                    mount.clearConfiguration();
                    Serial.println(F("Mount reset, EEPROM erased."));
                    _currentMenu->display();
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

void TestMenu::displayStepperPos() const
{
    String statusRaDec;
    statusRaDec = F("  RA: ");
    statusRaDec += rightJustify(String(mount.getCurrentStepperPosition(RA_STEPS)), 8);
    statusRaDec += mount.isAxisRunning(RA_STEPS) ? "^" : " ";
    statusRaDec += F("   ALT: ");
    statusRaDec += rightJustify(String(mount.getCurrentStepperPosition(ALTITUDE_STEPS)), 8);
    statusRaDec += mount.isAxisRunning(ALTITUDE_STEPS) ? "^" : " ";
    statusRaDec += F("   TRK: ");
    statusRaDec += rightJustify(String(mount.getCurrentStepperPosition(TRACKING)), 8);
    statusRaDec += mount.isSlewingTRK() ? "^" : " ";

    String statusAltAz;
    statusAltAz = F(" DEC: ");
    statusAltAz += rightJustify(String(mount.getCurrentStepperPosition(DEC_STEPS)), 8);
    statusAltAz += mount.isAxisRunning(DEC_STEPS) ? "^" : " ";
    statusAltAz += F("    AZ: ");
    statusAltAz += rightJustify(String(mount.getCurrentStepperPosition(AZIMUTH_STEPS)), 8);
    statusAltAz += mount.isAxisRunning(AZIMUTH_STEPS) ? "^" : " ";
    statusAltAz += F("   FOC: ");
    statusAltAz += rightJustify(String(mount.getCurrentStepperPosition(FOCUS_STEPS)), 8);
    statusAltAz += mount.isAxisRunning(FOCUS_STEPS) ? "^" : " ";
    Serial.println(statusRaDec);
    Serial.println(statusAltAz);
}

void TestMenu::display() const
{
    Serial.println("");

    if (_level == 0)
    {
        Serial.println(F("**************************************"));
    #ifdef OAM
        Serial.println(F("*** OpenAstroMount (OAM) Test Menu ***"));
    #else
        Serial.println(F("** OpenAstroTracker (OAT) Test Menu **"));
    #endif
        Serial.print(F("************* "));
        Serial.print(freeMemory());
        Serial.println(F(" bytes *************"));
        displayStepperPos();
        Serial.println(F("**************************************"));
    }
    else
    {
        Serial.print(F("------------------- "));
        Serial.print(freeMemory());
        Serial.println(F(" bytes -------------------"));
        displayStepperPos();
        Serial.println(F("--------------------------------------------------"));
        Serial.print("  ");
        Serial.print(_name);
        Serial.println(F(" Menu"));
        Serial.println(F("--------------------------"));
    }

    //Serial.println(F("Please choose:"));
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
        if (_internalState != testMenuInternalState::IDLE)
        {
            if (_internalState & DISPLAY_RA)
            {
                Serial.print(F("RA : "));
                Serial.print(mount.getCurrentStepperPosition(RA_STEPS));
                Serial.print(" (");
                Serial.print(String(100.0 * (mount.getCurrentStepperPosition(RA_STEPS) - TestMenu::_startRA)
                                        / (TestMenu::_targetRA - TestMenu::_startRA),
                                    0));
                Serial.print(F("%)   "));
                if (!mount.isAxisRunning(RA_STEPS))
                {
                    _internalState = static_cast<testMenuInternalState>(static_cast<int>(_internalState) & ~static_cast<int>(DISPLAY_RA));
                }
            }

            if (_internalState & DISPLAY_DEC)
            {
                Serial.print(F("DEC: "));
                Serial.print(mount.getCurrentStepperPosition(DEC_STEPS));
                Serial.print(" (");
                Serial.print(String(100.0 * (mount.getCurrentStepperPosition(DEC_STEPS) - TestMenu::_startDEC)
                                        / (TestMenu::_targetDEC - TestMenu::_startDEC),
                                    0));
                Serial.print(F("%)   "));
                if (!mount.isAxisRunning(DEC_STEPS))
                {
                    _internalState = static_cast<testMenuInternalState>(static_cast<int>(_internalState) & ~static_cast<int>(DISPLAY_DEC));
                }
            }

            if (_internalState & DISPLAY_AZ)
            {
                Serial.print(F("AZ: "));
                Serial.print(mount.getCurrentStepperPosition(AZIMUTH_STEPS));
                Serial.print(" (");
                Serial.print(String(100.0 * (mount.getCurrentStepperPosition(AZIMUTH_STEPS) - TestMenu::_startAZ)
                                        / (TestMenu::_targetAZ - TestMenu::_startAZ),
                                    0));
                Serial.print(F("%)   "));
                if (!mount.isAxisRunning(AZIMUTH_STEPS))
                {
                    _internalState = static_cast<testMenuInternalState>(static_cast<int>(_internalState) & ~static_cast<int>(DISPLAY_AZ));
                }
            }

            if (_internalState & DISPLAY_ALT)
            {
                Serial.print(F("ALT: "));
                Serial.print(mount.getCurrentStepperPosition(ALTITUDE_STEPS));
                Serial.print(" (");
                Serial.print(String(100.0 * (mount.getCurrentStepperPosition(ALTITUDE_STEPS) - TestMenu::_startALT)
                                        / (TestMenu::_targetALT - TestMenu::_startALT),
                                    0));
                Serial.print(F("%)   "));
                if (!mount.isAxisRunning(ALTITUDE_STEPS))
                {
                    _internalState = static_cast<testMenuInternalState>(static_cast<int>(_internalState) & ~static_cast<int>(DISPLAY_ALT));
                }
            }

            Serial.println();

            if (_internalState == testMenuInternalState::IDLE)
            {
                _currentMenu->display();
            }
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