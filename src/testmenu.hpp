#pragma once
#include <Arduino.h>

enum testMenuState_t
{
    CLEAR,
    WAITING_ON_INPUT,
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
    TestMenuItem(String label, String action, TestMenu *subMenu = nullptr);
    void display() const;
    int getKey() const;
    void setKey(int key);
    String getAction() const;
    TestMenu *getSubMenu() const;
};

class TestMenu
{
    int _level;
    String _name;
    String _parent;
    TestMenuItem *_choices;
    int _numChoices;
    TestMenu *_parentMenu;
    static testMenuState_t _menuState;
    static TestMenu *_currentMenu;
    static TestMenuItem *_backItem;

  public:
    TestMenu(int level, String name, String parent, TestMenuItem *choices, int numChoices, TestMenu *parentMenu = nullptr);
    void onKeyPressed(int key);
    void display() const;
    void setParentMenu(TestMenu *parentMenu);
    static TestMenu *getCurrentMenu();
    static testMenuState_t getMenuState();
    static void setMenuState(testMenuState_t state);

    void listHardware() const;
    void connectDriver(String axisStr);
};
