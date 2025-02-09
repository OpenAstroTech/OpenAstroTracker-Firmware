#include "../Configuration.hpp"
#include "Utility.hpp"
#include "Mount.hpp"
#include "MeadeCommandProcessor.hpp"

#if TEST_VERIFY_MODE == 1

    #include "MappedDict.hpp"
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
        CASERETURN(MENU_BACK, F("Back"));
        CASERETURN(MENU_CONNECT_RA, F("Connect to RA Driver"));
        CASERETURN(MENU_CONNECT_DEC, F("Connect to DEC Driver"));
        CASERETURN(MENU_CONNECT_ALT, F("Connect to ALT Driver"));
        CASERETURN(MENU_CONNECT_AZ, F("Connect to AZ Driver"));
        CASERETURN(MENU_CONNECT_FOC, F("Connect to FOCUS Driver"));
        CASERETURN(MENU_PRIMARY_RA_CW, F("Move RA Axis 1h clockwise"));
        CASERETURN(MENU_PRIMARY_RA_CCW, F("Move RA Axis 1h counter-clockwise"));
        CASERETURN(MENU_PRIMARY_DEC_UP, F("Move DEC Axis 15deg up"));
        CASERETURN(MENU_PRIMARY_DEC_DOWN, F("Move DEC Axis 15deg down"));
        CASERETURN(MENU_TOGGLE_TRK, F("Stop/Start Tracking"));
        CASERETURN(MENU_SECONDARY_RATE_1, F("Set distance to 0.1 arcmin"));
        CASERETURN(MENU_SECONDARY_RATE_2, F("Set distance to 0.5 arcmin"));
        CASERETURN(MENU_SECONDARY_RATE_3, F("Set distance to 2 arcmin"));
        CASERETURN(MENU_SECONDARY_RATE_4, F("Set distance to 5 arcmin"));
        CASERETURN(MENU_SECONDARY_RATE_5, F("Set distance to 15 arcmin"));
        CASERETURN(MENU_SECONDARY_ALT_UP, F("Move ALT Axis Up"));
        CASERETURN(MENU_SECONDARY_ALT_DOWN, F("Move ALT Axis Down"));
        CASERETURN(MENU_SECONDARY_AZ_LEFT, F("Move AZ Axis Left"));
        CASERETURN(MENU_SECONDARY_AZ_RIGHT, F("Move AZ Axis Right"));
        CASERETURN(MENU_FACTORY_RESET, F("Factory Reset (Erase EEPROM)"));
        CASERETURN(MENU_PASSTHROUGH_COMMAND, F("Issue LX200 Command"));
        CASERETURN(MENU_MAIN_LIST_HARDWARE, F("List Hardware"));
        CASERETURN(MENU_MAIN_CONNECT_DRIVERS, F("Connect Drivers"));
        CASERETURN(MENU_MAIN_PRIMARY_AXIS_MOVES, F("Primary Axis Moves (RA/DEC)"));
        CASERETURN(MENU_MAIN_SECONDARY_AXIS_MOVES, F("Secondary Axis Moves (ALT/AZ)"));
        CASERETURN(MENU_PRIMARY_SET_HOME, F("Set current as Home"));
        CASERETURN(MENU_PRIMARY_GO_HOME, F("Go Home"));
        default:
            return F("Unknown");
    }
}

String getMenuAction(menuText_t labelId)
{
    switch (labelId)
    {
        CASERETURN(MENU_BACK, F("Action:Back"));
        CASERETURN(MENU_CONNECT_RA, F("Action:Connect|RA"));
        CASERETURN(MENU_CONNECT_DEC, F("Action:Connect|DEC"));
        CASERETURN(MENU_CONNECT_ALT, F("Action:Connect|ALT"));
        CASERETURN(MENU_CONNECT_AZ, F("Action:Connect|AZ"));
        CASERETURN(MENU_CONNECT_FOC, F("Action:Connect|FOC"));
        CASERETURN(MENU_PRIMARY_RA_CW, F("Action:MoveRAAxis|CW"));
        CASERETURN(MENU_PRIMARY_RA_CCW, F("Action:MoveRAAxis|CCW"));
        CASERETURN(MENU_PRIMARY_SET_HOME, F("Action:SetHome"));
        CASERETURN(MENU_PRIMARY_GO_HOME, F("Action:GoHome"));
        CASERETURN(MENU_PRIMARY_DEC_UP, F("Action:MoveDECAxis|UP"));
        CASERETURN(MENU_PRIMARY_DEC_DOWN, F("Action:MoveDECAxis|DOWN"));
        CASERETURN(MENU_TOGGLE_TRK, F("Action:ToggleTRK"));

        CASERETURN(MENU_SECONDARY_RATE_1, F("Action:SetSecDist|0.1"));
        CASERETURN(MENU_SECONDARY_RATE_2, F("Action:SetSecDist|0.5"));
        CASERETURN(MENU_SECONDARY_RATE_3, F("Action:SetSecDist|2"));
        CASERETURN(MENU_SECONDARY_RATE_4, F("Action:SetSecDist|5"));
        CASERETURN(MENU_SECONDARY_RATE_5, F("Action:SetSecDist|15"));

        CASERETURN(MENU_SECONDARY_ALT_UP, F("Action:MoveALTAxis|UP"));
        CASERETURN(MENU_SECONDARY_ALT_DOWN, F("Action:MoveALTAxis|DOWN"));
        CASERETURN(MENU_SECONDARY_AZ_LEFT, F("Action:MoveAZAxis|LEFT"));
        CASERETURN(MENU_SECONDARY_AZ_RIGHT, F("Action:MoveAZAxis|RIGHT"));
        CASERETURN(MENU_FACTORY_RESET, F("Action:FactoryReset"));
        CASERETURN(MENU_PASSTHROUGH_COMMAND, F("Action:PassthroughCmd"));
        CASERETURN(MENU_MAIN_LIST_HARDWARE, F("Action:ListHardware"));
        CASERETURN(MENU_MAIN_CONNECT_DRIVERS, F("Submenu:ConnectDrivers"));
        CASERETURN(MENU_MAIN_PRIMARY_AXIS_MOVES, F("Submenu:PrimaryAxisMoves"));
        CASERETURN(MENU_MAIN_SECONDARY_AXIS_MOVES, F("Submenu:SecondaryAxisMoves"));
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
String getComponent(const String &comp)
{
    MappedDict<String, String>::DictEntry_t lookupTable[] = {
        {F("AUTO_AZ_ALT"), F("AZ and ALT steppers (AutoPA)")},
        {F("AUTO_AZ"), F("AZ stepper")},
        {F("AUTO_ALT"), F("ALT stepper")},
        {F("GPS"), F("GPS receiver")},
        {F("GYRO"), F("Digital Level")},
        {F("LCD_KEYPAD"), F("LCD display and keypad")},
        {F("LCD_I2C_MCP23008"), F("LCD display (MCP23008)")},
        {F("LCD_I2C_MCP23017"), F("LCD display (MCP23017)")},
        {F("LCD_JOY_I2C_SSD1306"), F("LCD display (SSD1306) with joystick")},
        {F("INFO_I2C_SSD1306_128x64"), F("Info display (SSD1306)")},
        {F("INFO_UNKNOWN"), F("Info display (unknown type)")},
        {F("FOC"), F("Focuser stepper")},
        {F("HSAH"), F("RA Hall Sensor Auto-Homing")},
        {F("HSAV"), F("DEC Hall Sensor Auto-Homing")},
        {F("ENDSW_RA"), F("End switches on RA")},
        {F("ENDSW_DEC"), F("End switches on DEC")},
        {F("ENDSW_RA_DEC"), F("End switches on RA and DEC")},
    };
    auto driverLookup = MappedDict<String, String>(lookupTable, ARRAY_SIZE(lookupTable));

    String rtn;
    if (driverLookup.tryGet(comp, &rtn))
    {
        return rtn;
    }
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
    Serial.print(F("Connecting to "));
    Serial.print(axisStr);
    Serial.print(F(" driver...."));
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

void TestMenu::onCommandReceived(String s)
{
    Serial.println(s);
    if (s.startsWith(":") && s.endsWith("#"))
    {
        s = s.substring(0, s.length() - 1);
    }

    String reply = MeadeCommandProcessor::instance()->processCommand(s);

    if (reply.length() > 0)
    {
        Serial.println(F("-- Command Response --------"));
        Serial.println(reply);
        Serial.println(F("----------------------------"));
    }
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
                if (action == F("ListHardware"))
                {
                    listHardware();
                    _currentMenu->display();
                }
                else if (action.startsWith("Connect"))
                {
                    connectDriver(actionArg);
                }
                else if (action == F("SetHome"))
                {
                    mount.setHome(false);
                    _currentMenu->display();
                }
                else if (action == F("GoHome"))
                {
                    _startDEC  = mount.getCurrentStepperPosition(DEC_STEPS);
                    _startRA   = mount.getCurrentStepperPosition(RA_STEPS);
                    _targetDEC = 0;
                    _targetRA  = 0;
                    mount.startSlewingToHome();
                    _internalState |= (DISPLAY_RA | DISPLAY_DEC);
                }
                else if (action == F("SetSecDist"))
                {
                    _secondaryDistance = actionArg.toFloat();
                    _currentMenu->display();
                }
                else if (action == F("MoveRAAxis"))
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
                else if (action == F("MoveDECAxis"))
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
    #if AZ_STEPPER_TYPE != STEPPER_TYPE_NONE
                else if (action == F("MoveAZAxis"))
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
    #endif
    #if ALT_STEPPER_TYPE != STEPPER_TYPE_NONE
                else if (action == F("MoveALTAxis"))
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
    #endif
                else if (action == F("ToggleTRK"))
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
                else if (action == F("FactoryReset"))
                {
                    mount.clearConfiguration();
                    Serial.println(F("Mount reset, EEPROM erased."));
                    _currentMenu->display();
                }
                else if (action = F("PassthroughCmd"))
                {
                    Serial.print(F("Enter LX200-OAT command to send to mount: "));
                    TestMenu::setMenuState(testMenuState_t::WAITING_ON_COMMAND);
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
    char buffer[64];
    snprintf_P(buffer,
               sizeof(buffer),
               (const char *) F("  RA: %8ld%c   ALT: %8ld%c   TRK: %8ld%c"),
               mount.getCurrentStepperPosition(RA_STEPS),
               mount.isAxisRunning(RA_STEPS) ? '^' : ' ',
               mount.getCurrentStepperPosition(ALTITUDE_STEPS),
               mount.isAxisRunning(ALTITUDE_STEPS) ? '^' : ' ',
               mount.getCurrentStepperPosition(TRACKING),
               mount.isSlewingTRK() ? '^' : ' ');
    Serial.println(buffer);

    snprintf_P(buffer,
               sizeof(buffer),
               (const char *) F(" DEC: %8ld%c    AZ: %8ld%c   FOC: %8ld%c"),
               mount.getCurrentStepperPosition(DEC_STEPS),
               mount.isAxisRunning(DEC_STEPS) ? '^' : ' ',
               mount.getCurrentStepperPosition(AZIMUTH_STEPS),
               mount.isAxisRunning(AZIMUTH_STEPS) ? '^' : ' ',
               mount.getCurrentStepperPosition(FOCUS_STEPS),
               mount.isAxisRunning(FOCUS_STEPS) ? '^' : ' ');
    Serial.println(buffer);
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
        Serial.print(F("---------------- "));
        Serial.print(freeMemory());
        Serial.println(F(" bytes -------------"));
        displayStepperPos();
        Serial.println(F("-----------------------------------------"));
        Serial.print("  ");
        Serial.print(_name);
        Serial.println(F(" Menu"));
        Serial.println(F("--------------------------"));
    }

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

    #if AZ_STEPPER_TYPE != STEPPER_TYPE_NONE
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
    #endif

    #if ALT_STEPPER_TYPE != STEPPER_TYPE_NONE
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
    #endif

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