#pragma once
#include "SSD1306Wire.h"
#include "Utility.hpp"
#include "Version.h"
#include "fonts128x64.h"
#include "Mount.hpp"
#include "InfoDisplayRender.hpp"

const float sineSize              = 18.0;
const uint8_t sineTable[] PROGMEM = {0, 22, 44, 66, 87, 108, 128, 146, 164, 180, 195, 209, 221, 231, 240, 246, 251, 254, 255, 255};

// This class renders the mount status to a 128x64 pixel display controlled by a SSD1306 chip.
class SDD1306OLED128x64 : public InfoDisplayRender
{
#ifdef OAM
    const float bottomDEC = -180.0f;
    const float rangeDEC  = 360.0;
#else
    const float bottomDEC = -90.0f;
    const float rangeDEC  = 270.0;
#endif
    const float leftRA  = -6.0f;
    const float rightRA = 6.0f;

    const int _leftEdgeMount = 69;  // x pos of start of scale
    const int _raSize        = 41;  // width of ra scale
    const int _raScalePos    = 51;  // Y pos of ra scale dotted line

    const int _topEdgeMount = 14;
    const int _decSize      = 43;
    const int _decScalePos  = 115;

    const int yMaxStatus = 63 - 11;

    SSD1306Wire *display;
    int _sizeMount;
    int _yStatus;
    int _dirStatus;
    char _commLetter;
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
        _yStatus     = 0;
        _dirStatus   = 1;
        _commLetter  = ' ';
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
            display->drawString(32, 6, F("OpenAstroMount"));
#else
            display->drawString(32, 6, F("OpenAstroTracker"));
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

    // Display the tiem left before tracking hits the limit
    void drawTime(Mount *mount, String label, const DayTime &time)
    {
        char achTemp[24];
        display->setColor(WHITE);
        display->setFont(Bitmap3x5);
        sprintf(achTemp, "%s%02d:%02d", label.c_str(), time.getHours(), time.getMinutes());
        display->drawString(55, 59, achTemp);
    }

    // Draw all the indicators on screen
    void drawIndicators(Mount *mount)
    {
        char scratchBuffer[24];
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
            long timeSecs = millis() / 2000;  // Change every 2 secs
            int index     = timeSecs % 5;     // Cycle through multiple data displays

            display->setFont(CommSymbols);
            display->drawString(11, 59, F("L"));  // Memory chip icon
            display->setFont(Bitmap3x5);
            long availMem = freeMemory();
            if (availMem > 9999)
            {
                display->drawString(20, 59, String(availMem / 1024) + "K");
            }
            else
            {
                display->drawString(20, 59, String(availMem));
            }

            switch (index)
            {
                case 0:
                    {
                        float hoursLeft = mount->checkRALimit();
                        DayTime dt(hoursLeft);
                        drawTime(mount, F("REM "), dt);
                    }
                    break;
                case 1:
                    {
                        drawTime(mount, F("LST "), mount->calculateLst());
                    }
                    break;
                case 2:
                    {
                        long now      = millis();
                        long msPerDay = 60L * 60 * 24 * 1000;
                        int days      = (int) (now / msPerDay);
                        now -= days * msPerDay;
                        DayTime elapsed(1.0 * now / (1000.0 * 3600.0));
                        drawTime(mount, F("UPT "), elapsed);
                    }
                    break;
                case 3:
                    {
                        display->drawString(55, 59, (String(F(" FW ")) + String(VERSION)).c_str());
                    }
                    break;
                case 4:
                    {
                        float lat          = fabsf(mount->latitude().getTotalHours());
                        float lng          = fabsf(mount->longitude().getTotalHours());
                        const char dirLat  = (mount->latitude().getTotalHours() < 0) ? 'S' : 'N';
                        const char dirLong = (mount->longitude().getTotalHours() < 0) ? 'W' : 'E';
                        sprintf(scratchBuffer, "LOC %s%c %s%c", String(lat, 0).c_str(), dirLat, String(lng, 0).c_str(), dirLong);
                        display->drawString(55, 59, scratchBuffer);
                    }
                    break;
            }
            drawCommunicationStatus(mount);
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
    void drawStepperState(String name, bool active, int xoff, int width, int textOffX = 0)
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
        drawStepperState(F("RA"), mount->isAxisRunning(RA_STEPS), 0, 15);
        drawStepperState(F("DEC"), mount->isAxisRunning(DEC_STEPS), 16, 21);
#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
        drawStepperState(F("ALT"), mount->isRunningALT(), 38, 21);
#endif
#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
        drawStepperState(F("AZ"), mount->isRunningAZ(), 60, 16);
#endif
        drawStepperState(F("GDE"), mount->isGuiding(), 83, 21);
        drawStepperState(F("TRK"), mount->isSlewingTRK(), 105, 23, 1);
    }

    void drawCommunicationStatus(Mount *mount)
    {
        long recvdCmds = mount->getNumCommandsReceived();
        // If we have received any commands since the last display, draw the marker.

        if (_commLetter != ' ')
        {
            display->setFont(CommSymbols);
            display->drawString(1, 59, String(_commLetter));
            _commLetter++;
            if (_commLetter == 'G')  // Past last communication animation frame (F)
            {
                _commLetter = ' ';
            }
        }
        else if (recvdCmds != _lastNumCmds)
        {
            _commLetter  = 'C';  // First communication animation frame
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

    // Map the given RA coordinate to the pixel position on the display
    int xRAPixel(float ra)
    {
        float rangeRA = rightRA - leftRA;
        int x         = 4 + (int) round(1.0f * (_raSize - 9) * ((ra - leftRA) / rangeRA));
        return (_leftEdgeMount + x);
    }

    // Map the given DEC coordinate to the pixel position on the display
    int yDECPixel(float dec)
    {
        int y = (int) round(1.0f * (_decSize) * ((dec - bottomDEC) / rangeDEC));
        return (_topEdgeMount + _decSize - y);
    }

    // Draw the rectangle with the current and target positions
    void drawMountPosition(Mount *mount)
    {
        display->setColor(WHITE);
        display->setFont(Bitmap3x5);

        // DEC tickmarks
        for (int p = _topEdgeMount; p <= _topEdgeMount + _decSize; p += 2)
        {
            display->setPixel(_decScalePos, p);
        }
#ifdef OAM
        display->drawHorizontalLine(_decScalePos - 1, yDECPixel(-180.0), 2);
#endif
        display->drawHorizontalLine(_decScalePos - 1, yDECPixel(-90.0), 2);
        display->drawHorizontalLine(_decScalePos - 1, yDECPixel(0.0), 2);
        display->drawHorizontalLine(_decScalePos - 1, yDECPixel(90.0), 2);
        display->drawHorizontalLine(_decScalePos - 1, yDECPixel(180.0), 2);
// DEC tickmark labels
#ifdef OAM
        display->drawString(_decScalePos + 6, yDECPixel(-180.0f) - 2, F("180"));
        display->drawHorizontalLine(_decScalePos + 3, yDECPixel(-180.0), 2);  // Smaller minus sign
#endif
        display->drawString(_decScalePos + 6, yDECPixel(-90.0f) - 2, F("90"));
        display->drawHorizontalLine(_decScalePos + 3, yDECPixel(-90.0), 2);  // Smaller minus sign
        display->drawString(_decScalePos + 3, yDECPixel(0.0f) - 2, "0");
        display->drawString(_decScalePos + 3, yDECPixel(90.0f) - 2, F("90"));
        display->drawString(_decScalePos + 3, yDECPixel(180.0f) - 2, F("180"));

        // DEC Pos Marker
        float decStepsPerDeg = mount->getStepsPerDegree(StepperAxis::DEC_STEPS);
        long decSteps        = mount->getCurrentStepperPosition(StepperAxis::DEC_STEPS);
        float decDegrees     = decSteps / decStepsPerDeg;
        int yMark            = yDECPixel(decDegrees);
        display->setPixel(_decScalePos - 2, yMark);
        display->drawVerticalLine(_decScalePos - 3, yMark - 1, 3);
        display->drawVerticalLine(_decScalePos - 4, yMark - 2, 5);

        // RA tickmarks
        for (int p = _leftEdgeMount; p <= _leftEdgeMount + _raSize; p += 2)
        {
            display->setPixel(p, _raScalePos);
        }
        display->drawVerticalLine(xRAPixel(-6.0f), _raScalePos - 1, 2);
        display->drawVerticalLine(xRAPixel(-3.0f), _raScalePos - 1, 2);
        display->drawVerticalLine(xRAPixel(0.0f), _raScalePos - 1, 2);
        display->drawVerticalLine(xRAPixel(3.0f), _raScalePos - 1, 2);
        display->drawVerticalLine(xRAPixel(6.0f), _raScalePos - 1, 2);

        // RA tickmark labels
        display->drawString(xRAPixel(-6.0f) - 1, _raScalePos + 2, "6");
        display->drawHorizontalLine(xRAPixel(-6.0f) - 4, _raScalePos + 2 + 2, 2);  // Smaller minus sign
        display->drawString(xRAPixel(-3.0f) - 1, _raScalePos + 2, "3");
        display->drawHorizontalLine(xRAPixel(-3.0f) - 4, _raScalePos + 2 + 2, 2);  // Smaller minus sign
        display->drawString(xRAPixel(0.0f) - 1, _raScalePos + 2, "0");
        display->drawString(xRAPixel(3.0f) - 1, _raScalePos + 2, "3");
        display->drawString(xRAPixel(6.0f) - 1, _raScalePos + 2, "6");

        float raStepsPerDeg = mount->getStepsPerDegree(StepperAxis::RA_STEPS);
        float trkSteps = 1.0f * mount->getCurrentStepperPosition(TRACKING) / (1.0f * RA_TRACKING_MICROSTEPPING / RA_SLEW_MICROSTEPPING);
        long raSteps   = mount->getCurrentStepperPosition(StepperAxis::RA_STEPS);
        float raHours  = (trkSteps + raSteps) / raStepsPerDeg / 15.0f;

        // RA Position Marker
        int xMark = xRAPixel(raHours);
        display->setPixel(xMark, _raScalePos - 2);
        display->drawHorizontalLine(xMark - 1, _raScalePos - 3, 3);
        display->drawHorizontalLine(xMark - 2, _raScalePos - 4, 5);
    }

    // Display the tiem left before tracking hits the limit
    void drawSafeTime(Mount *mount)
    {
        char achTemp[10];
        float hoursLeft = mount->checkRALimit();
        DayTime dt(hoursLeft);
        display->setColor(WHITE);
        display->setFont(CommSymbols);
        display->drawString(48, 59, "M");  // Clock sign
        display->setFont(Bitmap3x5);
        sprintf(achTemp, "%02d:%02d", dt.getHours(), dt.getMinutes());
        display->drawString(55, 59, achTemp);
    }

    float sinLookup(float deg)
    {
        while (deg < 0.0f)
            deg += 360.0f;
        while (deg > 360.0f)
            deg -= 360.0f;

        if (deg <= 90)
        {
            int index = (int) roundf(sineSize * deg / 90.0f);
            return 1.0f * pgm_read_byte(sineTable + index) / 255.0;
        }
        else if (deg <= 180)
        {
            int index = (int) roundf(sineSize * (180.0f - deg) / 90.0f);
            return 1.0f * pgm_read_byte(sineTable + index) / 255.0;
        }
        else if (deg <= 270)
        {
            int index = (int) roundf(sineSize * (deg - 180.0f) / 90.0f);
            return -1.0f * pgm_read_byte(sineTable + index) / 255.0;
        }
        else if (deg <= 360)
        {
            int index = (int) roundf(sineSize * (360.0f - deg) / 90.0f);
            return -1.0f * pgm_read_byte(sineTable + index) / 255.0;
        }
        return 0.0f;
    }

    // Display the mount status string
    void drawStatus(Mount *mount)
    {
        display->setColor(WHITE);
        display->setFont(Bitmap5x7);
        String state = mount->getStatusStateString();
        state.toUpperCase();
        display->drawString(4, 14, state.c_str());

        // Bouncing pixel (bounce frequency every 1.5s). 180 degrees is one cap.
        float deg  = 180.0f * (millis() % 1500) / 1500.0f;
        int pixPos = (int) round(1.0f * yMaxStatus * sinLookup(deg));
        display->setPixel(0, 11 + yMaxStatus - pixPos);
    }
};
