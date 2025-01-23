#include "../Configuration.hpp"
#include "Utility.hpp"
#include "Mount.hpp"
#include "testmenu.hpp"

extern Mount mount;

TestMenu *TestMenu::_currentMenu     = nullptr;
TestMenuItem *TestMenu::_backItem    = nullptr;
testMenuState_t TestMenu::_menuState = testMenuState_t::CLEAR;

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
    : _level(level), _name(name), _parent(parent), _choices(choices), _numChoices(numChoices), _parentMenu(parentMenu)
{
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
        return "AZ and ALT steppers (AutoPA)";
    }
    if (comp == "AUTO_AZ")
    {
        return "AZ stepper";
    }
    if (comp == "AUTO_ALT")
    {
        return "ALT stepper";
    }
    if (comp == "GYRO")
    {
        return "Digital Level";
    }
    if (comp == "LCD_KEYPAD")
    {
        return "LCD display and keypad";
    }

    if (comp == "LCD_I2C_MCP23008")
    {
        return "LCD display (MCP23008)";
    };
    if (comp == "LCD_I2C_MCP23017")
    {
        return "LCD display (MCP23017)";
    };
    if (comp == "LCD_JOY_I2C_SSD1306")
    {
        return "LCD display (SSD1306) with joystick";
    };

    if (comp == "INFO_I2C_SSD1306_128x64")
    {
        return "Info display (SSD1306)";
    };
    if (comp == "INFO_UNKNOWN")
    {
        return "Info display (unknown type)";
    };

    if (comp == "FOC")
    {
        return "Focuser stepper";
    };

    if (comp == "HSAH")
    {
        return "RA Hall Sensor Auto-Homing";
    };
    if (comp == "HSAV")
    {
        return "DEC Hall Sensor Auto-Homing";
    };

    if (comp == "ENDSW_RA")
    {
        return "End switches on RA";
    };
    if (comp == "ENDSW_DEC")
    {
        return "End switches on DEC";
    };
    if (comp == "ENDSW_RA_DEC")
    {
        return "End switches on RA and DEC";
    };
    return "Unknown component";
}

void TestMenu::listHardware() const
{
    Serial.println("Firmware is configured to support these hardware components:");
    String *hw = splitStringBy(mount.getMountHardwareInfo(), ',');
    String *p  = hw;
    int index  = 0;
    while (p->length() > 0)
    {
        switch (index)
        {
            case 0:
                Serial.print("Board: ");
                Serial.println(*p);
                break;
            case 1:
                Serial.print("RA stepper: ");
                Serial.println(*p);
                break;
            case 2:
                Serial.print("DEC stepper: ");
                Serial.println(*p);
                break;
            default:
                if (!p->startsWith("NO_"))
                {
                    String component = getComponent(*p);
                    Serial.print("Component: ");
                    Serial.println(component);
                }
                break;
        }
        p++;
        index++;
    }
    delete [] hw;
}

void TestMenu::connectDriver(String axisStr)
{
    StepperAxis axis;
    if (axisStr == "RA")
        axis = StepperAxis::RA_STEPS;
    if (axisStr == "DEC")
        axis = StepperAxis::DEC_STEPS;
    if (axisStr == "ALT")
        axis = StepperAxis::ALTITUDE_STEPS;
    if (axisStr == "AZ")
        axis = StepperAxis::AZIMUTH_STEPS;
    if (axisStr == "FOC")
        axis = StepperAxis::FOCUS_STEPS;
    switch (axis)
    {
        case StepperAxis::RA_STEPS:
            {
            }
            break;
        case StepperAxis::DEC_STEPS:
            {
            }
            break;
        case StepperAxis::ALTITUDE_STEPS:
            {
            }
            break;
        case StepperAxis::AZIMUTH_STEPS:
            {
            }
            break;
        case StepperAxis::FOCUS_STEPS:
            {
            }
            break;
        case StepperAxis::RA_AND_DEC_STEPS:
            break;
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
            String cmd    = _choices[i].getAction();
            int sep       = cmd.indexOf(':');
            String verb   = cmd.substring(0, sep);
            String action = cmd.substring(sep + 1);
            if (verb == "Action")
            {
                if (action == "ListHardware")
                {
                    listHardware();
                }
                else if (action.startsWith("Connect-"))
                {
                    connectDriver(action.substring(8));
                }
            }
            _currentMenu->display();
            return;
        }
    }
    Serial.println("Invalid key pressed.");
}

void TestMenu::display() const
{
    Serial.println("");
    Serial.println("");
    if (_level == 0)
    {
        Serial.print("*************** ");
        Serial.print(freeMemory());
        Serial.println(" bytes");
        Serial.print("* ");
        Serial.println(_name);
        Serial.println("**************************");
    }
    else
    {
        Serial.print("--------------- ");
        Serial.print(freeMemory());
        Serial.println(" bytes");
        Serial.print("  ");
        Serial.print(_name);
        Serial.println(" Menu");
        Serial.println("--------------------------");
    }

    Serial.println("Please choose:");
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
    Serial.print("Your choice:");
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