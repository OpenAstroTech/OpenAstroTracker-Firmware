#include "adapters/Ssd1306InfoDisplay.hpp"

#if (INFO_DISPLAY_TYPE == INFO_DISPLAY_TYPE_I2C_SSD1306_128x64)

    #include "../../Configuration.hpp"
    #include "hal/arduino/FreeMemory.hpp"
#include "ports/ILogger.hpp"
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
    : _panel(panel), _clock(clock), _consoleMode(true), _commLetter(' '), _lastNumCmds(0)
{
}

adapters::InfoDisplayScale Ssd1306InfoDisplay::scale() const
{
    #if defined(OAM) || defined(OAE)
    constexpr float bottomDec = -180.0f;
    constexpr float rangeDec  = 360.0f;
    #else
    constexpr float bottomDec = -90.0f;
    constexpr float rangeDec  = 270.0f;
    #endif
    // 69/41/51 and 14/43/115 are the panel's pixel layout: RA scale, DEC scale and their lines.
    return {69, 41, 51, 14, 43, 115, -6.0f, 6.0f, bottomDec, rangeDec};
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
    const int line = _console.add(text.c_str());
    renderConsole();
    return line;
}

void Ssd1306InfoDisplay::updateConsoleText(int line, const String &text)
{
    _console.update(line, text.c_str());
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
    _panel.drawString(32, 6, "OpenAstroExplorer");
    #elif defined(OAM)
    _panel.drawString(32, 6, "OpenAstroMount");
    #else
    _panel.drawString(32, 6, "OpenAstroTracker");
    #endif
}

void Ssd1306InfoDisplay::drawConsole()
{
    int y = 21;
    _panel.setFont(hal::OledFont::Bitmap3x5);
    const int first = _console.firstVisible();
    const int count = _console.visibleCount();
    for (int i = first; i < first + count; i++)
    {
        const char *text = _console.line(i);
        if (text[0] != '\0')
        {
            _panel.drawString(0, y, text);
        }
        y += 7;
    }
}

void Ssd1306InfoDisplay::drawTime(const char *label, const types::DayTime &time)
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
        _panel.drawString(11, 59, "L");  // Memory chip icon
        _panel.setFont(hal::OledFont::Bitmap3x5);
        const long availMem = freeMemory();
        if (availMem > 999999)
        {
            _panel.drawString(20, 59, (String(availMem / 1024 / 1024, 1) + "M").c_str());
        }
        else if (availMem > 9999)
        {
            _panel.drawString(20, 59, (String(availMem / 1024) + "K").c_str());
        }
        else
        {
            _panel.drawString(20, 59, String(availMem).c_str());
        }

        switch (index)
        {
            case 0:
                {
                    types::DayTime dt(state.hoursToLimit);
                    drawTime("REM ", dt);
                }
                break;
            case 1:
                {
                    drawTime("LST ", state.lst);
                }
                break;
            case 2:
                {
                    long now      = _clock.now();
                    long msPerDay = 60L * 60 * 24 * 1000;
                    int days      = (int) (now / msPerDay);
                    now -= days * msPerDay;
                    types::DayTime elapsed(1.0 * now / (1000.0 * 3600.0));
                    drawTime("UPT ", elapsed);
                }
                break;
            case 3:
                {
                    // Bitmap3x5 has no lowercase glyphs; uppercase so release-please's
                    // "v" version prefix renders as V instead of an out-of-range glyph.
                    String fw = String(" FW ") + String(VERSION);
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
    _panel.fillRect(0, 60, adapters::progressBarWidth(percRA), 2);
    _panel.fillRect(0, 62, adapters::progressBarWidth(percDEC), 2);
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
    drawStepperState("RA", state.raRunning, 0, 15);
    drawStepperState("DEC", state.decRunning, 16, 21);
    #if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    drawStepperState("ALT", state.altRunning, 38, 21);
    #endif
    #if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    drawStepperState("AZ", state.azRunning, 60, 16);
    #endif
    drawStepperState("GDE", state.guiding, 83, 21);
    drawStepperState("TRK", state.trkRunning, 105, 23, 1);
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

int Ssd1306InfoDisplay::xRAPixel(float ra) const
{
    return adapters::xRAPixel(scale(), ra);
}

int Ssd1306InfoDisplay::yDECPixel(float dec) const
{
    return adapters::yDECPixel(scale(), dec);
}

void Ssd1306InfoDisplay::drawMountPosition(const ports::InfoDisplayState &state)
{
    const adapters::InfoDisplayScale g = scale();
    _panel.setColor(hal::OledColor::White);
    _panel.setFont(hal::OledFont::Bitmap3x5);

    // DEC tick marks
    for (int p = g.topEdgeMount; p <= g.topEdgeMount + g.decSize; p += 2)
    {
        _panel.setPixel(g.decScalePos, p);
    }
    #if defined(OAM) || defined(OAE)
    _panel.drawHorizontalLine(g.decScalePos - 1, yDECPixel(-180.0), 2);
    #endif
    _panel.drawHorizontalLine(g.decScalePos - 1, yDECPixel(-90.0), 2);
    _panel.drawHorizontalLine(g.decScalePos - 1, yDECPixel(0.0), 2);
    _panel.drawHorizontalLine(g.decScalePos - 1, yDECPixel(90.0), 2);
    _panel.drawHorizontalLine(g.decScalePos - 1, yDECPixel(180.0), 2);

    // DEC tick labels
    #if defined(OAM) || defined(OAE)
    _panel.drawString(g.decScalePos + 6, yDECPixel(-180.0f) - 2, "180");
    _panel.drawHorizontalLine(g.decScalePos + 3, yDECPixel(-180.0), 2);  // Smaller minus sign
    #endif
    _panel.drawString(g.decScalePos + 6, yDECPixel(-90.0f) - 2, "90");
    _panel.drawHorizontalLine(g.decScalePos + 3, yDECPixel(-90.0), 2);  // Smaller minus sign
    _panel.drawString(g.decScalePos + 3, yDECPixel(0.0f) - 2, "0");
    _panel.drawString(g.decScalePos + 3, yDECPixel(90.0f) - 2, "90");
    _panel.drawString(g.decScalePos + 3, yDECPixel(180.0f) - 2, "180");

    // DEC position marker
    const int yMark = yDECPixel(adapters::markerDecDegrees(state.decSteps, state.decStepsPerDegree));
    _panel.setPixel(g.decScalePos - 2, yMark);
    _panel.drawVerticalLine(g.decScalePos - 3, yMark - 1, 3);
    _panel.drawVerticalLine(g.decScalePos - 4, yMark - 2, 5);

    // RA tick marks
    for (int p = g.leftEdgeMount; p <= g.leftEdgeMount + g.raSize; p += 2)
    {
        _panel.setPixel(p, g.raScalePos);
    }
    _panel.drawVerticalLine(xRAPixel(-6.0f), g.raScalePos - 1, 2);
    _panel.drawVerticalLine(xRAPixel(-3.0f), g.raScalePos - 1, 2);
    _panel.drawVerticalLine(xRAPixel(0.0f), g.raScalePos - 1, 2);
    _panel.drawVerticalLine(xRAPixel(3.0f), g.raScalePos - 1, 2);
    _panel.drawVerticalLine(xRAPixel(6.0f), g.raScalePos - 1, 2);

    // RA tick labels
    _panel.drawString(xRAPixel(-6.0f) - 1, g.raScalePos + 2, "6");
    _panel.drawHorizontalLine(xRAPixel(-6.0f) - 4, g.raScalePos + 2 + 2, 2);  // Smaller minus sign
    _panel.drawString(xRAPixel(-3.0f) - 1, g.raScalePos + 2, "3");
    _panel.drawHorizontalLine(xRAPixel(-3.0f) - 4, g.raScalePos + 2 + 2, 2);  // Smaller minus sign
    _panel.drawString(xRAPixel(0.0f) - 1, g.raScalePos + 2, "0");
    _panel.drawString(xRAPixel(3.0f) - 1, g.raScalePos + 2, "3");
    _panel.drawString(xRAPixel(6.0f) - 1, g.raScalePos + 2, "6");

    // RA position marker
    const int xMark = xRAPixel(
        adapters::markerRaHours(state.trkSteps, state.raSteps, state.raStepsPerDegree, RA_TRACKING_MICROSTEPPING, RA_SLEW_MICROSTEPPING));
    _panel.setPixel(xMark, g.raScalePos - 2);
    _panel.drawHorizontalLine(xMark - 1, g.raScalePos - 3, 3);
    _panel.drawHorizontalLine(xMark - 2, g.raScalePos - 4, 5);
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
