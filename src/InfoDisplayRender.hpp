#pragma once
#include <Arduino.h>
#include "OLEDDisplay.h"
#include "fonts128x64.h"
#include "../Configuration.hpp"

// Base class to implement a
class InfoDisplayRender
{
    const static int MAX_CONSOLE_LINES     = 16;  // Maximum number of lines of text to buffer in console mode
    const static int DISPLAY_CONSOLE_LINES = 6;   // Number of lines to display in console mode

  protected:
    OLEDDisplay *_display;
    long _lastNumCmds;
    bool _consoleMode;
    String _textList[MAX_CONSOLE_LINES];
    int _curLine;

  public:
    InfoDisplayRender()
    {
        _consoleMode = true;
        _curLine     = 0;
        for (int i = 0; i < MAX_CONSOLE_LINES; i++)
        {
            _textList[i] = "";
        }
    };

    virtual void init()
    {
        _display->init();
        #if (INFO_DISPLAY_UPSIDE_DOWN == 1)
        _display->flipScreenVertically();
        #endif
        #if (INFO_DISPLAY_MIRRORED == 1)
        _display->mirrorScreen();
        #endif
        _display->clear();
        _display->displayOn();
    };

    OLEDDisplay *getDisplayDevice()
    {
        return _display;
    };

    virtual void renderScreen(void *context) = 0;

    // Build the display from the mount
    virtual void render(void (*drawContentFunction)(void *))
    {
        _display->clear();
        if (drawContentFunction)
        {
            drawContentFunction(this);
        }

        if (_consoleMode)
        {
            // Console lines
            int y = 21;
            _display->setFont(Bitmap3x5);
            // Start 6 lines back from current line and display next 6 lines
            int indexStart = max(0, _curLine - DISPLAY_CONSOLE_LINES);
            int indexEnd   = min(indexStart + DISPLAY_CONSOLE_LINES, MAX_CONSOLE_LINES);
            for (int i = indexStart; i < indexEnd; i++)
            {
                if (_textList[i].length() != 0)
                {
                    String text = _textList[i];
                    text.toUpperCase();
                    _display->drawString(0, y, text);
                }
                y += 7;
            }
        }
        else
        {
        }

        _display->display();
    };

    virtual void setConsoleMode(bool active)
    {
        _consoleMode = active;
    };

    virtual int addConsoleText(const String &text, bool tinyFont = true)
    {
        int returnIndex = 0;
        if (_curLine > MAX_CONSOLE_LINES - 1)
        {
            for (int i = 0; i < MAX_CONSOLE_LINES - 1; i++)
            {
                _textList[i] = _textList[i + 1];
            }
            _curLine            = MAX_CONSOLE_LINES - 1;
            _textList[_curLine] = text;
            returnIndex         = _curLine;
        }
        else
        {
            returnIndex           = _curLine;
            _textList[_curLine++] = text;
        }
        render(nullptr);
        return returnIndex;
    };

    virtual void updateConsoleText(int line, String text)
    {
        if (line >= 0 && line < MAX_CONSOLE_LINES)
        {
            _textList[line] = text;
        }
        render(nullptr);
    };
};
