#pragma once

#include <SSD1306Wire.h>
#include "Version.h"
#include "fonts128x64.h"
#include "Mount.hpp"
#include "InfoDisplayRender.hpp"

// This class renders the mount status to a 128x64 pixel display controlled by a SSD1306 chip.
class SDD1306OLED128x64 : public InfoDisplayRender
{
#ifdef OAM
    const float bottomDEC = -170.0f;
    const float rangeDEC  = 340.0;
#else
    const float bottomDEC = -90.0f;
    const float rangeDEC  = 270.0;
#endif
    const float leftRA       = -7.0f;
    const float rightRA      = 7.0f;
    const int _leftEdgeMount = 83;  // Must be odd. This is the location of the left edge of the mount square
    const int _topEdgeMount  = 12;

    SSD1306Wire *display;
    int _sizeMount;
    long _lastNumCmds;
    long _lastUpdate;
    bool _consoleMode;
    String _textList[6];  // At most 6 lines of text in console mode
    int _curLine;

  public:
    SDD1306OLED128x64(byte addr, int sda, int scl) : InfoDisplayRender()
    {
        display      = new SSD1306Wire(addr, sda, scl, GEOMETRY_128_64);
        _sizeMount   = 128 - _leftEdgeMount;
        _consoleMode = true;
        _curLine     = 0;
        for (int i = 0; i < 6; i++)
        {
            _textList[i] = "";
        }
    }

    // Initialize the display
    virtual void init()
    {
        display->init();
        display->clear();
        display->displayOn();
    };

    // Build the display from the mount
    virtual void render(Mount *mount)
    {
        display->clear();
        if (_consoleMode)
        {
            display->setColor(WHITE);

            // Logo on the left
            display->setFont(OATLogo);
            display->drawString(0, 0, "!");

            // Name on the right
            display->setFont(Bitmap5x7);
#ifdef OAM
            display->drawString(32, 6, "OpenAstroMount");
#else
            display->drawString(32, 6, "OpenAstroTracker");
#endif

            // Other lines
            int y = 21;
            display->setFont(Bitmap3x5);
            for (int i = 0; i < 6; i++)
            {
                if (_textList[i].length() != 0)
                {
                    display->drawString(0, y, _textList[i]);
                }
                y += 7;
            }
        }
        else
        {
            drawIndicators(mount);
        }
        display->display();
    };

    virtual void setConsoleMode(bool active)
    {
        _consoleMode = active;
    };

    virtual int addConsoleText(String text, bool tinyFont)
    {
        _textList[_curLine++] = text;
        render(NULL);
        return _curLine - 1;
    };

    virtual void updateConsoleText(int line, String text)
    {
        _textList[line] = text;
        render(NULL);
    };

    // Draw all the indicators on screen
    void drawIndicators(Mount *mount)
    {
        display->setFont(Bitmap5x7);
        display->setColor(WHITE);

        drawStepperStates(mount);
        int ra, dec;
        // If a slew is in progress, we don't display the safe time, version, and
        // comms indicator, since the progress bar takes up the same space
        if (mount->getStepperProgress(ra, dec))
        {
            drawProgressBar(ra, dec);
        }
        else
        {
            drawVersion(mount);
            drawSafeTime(mount);
        }
        drawCoordinates(mount);
        drawMountPosition(mount);
        drawStatus(mount);
    }

    // Display two 2-pixel high progress bar in the last 4 lines of the display
    void drawProgressBar(int percRA, int percDEC)
    {
        display->setColor(WHITE);
        display->drawVerticalLine(127, 60, 4);
        int raWidth = round(1.28f * percRA);
        display->fillRect(0, 60, raWidth, 2);
        int decWidth = round(1.28f * percDEC);
        display->fillRect(0, 62, decWidth, 2);
    }

    // Display a rectangle with the stepper label in it
    void drawStepperState(const char *name, bool active, int xoff, int width, int textOffX = 0)
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
        display->drawString(xoff + 2 + textOffX, 2, name);
    }

    // Display all the configured stepper status rectangles
    // Focuser is currently not supported, but could be added here, if possible.
    void drawStepperStates(Mount *mount)
    {
        display->setFont(Bitmap5x7);
        drawStepperState("RA", mount->isAxisRunning(RA_STEPS), 0, 15);
        drawStepperState("DEC", mount->isAxisRunning(DEC_STEPS), 16, 21);
#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
        drawStepperState("ALT", mount->isRunningALT(), 38, 21);
#endif
#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
        drawStepperState("AZ", mount->isRunningAZ(), 60, 16);
#endif
        drawStepperState("GDE", mount->isGuiding(), 83, 21);
        drawStepperState("TRK", mount->isSlewingTRK(), 105, 23, 1);
    }

    // Display the firmware version and communication activity marker in the bottom right corner
    void drawVersion(Mount *mount)
    {
        display->setColor(WHITE);
        display->setFont(Bitmap3x5);
        int len = display->getStringWidth(VERSION);
        // Leave 8 pixels for the indicator
        display->drawString(128 - len - 8, 59, VERSION);
        long recvdCmds = mount->getNumCommandsReceived();
        // If we have received any commands since the last display, draw the marker.
        if (recvdCmds != _lastNumCmds)
        {
            display->setFont(CommSymbols);
            display->drawString(121, 59, "C");
            _lastNumCmds = recvdCmds;
        }
    }

    // Draw the given coordinate string at the given point
    void drawCoordinate(int x, int y, const char *coord)
    {
        char achCoord[30];
        char *n = achCoord;
        // Since this is not a full font, remap the supported letters to the right character
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

    // Draw the mounts celestial RA and DEC coordinates
    void drawCoordinates(Mount *mount)
    {
        String rc = mount->RAString(LCD_STRING | CURRENT_STRING);
        String dc = mount->DECString(LCD_STRING | CURRENT_STRING);
        drawCoordinate(8, 24, rc.c_str());
        drawCoordinate(0, 42, dc.c_str());
    }

    int xc(float ra)
    {
        float rangeRA = rightRA - leftRA;
        int x         = (int) round(1.0f * (_sizeMount - 2) * ((ra - leftRA) / rangeRA));
        return (_leftEdgeMount + x + 1);
    }

    int yc(float dec)
    {
        int y = (int) round(1.0f * (_sizeMount - 2) * ((dec - bottomDEC) / rangeDEC));
        return (_topEdgeMount + _sizeMount - y - 1);
    }

    // Draw the rectangle with the current and target positions
    void drawMountPosition(Mount *mount)
    {
        float raStepsPerDeg  = mount->getStepsPerDegree(StepperAxis::RA_STEPS);
        float decStepsPerDeg = mount->getStepsPerDegree(StepperAxis::DEC_STEPS);

        // int half              = (_sizeMount - 1) / 2;
        // long raPos            = 0;
        // long decPos           = 0;
        // DayTime raTarget      = mount->targetRA();
        // Declination decTarget = mount->targetDEC();

        display->setColor(WHITE);
        display->drawRect(_leftEdgeMount, _topEdgeMount, _sizeMount, _sizeMount);
        int yZero = yc(0.0f);
        int xZero = xc(0.0f);

        for (int p = 0; p < _sizeMount - 1; p += 2)
        {
            display->setPixel(_leftEdgeMount + 1 + p, yZero);
            display->setPixel(xZero, _topEdgeMount + 1 + p);
        }

        float trkSteps = 1.0f * mount->getCurrentStepperPosition(TRACKING) / (1.0f * RA_TRACKING_MICROSTEPPING / RA_SLEW_MICROSTEPPING);
        long raSteps   = mount->getCurrentStepperPosition(StepperAxis::RA_STEPS);
        float raHours  = (trkSteps + raSteps) / raStepsPerDeg / 15.0f;

        long decSteps    = mount->getCurrentStepperPosition(StepperAxis::DEC_STEPS);
        float decDegrees = decSteps / decStepsPerDeg;

        int xMark = xc(raHours);
        display->setPixel(xMark, _topEdgeMount + _sizeMount - 3);
        display->setPixel(xMark - 1, _topEdgeMount + _sizeMount - 2);
        display->setPixel(xMark, _topEdgeMount + _sizeMount - 2);
        display->setPixel(xMark + 1, _topEdgeMount + _sizeMount - 2);

        display->setPixel(xc(-6.0f), _topEdgeMount + _sizeMount - 2);
        display->setPixel(xc(-3.0f), _topEdgeMount + _sizeMount - 2);
        display->setPixel(xc(3.0f), _topEdgeMount + _sizeMount - 2);
        display->setPixel(xc(6.0f), _topEdgeMount + _sizeMount - 2);

        int yMark = yc(decDegrees);
        display->setPixel(_leftEdgeMount + _sizeMount - 3, yMark);
        display->setPixel(_leftEdgeMount + _sizeMount - 2, yMark - 1);
        display->setPixel(_leftEdgeMount + _sizeMount - 2, yMark);
        display->setPixel(_leftEdgeMount + _sizeMount - 2, yMark + 1);

        display->setPixel(_leftEdgeMount + _sizeMount - 2, yc(-45.0f));
        display->setPixel(_leftEdgeMount + _sizeMount - 2, yc(45.0f));
        display->setPixel(_leftEdgeMount + _sizeMount - 2, yc(90.0f));
        display->setPixel(_leftEdgeMount + _sizeMount - 2, yc(135.0f));

        // LOG(DEBUG_DISPLAY, "DEC Y0: %f, Y90 %f", yZero, y90);
        // LOG(DEBUG_DISPLAY, "DEC Deg: %l  -> %f (ratio: %f)", decSteps, decDegrees, (decDegrees - bottomDEC) / rangeDEC);
        // LOG(DEBUG_DISPLAY, "DEC Pix: %d / %d", d, _sizeMount - 2);

        // mount->calculateStepperPositions(raTarget.getTotalHours(), decTarget.getTotalDegrees(), raPos, decPos);

        // int rt = xc(raPos);
        // display->drawVerticalLine(_leftEdgeMount + 1 + rt, _topEdgeMount + _sizeMount - 5, 4);

        // int dt = yc(1.0f * decPos / decStepsPerDeg);
        // display->drawHorizontalLine(_leftEdgeMount + _sizeMount - 5, dt, 4);
    }

    // Display the tiem left before tracking hits the limit
    void drawSafeTime(Mount *mount)
    {
        char achTemp[10];
        float hoursLeft = mount->checkRALimit();
        DayTime dt(hoursLeft);
        display->setColor(WHITE);
        display->setFont(Bitmap3x5);
        display->drawString(0, 59, dt.formatString(achTemp, "SAFE: {d}:{m}"));
    }

    // Display the mount status string
    void drawStatus(Mount *mount)
    {
        display->setColor(WHITE);
        display->setFont(Bitmap5x7);
        String state = mount->getStatusStateString();
        state.toUpperCase();
        display->drawString(4, 14, state.c_str());
        if (millis() - _lastUpdate > 1000)
        {
            display->drawVerticalLine(0, 15, 5);
            display->drawVerticalLine(1, 16, 3);
            display->setPixel(2, 17);
            _lastUpdate = millis();
        }
    }
};
