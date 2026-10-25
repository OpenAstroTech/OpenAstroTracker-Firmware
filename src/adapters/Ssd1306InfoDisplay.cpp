#include "adapters/Ssd1306InfoDisplay.hpp"

#if (INFO_DISPLAY_TYPE == INFO_DISPLAY_TYPE_I2C_SSD1306_128x64)

    #include "../../Configuration.hpp"
    #include "Utility.hpp"
    #include "hal/IOledPanel.hpp"

namespace
{

const float kSineSize              = 18.0;
const uint8_t kSineTable[] PROGMEM = {0, 22, 44, 66, 87, 108, 128, 146, 164, 180, 195, 209, 221, 231, 240, 246, 251, 254, 255, 255};

float sinLookup(float deg)
{
    while (deg < 0.0f)
        deg += 360.0f;
    while (deg > 360.0f)
        deg -= 360.0f;

    if (deg <= 90)
    {
        int index = (int) roundf(kSineSize * deg / 90.0f);
        return 1.0f * pgm_read_byte(kSineTable + index) / 255.0;
    }
    else if (deg <= 180)
    {
        int index = (int) roundf(kSineSize * (180.0f - deg) / 90.0f);
        return 1.0f * pgm_read_byte(kSineTable + index) / 255.0;
    }
    else if (deg <= 270)
    {
        int index = (int) roundf(kSineSize * (deg - 180.0f) / 90.0f);
        return -1.0f * pgm_read_byte(kSineTable + index) / 255.0;
    }
    else if (deg <= 360)
    {
        int index = (int) roundf(kSineSize * (360.0f - deg) / 90.0f);
        return -1.0f * pgm_read_byte(kSineTable + index) / 255.0;
    }
    return 0.0f;
}

}  // namespace

namespace adapters
{

Ssd1306InfoDisplay::Ssd1306InfoDisplay(hal::IOledPanel &panel, ports::IClock &clock)
    : _panel(panel), _clock(clock), _consoleMode(true), _curLine(0), _commLetter(' '), _lastNumCmds(0)
{
    for (int i = 0; i < kMaxConsoleLines; i++)
    {
        _textList[i] = "";
    }
}

bool Ssd1306InfoDisplay::begin()
{
    const bool started = _panel.init();
    #if (INFO_DISPLAY_UPSIDE_DOWN == 1)
    _panel.flipVertically();
    #endif
    #if (INFO_DISPLAY_MIRRORED == 1)
    _panel.mirror();
    #endif
    _panel.clear();
    _panel.power(true);
    return started;
}

void Ssd1306InfoDisplay::setConsoleMode(bool active)
{
    _consoleMode = active;
}

void Ssd1306InfoDisplay::render(const ports::InfoDisplayState &state)
{
    _panel.clear();
    if (_consoleMode)
    {
        drawHeader();
        drawConsole();
    }
    else
    {
        drawIndicators(state);
    }
    _panel.display();
}

int Ssd1306InfoDisplay::addConsoleText(const String &text)
{
    int line;
    if (_curLine > kMaxConsoleLines - 1)
    {
        for (int i = 0; i < kMaxConsoleLines - 1; i++)
        {
            _textList[i] = _textList[i + 1];
        }
        _curLine = kMaxConsoleLines - 1;
        line     = _curLine;
    }
    else
    {
        line = _curLine++;
    }

    _textList[line] = text;
    renderConsole();
    return line;
}

void Ssd1306InfoDisplay::updateConsoleText(int line, const String &text)
{
    if (line >= 0 && line < kMaxConsoleLines)
    {
        _textList[line] = text;
    }
    renderConsole();
}

void Ssd1306InfoDisplay::renderConsole()
{
    _panel.clear();
    drawHeader();
    drawConsole();
    _panel.display();
}

// Console-mode header, in the area 0,0 to 127,20: logo on the left, product name on the right.
void Ssd1306InfoDisplay::drawHeader()
{
    _panel.setColor(hal::OledColor::White);
    _panel.setFont(hal::OledFont::OatLogo);
    _panel.drawString(0, 0, "!");

    _panel.setFont(hal::OledFont::Bitmap5x7);
    #ifdef OAE
    _panel.drawString(32, 6, F("OpenAstroExplorer"));
    #elif defined(OAM)
    _panel.drawString(32, 6, F("OpenAstroMount"));
    #else
    _panel.drawString(32, 6, F("OpenAstroTracker"));
    #endif
}

void Ssd1306InfoDisplay::drawConsole()
{
    int y = 21;
    _panel.setFont(hal::OledFont::Bitmap3x5);
    const int indexStart = max(0, _curLine - kConsoleVisibleLines);
    const int indexEnd   = min(indexStart + kConsoleVisibleLines, kMaxConsoleLines);
    for (int i = indexStart; i < indexEnd; i++)
    {
        if (_textList[i].length() != 0)
        {
            String text = _textList[i];
            text.toUpperCase();
            _panel.drawString(0, y, text.c_str());
        }
        y += 7;
    }
}

void Ssd1306InfoDisplay::drawTime(const char *label, const core::DayTime &time)
{
    char achTemp[24];
    _panel.setColor(hal::OledColor::White);
    _panel.setFont(hal::OledFont::Bitmap3x5);
    sprintf(achTemp, "%s%02d:%02d", label, time.getHours(), time.getMinutes());
    _panel.drawString(55, 59, achTemp);
}

void Ssd1306InfoDisplay::drawIndicators(const ports::InfoDisplayState &state)
{
    char scratchBuffer[24];
    _panel.setFont(hal::OledFont::Bitmap5x7);
    _panel.setColor(hal::OledColor::White);

    drawStepperStates(state);
    // While a slew is in progress the progress bar takes the place of the safe time, version and
    // communication indicator.
    if (state.progressActive)
    {
        drawProgressBar(state.raProgressPercent, state.decProgressPercent);
    }
    else
    {
        const long timeSecs = _clock.now() / 2000;  // Change every 2 seconds
        const int index     = timeSecs % 5;         // Cycle through the data readouts

        _panel.setFont(hal::OledFont::CommSymbols);
        _panel.drawString(11, 59, F("L"));  // Memory chip icon
        _panel.setFont(hal::OledFont::Bitmap3x5);
        const long availMem = freeMemory();
        if (availMem > 999999)
        {
            _panel.drawString(20, 59, String(availMem / 1024 / 1024, 1) + "M");
        }
        else if (availMem > 9999)
        {
            _panel.drawString(20, 59, String(availMem / 1024) + "K");
        }
        else
        {
            _panel.drawString(20, 59, String(availMem));
        }

        switch (index)
        {
            case 0:
                {
                    core::DayTime dt(state.hoursToLimit);
                    drawTime(F("REM "), dt);
                }
                break;
            case 1:
                {
                    drawTime(F("LST "), state.lst);
                }
                break;
            case 2:
                {
                    long now      = _clock.now();
                    long msPerDay = 60L * 60 * 24 * 1000;
                    int days      = (int) (now / msPerDay);
                    now -= days * msPerDay;
                    core::DayTime elapsed(1.0 * now / (1000.0 * 3600.0));
                    drawTime(F("UPT "), elapsed);
                }
                break;
            case 3:
                {
                    // Bitmap3x5 has no lowercase glyphs; uppercase so release-please's
                    // "v" version prefix renders as V instead of an out-of-range glyph.
                    String fw = String(F(" FW ")) + String(VERSION);
                    fw.toUpperCase();
                    _panel.drawString(55, 59, fw.c_str());
                }
                break;
            case 4:
                {
                    float lat          = fabsf(state.latitudeHours);
                    float lng          = fabsf(state.longitudeHours);
                    const char dirLat  = (state.latitudeHours < 0) ? 'S' : 'N';
                    const char dirLong = (state.longitudeHours < 0) ? 'W' : 'E';
                    sprintf(scratchBuffer, "LOC %s%c %s%c", String(lat, 0).c_str(), dirLat, String(lng, 0).c_str(), dirLong);
                    _panel.drawString(55, 59, scratchBuffer);
                }
                break;
        }
        drawCommunicationStatus(state);
    }
    drawCoordinates(state);
    drawMountPosition(state);
    drawStatus(state);
}

// Two 2-pixel high progress bars in the last four rows of the display.
void Ssd1306InfoDisplay::drawProgressBar(int percRA, int percDEC)
{
    _panel.setColor(hal::OledColor::White);
    _panel.drawVerticalLine(127, 60, 4);
    _panel.fillRect(0, 60, (int) round(1.28f * percRA), 2);
    _panel.fillRect(0, 62, (int) round(1.28f * percDEC), 2);
}

void Ssd1306InfoDisplay::drawStepperState(const char *name, bool active, int xoff, int width, int textOffX)
{
    _panel.setColor(hal::OledColor::White);
    if (active)
    {
        _panel.fillRect(xoff, 0, width, 11);
    }
    else
    {
        _panel.drawRect(xoff, 0, width, 11);
    }
    _panel.setColor(hal::OledColor::Inverse);
    _panel.drawString(xoff + 2 + textOffX, 2, name);
}

// The focuser has no status rectangle: there is no room left in the header row.
void Ssd1306InfoDisplay::drawStepperStates(const ports::InfoDisplayState &state)
{
    _panel.setFont(hal::OledFont::Bitmap5x7);
    drawStepperState(F("RA"), state.raRunning, 0, 15);
    drawStepperState(F("DEC"), state.decRunning, 16, 21);
    #if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    drawStepperState(F("ALT"), state.altRunning, 38, 21);
    #endif
    #if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    drawStepperState(F("AZ"), state.azRunning, 60, 16);
    #endif
    drawStepperState(F("GDE"), state.guiding, 83, 21);
    drawStepperState(F("TRK"), state.trkRunning, 105, 23, 1);
}

void Ssd1306InfoDisplay::drawCommunicationStatus(const ports::InfoDisplayState &state)
{
    if (_commLetter != ' ')
    {
        _panel.setFont(hal::OledFont::CommSymbols);
        _panel.drawString(1, 59, String(_commLetter).c_str());
        _commLetter++;
        if (_commLetter == 'G')  // Past the last communication animation frame (F)
        {
            _commLetter = ' ';
        }
    }
    else if (state.commandsReceived != _lastNumCmds)
    {
        _commLetter  = 'C';  // First communication animation frame
        _lastNumCmds = state.commandsReceived;
    }
}

// Draw the given coordinate string at the given point
void Ssd1306InfoDisplay::drawCoordinate(int x, int y, const char *coord)
{
    char achCoord[30];
    char *n = achCoord;
    // This font is not a full character set, so remap the letters it lacks.
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
    _panel.setFont(hal::OledFont::Bitmap7x15);
    _panel.setColor(hal::OledColor::White);
    _panel.drawString(x, y, achCoord);
}

void Ssd1306InfoDisplay::drawCoordinates(const ports::InfoDisplayState &state)
{
    drawCoordinate(8, 24, state.raText);
    drawCoordinate(0, 42, state.decText);
}

int Ssd1306InfoDisplay::xRAPixel(float ra)
{
    const float rangeRA = kRightRa - kLeftRa;
    const int x         = 4 + (int) round(1.0f * (kRaSize - 9) * ((ra - kLeftRa) / rangeRA));
    return (kLeftEdgeMount + x);
}

int Ssd1306InfoDisplay::yDECPixel(float dec)
{
    const int y = (int) round(1.0f * (kDecSize) * ((dec - kBottomDec) / kRangeDec));
    return (kTopEdgeMount + kDecSize - y);
}

void Ssd1306InfoDisplay::drawMountPosition(const ports::InfoDisplayState &state)
{
    _panel.setColor(hal::OledColor::White);
    _panel.setFont(hal::OledFont::Bitmap3x5);

    // DEC tick marks
    for (int p = kTopEdgeMount; p <= kTopEdgeMount + kDecSize; p += 2)
    {
        _panel.setPixel(kDecScalePos, p);
    }
    #if defined(OAM) || defined(OAE)
    _panel.drawHorizontalLine(kDecScalePos - 1, yDECPixel(-180.0), 2);
    #endif
    _panel.drawHorizontalLine(kDecScalePos - 1, yDECPixel(-90.0), 2);
    _panel.drawHorizontalLine(kDecScalePos - 1, yDECPixel(0.0), 2);
    _panel.drawHorizontalLine(kDecScalePos - 1, yDECPixel(90.0), 2);
    _panel.drawHorizontalLine(kDecScalePos - 1, yDECPixel(180.0), 2);

    // DEC tick labels
    #if defined(OAM) || defined(OAE)
    _panel.drawString(kDecScalePos + 6, yDECPixel(-180.0f) - 2, F("180"));
    _panel.drawHorizontalLine(kDecScalePos + 3, yDECPixel(-180.0), 2);  // Smaller minus sign
    #endif
    _panel.drawString(kDecScalePos + 6, yDECPixel(-90.0f) - 2, F("90"));
    _panel.drawHorizontalLine(kDecScalePos + 3, yDECPixel(-90.0), 2);  // Smaller minus sign
    _panel.drawString(kDecScalePos + 3, yDECPixel(0.0f) - 2, "0");
    _panel.drawString(kDecScalePos + 3, yDECPixel(90.0f) - 2, F("90"));
    _panel.drawString(kDecScalePos + 3, yDECPixel(180.0f) - 2, F("180"));

    // DEC position marker
    const float decDegrees = state.decSteps / state.decStepsPerDegree;
    const int yMark        = yDECPixel(decDegrees);
    _panel.setPixel(kDecScalePos - 2, yMark);
    _panel.drawVerticalLine(kDecScalePos - 3, yMark - 1, 3);
    _panel.drawVerticalLine(kDecScalePos - 4, yMark - 2, 5);

    // RA tick marks
    for (int p = kLeftEdgeMount; p <= kLeftEdgeMount + kRaSize; p += 2)
    {
        _panel.setPixel(p, kRaScalePos);
    }
    _panel.drawVerticalLine(xRAPixel(-6.0f), kRaScalePos - 1, 2);
    _panel.drawVerticalLine(xRAPixel(-3.0f), kRaScalePos - 1, 2);
    _panel.drawVerticalLine(xRAPixel(0.0f), kRaScalePos - 1, 2);
    _panel.drawVerticalLine(xRAPixel(3.0f), kRaScalePos - 1, 2);
    _panel.drawVerticalLine(xRAPixel(6.0f), kRaScalePos - 1, 2);

    // RA tick labels
    _panel.drawString(xRAPixel(-6.0f) - 1, kRaScalePos + 2, "6");
    _panel.drawHorizontalLine(xRAPixel(-6.0f) - 4, kRaScalePos + 2 + 2, 2);  // Smaller minus sign
    _panel.drawString(xRAPixel(-3.0f) - 1, kRaScalePos + 2, "3");
    _panel.drawHorizontalLine(xRAPixel(-3.0f) - 4, kRaScalePos + 2 + 2, 2);  // Smaller minus sign
    _panel.drawString(xRAPixel(0.0f) - 1, kRaScalePos + 2, "0");
    _panel.drawString(xRAPixel(3.0f) - 1, kRaScalePos + 2, "3");
    _panel.drawString(xRAPixel(6.0f) - 1, kRaScalePos + 2, "6");

    const float trkSteps = 1.0f * state.trkSteps / (1.0f * RA_TRACKING_MICROSTEPPING / RA_SLEW_MICROSTEPPING);
    const float raHours  = (trkSteps + state.raSteps) / state.raStepsPerDegree / 15.0f;

    // RA position marker
    const int xMark = xRAPixel(raHours);
    _panel.setPixel(xMark, kRaScalePos - 2);
    _panel.drawHorizontalLine(xMark - 1, kRaScalePos - 3, 3);
    _panel.drawHorizontalLine(xMark - 2, kRaScalePos - 4, 5);
}

void Ssd1306InfoDisplay::drawStatus(const ports::InfoDisplayState &state)
{
    _panel.setColor(hal::OledColor::White);
    _panel.setFont(hal::OledFont::Bitmap5x7);
    String text = (state.statusText != nullptr) ? state.statusText : "";
    text.toUpperCase();
    _panel.drawString(4, 14, text.c_str());

    // Bouncing pixel, one round trip every 1.5 s: 180 degrees is one cap.
    const float deg  = 180.0f * (_clock.now() % 1500) / 1500.0f;
    const int pixPos = (int) round(1.0f * kStatusBounceHeight * sinLookup(deg));
    _panel.setPixel(0, 11 + kStatusBounceHeight - pixPos);
}

}  // namespace adapters

#endif
