#pragma once
#include <Arduino.h>
#include "SSD1306Wire.h"
#include "Configuration.hpp"
#include "Utility.hpp"
#include "Version.h"
#include "fonts128x64.h"
#include "Mount.hpp"
#include "InfoDisplayRender.hpp"

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

const float sineSize              = 18.0;
const uint8_t sineTable[] PROGMEM = {0, 22, 44, 66, 87, 108, 128, 146, 164, 180, 195, 209, 221, 231, 240, 246, 251, 254, 255, 255};

class Mount;

// This class renders the mount status to a 128x64 pixel display controlled by a SSD1306 chip.
class SDD1306OLED128x64 : public InfoDisplayRender
{
#if defined(OAM) || defined(OAE)
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

    char _commLetter;
    static Mount *_mount;

  public:
    SDD1306OLED128x64(uint8_t addr, int sda, int scl);
    void static renderCallback(void *context);
    void drawScreen();

    // Build the display from the mount
    void renderScreen(void *mount);

    // Display the tiem left before tracking hits the limit
    void drawTime(Mount *mount, String label, const DayTime &time);

    // Draw all the indicators on screen
    void drawIndicators(Mount *mount);

    // Display two 2-pixel high progress bar in the last 4 lines of the display
    void drawProgressBar(int percRA, int percDEC);

    // Display a rectangle with the stepper label in it
    void drawStepperState(String name, bool active, int xoff, int width, int textOffX = 0);

    // Display all the configured stepper status rectangles
    // Focuser is currently not supported, but could be added here, if possible.
    void drawStepperStates(Mount *mount);

    void drawCommunicationStatus(Mount *mount);

    // Draw the given coordinate string at the given point
    void drawCoordinate(int x, int y, const char *coord);

    // Draw the mounts celestial RA and DEC coordinates
    void drawCoordinates(Mount *mount);

    // Map the given RA coordinate to the pixel position on the display
    int xRAPixel(float ra);

    // Map the given DEC coordinate to the pixel position on the display
    int yDECPixel(float dec);

    // Draw the rectangle with the current and target positions
    void drawMountPosition(Mount *mount);
    // Display the tiem left before tracking hits the limit
    void drawSafeTime(Mount *mount);

    // Display the mount status string
    void drawStatus(Mount *mount);
};
