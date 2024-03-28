#pragma once

#include <SSD1306Wire.h>
#include "Version.h"
#include "fonts128x64.h"
#include "Mount.hpp"

class SDD1306OLED128x64 : public InfoDisplayRender
{
    SSD1306Wire *display;
    int _leftEdgeMount = 83;  // Must be odd
    int _sizeMount;


  public:
    SDD1306OLED128x64(byte addr, int sda, int scl) : InfoDisplayRender()
    {
        display = new SSD1306Wire(addr, sda, scl, GEOMETRY_128_64);
        _sizeMount     = 128 - _leftEdgeMount;

    }

    virtual void init()
    {
        display->init();
        // Wire.setClock(700000);
        display->clear();
        display->displayOn();
    };

    virtual void render(Mount *mount)
    {
        //long start=millis();
        display->clear();
        //long seg1=millis();
        drawIndicators(mount);
        //long seg2=millis();
        display->display();
        //long end = millis();
        //LOG(DEBUG_INFO, "Clr: %l  Draw: %l  Disp: %l  Total: %ld",seg1-start,seg2-seg1,end-seg2,end-start);
    };

    void drawIndicators(Mount *mount)
    {
        display->setFont(Bitmap5x7);
         display->setColor(WHITE);

        drawStepperStates(mount);
        if (false /*slewinprogress*/)
        {
            int percent=34;
            drawProgressBar(percent);
        }
        else
        {
            drawVersion(mount);
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

    void drawVersion(Mount* mount)
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
        display->drawString(_leftEdgeMount, 59, String(mount->getNumCommandsReceived()).c_str());
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
        float raStepsPerDeg = mount->getStepsPerDegree(StepperAxis::RA_STEPS);
        float leftRA    = -7.0f*raStepsPerDeg*15;
        float rightRA   = 7.0f*raStepsPerDeg*15;
        float rangeRA   = rightRA - leftRA;
        // float topDEC    = 90.0f;
        float decStepsPerDeg = mount->getStepsPerDegree(StepperAxis::DEC_STEPS);
        float bottomDEC = 90.0f*decStepsPerDeg;
        float rangeDEC  = -180.0*decStepsPerDeg;

        int top               = 12;
        int half              = (_sizeMount - 1) / 2;
        long  raPos=0;
        long decPos=0;
        DayTime raTarget      = mount->targetRA();
        Declination decTarget = mount->targetDEC();

        display->setColor(WHITE);
        display->drawRect(_leftEdgeMount, top, _sizeMount, _sizeMount);
        for (int p = 0; p < _sizeMount - 1; p += 2)
        {
            display->setPixel(_leftEdgeMount + 1 + p, top + half);
            display->setPixel(_leftEdgeMount + half, top + 1 + p);
        }

        long raSteps = mount->getCurrentStepperPosition(StepperAxis::RA_STEPS);
        long decSteps = mount->getCurrentStepperPosition(StepperAxis::DEC_STEPS);
       
        int r = (_sizeMount - 2) * (raSteps - leftRA) / rangeRA;
        display->setPixel(_leftEdgeMount + 1 + r, top + _sizeMount - 3);
        display->setPixel(_leftEdgeMount + r, top + _sizeMount - 2);
        display->setPixel(_leftEdgeMount + 1 + r, top + _sizeMount - 2);
        display->setPixel(_leftEdgeMount + 2 + r, top + _sizeMount - 2);

        int d = (_sizeMount - 2) * (decSteps - bottomDEC) / rangeDEC;
        display->setPixel(_leftEdgeMount + _sizeMount - 3, top + 1 + d);
        display->setPixel(_leftEdgeMount + _sizeMount - 2, top + d);
        display->setPixel(_leftEdgeMount + _sizeMount - 2, top + 1 + d);
        display->setPixel(_leftEdgeMount + _sizeMount - 2, top + 2 + d);
  
        mount->calculateStepperPositions(raTarget.getTotalHours(), decTarget.getTotalDegrees(), raPos, decPos);

        int rt = (_sizeMount - 2) * (raPos - leftRA) / rangeRA;
        display->drawVerticalLine(_leftEdgeMount + 1 + rt, top + _sizeMount - 5, 4);

        int dt = (_sizeMount - 2) * (decPos - bottomDEC) / rangeDEC;
        display->drawHorizontalLine(_leftEdgeMount + _sizeMount - 5, top + 1 + dt, 4);
    }

    void drawStatus(Mount *mount)
    {
        display->setColor(WHITE);
        display->setFont(Bitmap5x7);
        String state = mount->getStatusStateString();
        state.toUpperCase();
        display->drawString(0, 14, state.c_str());
        display->drawString(36,1,String(millis()).c_str());
    }
};
