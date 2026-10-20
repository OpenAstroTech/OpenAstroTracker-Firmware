#pragma once

#include <stdint.h>

namespace hal
{

enum class OledColor : uint8_t
{
    Black,
    White,
    Inverse,
};

/// Font slots exposed by the mono OLED panels. Names follow the font tables the
/// current SSD1306 library ships; a backend maps them to its own font handles and
/// ignores slots it cannot render.
enum class OledFont : uint8_t
{
    Bitmap3x5,
    Bitmap5x7,
    Bitmap7x15,
    CommSymbols,
    OatLogo,
};

enum class OledTextAlignment : uint8_t
{
    Left,
    Center,
    Right,
};

/// A monochrome pixel-addressable panel.
///
/// Text handed to this interface is always RAM-resident: adapters that hold
/// program-memory strings expand them before drawing, so the HAL never has to know
/// which platform keeps literals in flash.
class IOledPanel
{
  public:
    virtual ~IOledPanel()
    {
    }

    /// Powers on and initialises the controller. Returns false when the panel does
    /// not answer.
    virtual bool init() = 0;

    virtual void clear() = 0;

    /// Pushes the framebuffer to the panel.
    virtual void display() = 0;

    virtual void power(bool on) = 0;

    virtual void flipVertically() = 0;

    virtual void mirror() = 0;

    virtual void setContrast(uint8_t contrast) = 0;

    virtual void setColor(OledColor color) = 0;

    virtual void setFont(OledFont font) = 0;

    virtual void setTextAlignment(OledTextAlignment alignment) = 0;

    virtual void drawString(int16_t x, int16_t y, const char *text) = 0;

    virtual void setPixel(int16_t x, int16_t y) = 0;

    virtual void drawHorizontalLine(int16_t x, int16_t y, int16_t length) = 0;

    virtual void drawVerticalLine(int16_t x, int16_t y, int16_t length) = 0;

    virtual void drawRect(int16_t x, int16_t y, int16_t width, int16_t height) = 0;

    virtual void fillRect(int16_t x, int16_t y, int16_t width, int16_t height) = 0;
};

}  // namespace hal
