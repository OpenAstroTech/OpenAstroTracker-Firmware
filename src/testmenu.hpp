#pragma once
#include <Arduino.h>

#if TEST_VERIFY_MODE == 1

enum menuText_t
{
    MENU_BACK,
    MENU_CONNECT_RA,
    MENU_CONNECT_DEC,
    MENU_CONNECT_ALT,
    MENU_CONNECT_AZ,
    MENU_CONNECT_FOC,
    MENU_PRIMARY_RA_CW,
    MENU_PRIMARY_RA_CCW,
    MENU_PRIMARY_DEC_UP,
    MENU_PRIMARY_DEC_DOWN,
    MENU_PRIMARY_SET_HOME,
    MENU_PRIMARY_GO_HOME,
    MENU_TOGGLE_TRK,
    MENU_SECONDARY_RATE_1,
    MENU_SECONDARY_RATE_2,
    MENU_SECONDARY_RATE_3,
    MENU_SECONDARY_RATE_4,
    MENU_SECONDARY_RATE_5,
    MENU_SECONDARY_ALT_UP,
    MENU_SECONDARY_ALT_DOWN,
    MENU_SECONDARY_AZ_LEFT,
    MENU_SECONDARY_AZ_RIGHT,
    MENU_FACTORY_RESET,
    MENU_PASSTHROUGH_COMMAND,
    MENU_MAIN_LIST_HARDWARE,
    MENU_MAIN_CONNECT_DRIVERS,
    MENU_MAIN_PRIMARY_AXIS_MOVES,
    MENU_MAIN_SECONDARY_AXIS_MOVES,
};

enum testMenuState_t
{
    CLEAR,
    WAITING_ON_INPUT,
    WAITING_ON_COMMAND,
};

// Flag as to what to display to terminal
enum testMenuInternalState
{
    IDLE        = 0,
    DISPLAY_RA  = 1 << 0,
    DISPLAY_DEC = 1 << 1,
    DISPLAY_AZ  = 1 << 2,
    DISPLAY_ALT = 1 << 3,
};

class TestMenu;

class TestMenuItem
{
    int _key;
    String _label;
    String _action;
    TestMenu *_subMenu;
    bool _isSubMenu;

  public:
    TestMenuItem(menuText_t labelId, TestMenu *subMenu = nullptr);
    void display() const;
    int getKey() const;
    void setKey(int key);
    String getAction() const;
    TestMenu *getSubMenu() const;
};

class TestMenu
{
    int _level;
    unsigned long _lastTick;
    String _name;
    String _parent;
    TestMenuItem *_choices;
    int _numChoices;
    TestMenu *_parentMenu;
    float _secondaryDistance;

    static long _targetRA;
    static long _startRA;
    static long _targetDEC;
    static long _startDEC;
    static long _startAZ;
    static long _targetAZ;
    static long _startALT;
    static long _targetALT;

    static testMenuState_t _menuState;
    static testMenuInternalState _internalState;
    static TestMenu *_currentMenu;
    static TestMenuItem *_backItem;

  public:
    TestMenu(int level, String name, String parent, TestMenuItem *choices, int numChoices, TestMenu *parentMenu = nullptr);
    void onKeyPressed(int key);
    void onCommandReceived(String s);
    void display() const;
    void displayStepperPos() const;
    void setParentMenu(TestMenu *parentMenu);
    static TestMenu *getCurrentMenu();
    static testMenuState_t getMenuState();
    static void setMenuState(testMenuState_t state);

    void listHardware() const;
    void connectDriver(String axisStr);
    void tick();
};
#endif
