#pragma once

#include <stdint.h>

#include "../../../Configuration.hpp"

// Only the boards that actually carry the 128x64 status panel compile this backend: it is the one
// place the OLED library is named, so a board without the panel stays free of it.
#if (INFO_DISPLAY_TYPE == INFO_DISPLAY_TYPE_I2C_SSD1306_128x64)

    #include "hal/WarningGuard.hpp"

PUSH_NO_WARNINGS
    #include <SSD1306Wire.h>
POP_NO_WARNINGS

    #include "hal/IOledPanel.hpp"
    #include "hal/arduino/OledFonts.hpp"

    #if defined(ESP32)
        /*
     * ESP32 PROGMEM is fake, and its pgm_read_byte macro makes a useless cast which errors out
     * with Werror=useless-cast; it cannot be suppressed because the macro expands in our code.
     */
        #undef pgm_read_byte
        #define pgm_read_byte(addr) (*(addr))
    #endif

namespace hal
{

/// `IOledPanel` over the 128x64 SSD1306 status panel driven by `SSD1306Wire`.
class Ssd1306Panel final : public IOledPanel
{
  public:
    Ssd1306Panel(uint8_t address, int sdaPin, int sclPin);

    bool init() override;

    void clear() override;

    void display() override;

    void power(bool on) override;

    void flipVertically() override;

    void mirror() override;

    void setContrast(uint8_t contrast) override;

    void setColor(OledColor color) override;

    void setFont(OledFont font) override;

    void setTextAlignment(OledTextAlignment alignment) override;

    void drawString(int16_t x, int16_t y, const char *text) override;

    void setPixel(int16_t x, int16_t y) override;

    void drawHorizontalLine(int16_t x, int16_t y, int16_t length) override;

    void drawVerticalLine(int16_t x, int16_t y, int16_t length) override;

    void drawRect(int16_t x, int16_t y, int16_t width, int16_t height) override;

    void fillRect(int16_t x, int16_t y, int16_t width, int16_t height) override;

  private:
    SSD1306Wire _display;
};

}  // namespace hal

#endif
