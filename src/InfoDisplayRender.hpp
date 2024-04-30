#pragma once
#include <Arduino.h>

class Mount;

// Base class to implement a
class InfoDisplayRender
{
  public:
    InfoDisplayRender() {};

    virtual void init() {};
    virtual void render(Mount *mount) {};
    virtual void setConsoleMode(bool active) {};
    virtual int addConsoleText(String text, bool tinyFont = true);
    virtual void updateConsoleText(int line, String newText);
};
