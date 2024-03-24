#pragma once

#include <SSD1306Wire.h>
#include "Version.h"
#include "fonts128x64.h"
#include "Mount.hpp"

class SDD1306OLED128x64 : public InfoDisplayRender
{
    SSD1306Wire *display;

  public:
    SDD1306OLED128x64(byte addr, int sda, int scl) : InfoDisplayRender()
    {
        display = new SSD1306Wire(addr, sda, scl, GEOMETRY_128_64);
    }

    virtual void init()
    {
        display->init();
        display->clear();
        display->displayOn();
    };

    virtual void render(Mount *mount)
    {
        display->clear();
        drawIndicators(mount);
        display->display();
    };

    void drawIndicators(Mount *mount)
    {
        drawStepperStates(mount);
        if (false /*slewinprogress*/)
        {
            int percent=34;
            drawProgressBar(percent);
        }
        else
        {
            drawVersion();
        }
        drawCoordinates(mount);
        drawMountPosition(mount);
        drawStatus(mount);
    }

    void drawProgressBar(int percent)
    {
        display->setColor(WHITE);
        display->drawRect(0, 60, 128, 4);
        int width = round(126.0f * percent * 0.01f);
        display->fillRect(1, 61, width, 2);
    }

    void drawStepperState(const char *name, bool active, int xoff, int width)
    {
        display->setColor(WHITE);
        if (active)
        {
            display->fillRect(xoff, 0, width, 11);
        }
        else
        {
            display->drawRect(xoff, 0, width, 11);
        }
        display->setColor(INVERSE);
        display->drawString(xoff + 2, 2, name);
    }

    void drawStepperStates(Mount* mount)
    {
        display->setFont(Bitmap5x7);
        drawStepperState("RA", mount->isAxisRunning(RA_STEPS), 0, 15);
        drawStepperState("DEC", mount->isAxisRunning(DEC_STEPS), 14, 21);
#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
        drawStepperState("ALT", mount->isRunningALT(), 34, 21);
#endif
#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
        drawStepperState("AZ", mount->isRunningAZ(), 54, 16);
#endif
#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
        drawStepperState("FC", mount - isRunningFocus(), 69, 15);
#endif
        drawStepperState("GDE", mount->isGuiding(), 85, 21);
        drawStepperState("TRK", mount->isSlewingTRK(), 107, 21);
    }

    void drawVersion()
    {
        char achVersion[10];
        char *n = achVersion;
        for (const char *p = VERSION; *p != 0; p++)
        {
            if ((*p == 'V') || (*p == 'v'))
            {
                *n++ = '/';
            }
            else
            {
                *n++ = *p;
            }
        }
        *n = 0;
        display->setColor(WHITE);
        display->setFont(Bitmap3x5);
        int len = display->getStringWidth(achVersion);
        display->drawString(128 - len, 59, achVersion);
    }

    void drawCoordinate(int x, int y, const char *coord)
    {
        char achCoord[30];
        char *n = achCoord;
        for (const char *p = coord; *p != 0; p++)
        {
            switch (*p)
            {
                case 'R':
                    *n = '!';
                    break;
                case 'A':
                    *n = '&';
                    break;
                case 'D':
                    *n = '#';
                    break;
                case 'E':
                    *n = '$';
                    break;
                case 'C':
                    *n = '%';
                    break;
                case 'h':
                    *n = '<';
                    break;
                case 'm':
                    *n = ';';
                    break;
                case 's':
                    *n = '=';
                    break;
                case '@':
                    *n = '(';
                    break;
                default:
                    *n = *p;
                    break;
            }
            n++;
        }
        *n = 0;
        display->setFont(Bitmap7x15);
        display->setColor(WHITE);
        display->drawString(x, y, achCoord);
    }

    void drawCoordinates(Mount *mount)
    {
        String rc = mount->RAString(LCD_STRING | CURRENT_STRING);
        String dc = mount->DECString(LCD_STRING | CURRENT_STRING);
        drawCoordinate(0, 26, rc.c_str());
        drawCoordinate(0, 44, dc.c_str());
    }

    void drawMountPosition(Mount *mount)
    {
        float leftRA    = -7.0f;
        float rightRA   = 7.0f;
        float rangeRA   = rightRA - leftRA;
        // float topDEC    = 90.0f;
        float bottomDEC = -90.0f;
        float rangeDEC  = 180.0;

        int top               = 12;
        int left              = 83;  // Must be odd
        int size              = 128 - left;
        int half              = (size - 1) / 2;
        DayTime raPos         = mount->currentRA();
        Declination decPos    = mount->currentDEC();
        DayTime raTarget      = mount->targetRA();
        Declination decTarget = mount->targetDEC();

        display->setColor(WHITE);
        display->drawRect(left, top, size, size);
        for (int p = 0; p < size - 1; p += 2)
        {
            display->setPixel(left + 1 + p, top + half);
            display->setPixel(left + half, top + 1 + p);
        }

        int r = (size - 2) * (raPos.getTotalHours() - leftRA) / rangeRA;
        display->setPixel(left + 1 + r, top + size - 3);
        display->setPixel(left + r, top + size - 2);
        display->setPixel(left + 1 + r, top + size - 2);
        display->setPixel(left + 2 + r, top + size - 2);

        int rt = (size - 2) * (raTarget.getTotalHours() - leftRA) / rangeRA;
        display->drawVerticalLine(left + 1 + rt, top + size - 5, 4);

        int d = (size - 2) * (decPos.getTotalDegrees() - bottomDEC) / rangeDEC;
        display->setPixel(left + size - 3, top + 1 + d);
        display->setPixel(left + size - 2, top + d);
        display->setPixel(left + size - 2, top + 1 + d);
        display->setPixel(left + size - 2, top + 2 + d);

        int dt = (size - 2) * (decTarget.getTotalDegrees() - bottomDEC) / rangeDEC;
        display->drawHorizontalLine(left + size - 5, top + 1 + dt, 4);
    }

    void drawStatus(Mount *mount)
    {
        display->setColor(WHITE);
        display->setFont(RetroGaming7x8);
        String state = mount->getStatusStateString();
        state.toUpperCase();
        display->drawString(0, 14, state.c_str());
    }
};
