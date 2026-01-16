#include "SSD1306_128x64_Display.hpp"

#if defined(ESP32)
    /*
     * ESP32 PROGMEM is fake, and its pgm_read_byte macro makes a useless cast
     * which errors out with Werror=useless-cast, which can't be suppressed by
     * our PUSH/POP_NO_WARNINGS because the macro expands in OUR code, not their
     * header :/
     */
    #undef pgm_read_byte
    #define pgm_read_byte(addr) (*(addr))
#endif

Mount *SDD1306OLED128x64::_mount = nullptr;

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

SDD1306OLED128x64::SDD1306OLED128x64(uint8_t addr, int sda, int scl) : InfoDisplayRender()
{
    _display    = new SSD1306Wire(addr, sda, scl, GEOMETRY_128_64);
    _commLetter = ' ';
}

void SDD1306OLED128x64::renderCallback(void *context)
{
    SDD1306OLED128x64 *display = static_cast<SDD1306OLED128x64 *>(context);
    display->drawScreen();
}

void SDD1306OLED128x64::drawScreen()
{
    _display->setColor(WHITE);
    if (_consoleMode)
    {
        // Draw header only in console mode, in the area 0,0 to 127,20
        // Logo on the left. Base class renders console output text.
        _display->setFont(OATLogo);
        _display->drawString(0, 0, "!");

        // Name on the right
        _display->setFont(Bitmap5x7);
#ifdef OAE
        _display->drawString(32, 6, F("OpenAstroExplorer"));
#elif defined(OAM)
        _display->drawString(32, 6, F("OpenAstroMount"));
#else
        _display->drawString(32, 6, F("OpenAstroTracker"));
#endif
    }
    else
    {
        // Draw indicators in all other modes
        drawIndicators(SDD1306OLED128x64::_mount);
    }
}

// Build the display from the mount
void SDD1306OLED128x64::renderScreen(void *mount)
{
    _mount = (Mount *) mount;
    // Call the base
    this->InfoDisplayRender::render(renderCallback);
};

// Display the tiem left before tracking hits the limit
void SDD1306OLED128x64::drawTime(Mount *mount, String label, const DayTime &time)
{
    char achTemp[24];
    _display->setColor(WHITE);
    _display->setFont(Bitmap3x5);
    sprintf(achTemp, "%s%02d:%02d", label.c_str(), time.getHours(), time.getMinutes());
    _display->drawString(55, 59, achTemp);
}

// Draw all the indicators on screen
void SDD1306OLED128x64::drawIndicators(Mount *mount)
{
    char scratchBuffer[24];
    _display->setFont(Bitmap5x7);
    _display->setColor(WHITE);

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

        _display->setFont(CommSymbols);
        _display->drawString(11, 59, F("L"));  // Memory chip icon
        _display->setFont(Bitmap3x5);
        long availMem = freeMemory();
        if (availMem > 9999)
        {
            _display->drawString(20, 59, String(availMem / 1024) + "K");
        }
        else
        {
            _display->drawString(20, 59, String(availMem));
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
                    _display->drawString(55, 59, (String(F(" FW ")) + String(VERSION)).c_str());
                }
                break;
            case 4:
                {
                    float lat          = fabsf(mount->latitude().getTotalHours());
                    float lng          = fabsf(mount->longitude().getTotalHours());
                    const char dirLat  = (mount->latitude().getTotalHours() < 0) ? 'S' : 'N';
                    const char dirLong = (mount->longitude().getTotalHours() < 0) ? 'W' : 'E';
                    sprintf(scratchBuffer, "LOC %s%c %s%c", String(lat, 0).c_str(), dirLat, String(lng, 0).c_str(), dirLong);
                    _display->drawString(55, 59, scratchBuffer);
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
void SDD1306OLED128x64::drawProgressBar(int percRA, int percDEC)
{
    _display->setColor(WHITE);
    _display->drawVerticalLine(127, 60, 4);
    int raWidth = round(1.28f * percRA);
    _display->fillRect(0, 60, raWidth, 2);
    int decWidth = round(1.28f * percDEC);
    _display->fillRect(0, 62, decWidth, 2);
}

// Display a rectangle with the stepper label in it
void SDD1306OLED128x64::drawStepperState(String name, bool active, int xoff, int width, int textOffX)
{
    _display->setColor(WHITE);
    if (active)
    {
        _display->fillRect(xoff, 0, width, 11);
    }
    else
    {
        _display->drawRect(xoff, 0, width, 11);
    }
    _display->setColor(INVERSE);
    _display->drawString(xoff + 2 + textOffX, 2, name);
}

// Display all the configured stepper status rectangles
// Focuser is currently not supported, but could be added here, if possible.
void SDD1306OLED128x64::drawStepperStates(Mount *mount)
{
    _display->setFont(Bitmap5x7);
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

void SDD1306OLED128x64::drawCommunicationStatus(Mount *mount)
{
    long recvdCmds = mount->getNumCommandsReceived();
    // If we have received any commands since the last display, draw the marker.

    if (_commLetter != ' ')
    {
        _display->setFont(CommSymbols);
        _display->drawString(1, 59, String(_commLetter));
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
void SDD1306OLED128x64::drawCoordinate(int x, int y, const char *coord)
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
    _display->setFont(Bitmap7x15);
    _display->setColor(WHITE);
    _display->drawString(x, y, achCoord);
}

// Draw the mounts celestial RA and DEC coordinates
void SDD1306OLED128x64::drawCoordinates(Mount *mount)
{
    String rc = mount->RAString(LCD_STRING | CURRENT_STRING);
    String dc = mount->DECString(LCD_STRING | CURRENT_STRING);
    drawCoordinate(8, 24, rc.c_str());
    drawCoordinate(0, 42, dc.c_str());
}

// Map the given RA coordinate to the pixel position on the display
int SDD1306OLED128x64::xRAPixel(float ra)
{
    float rangeRA = rightRA - leftRA;
    int x         = 4 + (int) round(1.0f * (_raSize - 9) * ((ra - leftRA) / rangeRA));
    return (_leftEdgeMount + x);
}

// Map the given DEC coordinate to the pixel position on the display
int SDD1306OLED128x64::yDECPixel(float dec)
{
    int y = (int) round(1.0f * (_decSize) * ((dec - bottomDEC) / rangeDEC));
    return (_topEdgeMount + _decSize - y);
}

// Draw the rectangle with the current and target positions
void SDD1306OLED128x64::drawMountPosition(Mount *mount)
{
    _display->setColor(WHITE);
    _display->setFont(Bitmap3x5);

    // DEC tickmarks
    for (int p = _topEdgeMount; p <= _topEdgeMount + _decSize; p += 2)
    {
        _display->setPixel(_decScalePos, p);
    }
#if defined(OAM) || defined(OAE)
    _display->drawHorizontalLine(_decScalePos - 1, yDECPixel(-180.0), 2);
#endif
    _display->drawHorizontalLine(_decScalePos - 1, yDECPixel(-90.0), 2);
    _display->drawHorizontalLine(_decScalePos - 1, yDECPixel(0.0), 2);
    _display->drawHorizontalLine(_decScalePos - 1, yDECPixel(90.0), 2);
    _display->drawHorizontalLine(_decScalePos - 1, yDECPixel(180.0), 2);
// DEC tickmark labels
#if defined(OAM) || defined(OAE)
    _display->drawString(_decScalePos + 6, yDECPixel(-180.0f) - 2, F("180"));
    _display->drawHorizontalLine(_decScalePos + 3, yDECPixel(-180.0), 2);  // Smaller minus sign
#endif
    _display->drawString(_decScalePos + 6, yDECPixel(-90.0f) - 2, F("90"));
    _display->drawHorizontalLine(_decScalePos + 3, yDECPixel(-90.0), 2);  // Smaller minus sign
    _display->drawString(_decScalePos + 3, yDECPixel(0.0f) - 2, "0");
    _display->drawString(_decScalePos + 3, yDECPixel(90.0f) - 2, F("90"));
    _display->drawString(_decScalePos + 3, yDECPixel(180.0f) - 2, F("180"));

    // DEC Pos Marker
    float decStepsPerDeg = mount->getStepsPerDegree(StepperAxis::DEC_STEPS);
    long decSteps        = mount->getCurrentStepperPosition(StepperAxis::DEC_STEPS);
    float decDegrees     = decSteps / decStepsPerDeg;
    int yMark            = yDECPixel(decDegrees);
    _display->setPixel(_decScalePos - 2, yMark);
    _display->drawVerticalLine(_decScalePos - 3, yMark - 1, 3);
    _display->drawVerticalLine(_decScalePos - 4, yMark - 2, 5);

    // RA tickmarks
    for (int p = _leftEdgeMount; p <= _leftEdgeMount + _raSize; p += 2)
    {
        _display->setPixel(p, _raScalePos);
    }
    _display->drawVerticalLine(xRAPixel(-6.0f), _raScalePos - 1, 2);
    _display->drawVerticalLine(xRAPixel(-3.0f), _raScalePos - 1, 2);
    _display->drawVerticalLine(xRAPixel(0.0f), _raScalePos - 1, 2);
    _display->drawVerticalLine(xRAPixel(3.0f), _raScalePos - 1, 2);
    _display->drawVerticalLine(xRAPixel(6.0f), _raScalePos - 1, 2);

    // RA tickmark labels
    _display->drawString(xRAPixel(-6.0f) - 1, _raScalePos + 2, "6");
    _display->drawHorizontalLine(xRAPixel(-6.0f) - 4, _raScalePos + 2 + 2, 2);  // Smaller minus sign
    _display->drawString(xRAPixel(-3.0f) - 1, _raScalePos + 2, "3");
    _display->drawHorizontalLine(xRAPixel(-3.0f) - 4, _raScalePos + 2 + 2, 2);  // Smaller minus sign
    _display->drawString(xRAPixel(0.0f) - 1, _raScalePos + 2, "0");
    _display->drawString(xRAPixel(3.0f) - 1, _raScalePos + 2, "3");
    _display->drawString(xRAPixel(6.0f) - 1, _raScalePos + 2, "6");

    float raStepsPerDeg = mount->getStepsPerDegree(StepperAxis::RA_STEPS);
    float trkSteps      = 1.0f * mount->getCurrentStepperPosition(TRACKING) / (1.0f * RA_TRACKING_MICROSTEPPING / RA_SLEW_MICROSTEPPING);
    long raSteps        = mount->getCurrentStepperPosition(StepperAxis::RA_STEPS);
    float raHours       = (trkSteps + raSteps) / raStepsPerDeg / 15.0f;

    // RA Position Marker
    int xMark = xRAPixel(raHours);
    _display->setPixel(xMark, _raScalePos - 2);
    _display->drawHorizontalLine(xMark - 1, _raScalePos - 3, 3);
    _display->drawHorizontalLine(xMark - 2, _raScalePos - 4, 5);
}

// Display the tiem left before tracking hits the limit
void SDD1306OLED128x64::drawSafeTime(Mount *mount)
{
    char achTemp[10];
    float hoursLeft = mount->checkRALimit();
    DayTime dt(hoursLeft);
    _display->setColor(WHITE);
    _display->setFont(CommSymbols);
    _display->drawString(48, 59, "M");  // Clock sign
    _display->setFont(Bitmap3x5);
    sprintf(achTemp, "%02d:%02d", dt.getHours(), dt.getMinutes());
    _display->drawString(55, 59, achTemp);
}

// Display the mount status string
void SDD1306OLED128x64::drawStatus(Mount *mount)
{
    _display->setColor(WHITE);
    _display->setFont(Bitmap5x7);
    String state = mount->getStatusStateString();
    state.toUpperCase();
    _display->drawString(4, 14, state.c_str());

    // Bouncing pixel (bounce frequency every 1.5s). 180 degrees is one cap.
    float deg  = 180.0f * (millis() % 1500) / 1500.0f;
    int pixPos = (int) round(1.0f * yMaxStatus * sinLookup(deg));
    _display->setPixel(0, 11 + yMaxStatus - pixPos);
}
