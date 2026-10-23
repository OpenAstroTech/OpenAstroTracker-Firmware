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

const ports::InfoDisplayState *SDD1306OLED128x64::_state = nullptr;

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

// Console-mode header, in the area 0,0 to 127,20.
// Logo on the left, product name on the right.
void SDD1306OLED128x64::drawHeader()
{
    _display->setColor(WHITE);
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

void SDD1306OLED128x64::drawScreen()
{
    if (_consoleMode)
    {
        drawHeader();
    }
    else
    {
        // Draw indicators in all other modes
        drawIndicators();
    }
}

// Draw one frame from a snapshot the composition root produced
void SDD1306OLED128x64::renderScreen(const ports::InfoDisplayState &state)
{
    _state = &state;
    // Call the base
    this->InfoDisplayRender::render(renderCallback);
};

// Display the tiem left before tracking hits the limit
void SDD1306OLED128x64::drawTime(String label, const DayTime &time)
{
    char achTemp[24];
    _display->setColor(WHITE);
    _display->setFont(Bitmap3x5);
    sprintf(achTemp, "%s%02d:%02d", label.c_str(), time.getHours(), time.getMinutes());
    _display->drawString(55, 59, achTemp);
}

// Draw all the indicators on screen
void SDD1306OLED128x64::drawIndicators()
{
    char scratchBuffer[24];
    _display->setFont(Bitmap5x7);
    _display->setColor(WHITE);

    drawStepperStates();
    // If a slew is in progress, we don't display the safe time, version, and
    // comms indicator, since the progress bar takes up the same space
    if (_state->progressActive)
    {
        drawProgressBar(_state->raProgressPercent, _state->decProgressPercent);
    }
    else
    {
        long timeSecs = millis() / 2000;  // Change every 2 secs
        int index     = timeSecs % 5;     // Cycle through multiple data displays

        _display->setFont(CommSymbols);
        _display->drawString(11, 59, F("L"));  // Memory chip icon
        _display->setFont(Bitmap3x5);
        long availMem = freeMemory();
        if (availMem > 999999)
        {
            _display->drawString(20, 59, String(availMem / 1024 / 1024, 1) + "M");
        }
        else if (availMem > 9999)
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
                    DayTime dt(_state->hoursToLimit);
                    drawTime(F("REM "), dt);
                }
                break;
            case 1:
                {
                    drawTime(F("LST "), DayTime(_state->lst));
                }
                break;
            case 2:
                {
                    long now      = millis();
                    long msPerDay = 60L * 60 * 24 * 1000;
                    int days      = (int) (now / msPerDay);
                    now -= days * msPerDay;
                    DayTime elapsed(1.0 * now / (1000.0 * 3600.0));
                    drawTime(F("UPT "), elapsed);
                }
                break;
            case 3:
                {
                    // Bitmap3x5 has no lowercase glyphs; uppercase so release-please's
                    // "v" version prefix renders as V instead of an out-of-range glyph.
                    String fw = String(F(" FW ")) + String(VERSION);
                    fw.toUpperCase();
                    _display->drawString(55, 59, fw.c_str());
                }
                break;
            case 4:
                {
                    float lat          = fabsf(_state->latitudeHours);
                    float lng          = fabsf(_state->longitudeHours);
                    const char dirLat  = (_state->latitudeHours < 0) ? 'S' : 'N';
                    const char dirLong = (_state->longitudeHours < 0) ? 'W' : 'E';
                    sprintf(scratchBuffer, "LOC %s%c %s%c", String(lat, 0).c_str(), dirLat, String(lng, 0).c_str(), dirLong);
                    _display->drawString(55, 59, scratchBuffer);
                }
                break;
        }
        drawCommunicationStatus();
    }
    drawCoordinates();
    drawMountPosition();
    drawStatus();
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
void SDD1306OLED128x64::drawStepperStates()
{
    _display->setFont(Bitmap5x7);
    drawStepperState(F("RA"), _state->raRunning, 0, 15);
    drawStepperState(F("DEC"), _state->decRunning, 16, 21);
#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    drawStepperState(F("ALT"), _state->altRunning, 38, 21);
#endif
#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    drawStepperState(F("AZ"), _state->azRunning, 60, 16);
#endif
    drawStepperState(F("GDE"), _state->guiding, 83, 21);
    drawStepperState(F("TRK"), _state->trkRunning, 105, 23, 1);
}

void SDD1306OLED128x64::drawCommunicationStatus()
{
    long recvdCmds = _state->commandsReceived;
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
void SDD1306OLED128x64::drawCoordinates()
{
    drawCoordinate(8, 24, _state->raText);
    drawCoordinate(0, 42, _state->decText);
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
void SDD1306OLED128x64::drawMountPosition()
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
    float decStepsPerDeg = _state->decStepsPerDegree;
    long decSteps        = _state->decSteps;
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

    float raStepsPerDeg = _state->raStepsPerDegree;
    float trkSteps      = 1.0f * _state->trkSteps / (1.0f * RA_TRACKING_MICROSTEPPING / RA_SLEW_MICROSTEPPING);
    long raSteps        = _state->raSteps;
    float raHours       = (trkSteps + raSteps) / raStepsPerDeg / 15.0f;

    // RA Position Marker
    int xMark = xRAPixel(raHours);
    _display->setPixel(xMark, _raScalePos - 2);
    _display->drawHorizontalLine(xMark - 1, _raScalePos - 3, 3);
    _display->drawHorizontalLine(xMark - 2, _raScalePos - 4, 5);
}

// Display the tiem left before tracking hits the limit
void SDD1306OLED128x64::drawSafeTime()
{
    char achTemp[10];
    DayTime dt(_state->hoursToLimit);
    _display->setColor(WHITE);
    _display->setFont(CommSymbols);
    _display->drawString(48, 59, "M");  // Clock sign
    _display->setFont(Bitmap3x5);
    sprintf(achTemp, "%02d:%02d", dt.getHours(), dt.getMinutes());
    _display->drawString(55, 59, achTemp);
}

// Display the mount status string
void SDD1306OLED128x64::drawStatus()
{
    _display->setColor(WHITE);
    _display->setFont(Bitmap5x7);
    String state = (_state->statusText != nullptr) ? _state->statusText : "";
    state.toUpperCase();
    _display->drawString(4, 14, state.c_str());

    // Bouncing pixel (bounce frequency every 1.5s). 180 degrees is one cap.
    float deg  = 180.0f * (millis() % 1500) / 1500.0f;
    int pixPos = (int) round(1.0f * yMaxStatus * sinLookup(deg));
    _display->setPixel(0, 11 + yMaxStatus - pixPos);
}
