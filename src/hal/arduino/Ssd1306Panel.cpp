#include "hal/arduino/Ssd1306Panel.hpp"

#if (INFO_DISPLAY_TYPE == INFO_DISPLAY_TYPE_I2C_SSD1306_128x64)

namespace hal
{

namespace
{

/// The library's own font tables, selected by the panel-independent slot.
const uint8_t *fontTable(OledFont font)
{
    switch (font)
    {
        case OledFont::Bitmap5x7:
            return Bitmap5x7;
        case OledFont::Bitmap7x15:
            return Bitmap7x15;
        case OledFont::CommSymbols:
            return CommSymbols;
        case OledFont::OatLogo:
            return OatLogo;
        case OledFont::Bitmap3x5:
        default:
            return Bitmap3x5;
    }
}

OLEDDISPLAY_COLOR colorValue(OledColor color)
{
    switch (color)
    {
        case OledColor::Black:
            return BLACK;
        case OledColor::Inverse:
            return INVERSE;
        case OledColor::White:
        default:
            return WHITE;
    }
}

OLEDDISPLAY_TEXT_ALIGNMENT alignmentValue(OledTextAlignment alignment)
{
    switch (alignment)
    {
        case OledTextAlignment::Center:
            return TEXT_ALIGN_CENTER;
        case OledTextAlignment::Right:
            return TEXT_ALIGN_RIGHT;
        case OledTextAlignment::Left:
        default:
            return TEXT_ALIGN_LEFT;
    }
}

}  // namespace

Ssd1306Panel::Ssd1306Panel(uint8_t address, int sdaPin, int sclPin) : _display(address, sdaPin, sclPin, GEOMETRY_128_64)
{
}

bool Ssd1306Panel::init()
{
    return _display.init();
}

void Ssd1306Panel::clear()
{
    _display.clear();
}

void Ssd1306Panel::display()
{
    _display.display();
}

void Ssd1306Panel::power(bool on)
{
    if (on)
    {
        _display.displayOn();
    }
    else
    {
        _display.displayOff();
    }
}

void Ssd1306Panel::flipVertically()
{
    _display.flipScreenVertically();
}

void Ssd1306Panel::mirror()
{
    _display.mirrorScreen();
}

void Ssd1306Panel::setContrast(uint8_t contrast)
{
    _display.setContrast(contrast);
}

void Ssd1306Panel::setColor(OledColor color)
{
    _display.setColor(colorValue(color));
}

void Ssd1306Panel::setFont(OledFont font)
{
    _display.setFont(fontTable(font));
}

void Ssd1306Panel::setTextAlignment(OledTextAlignment alignment)
{
    _display.setTextAlignment(alignmentValue(alignment));
}

void Ssd1306Panel::drawString(int16_t x, int16_t y, const char *text)
{
    _display.drawString(x, y, text);
}

void Ssd1306Panel::setPixel(int16_t x, int16_t y)
{
    _display.setPixel(x, y);
}

void Ssd1306Panel::drawHorizontalLine(int16_t x, int16_t y, int16_t length)
{
    _display.drawHorizontalLine(x, y, length);
}

void Ssd1306Panel::drawVerticalLine(int16_t x, int16_t y, int16_t length)
{
    _display.drawVerticalLine(x, y, length);
}

void Ssd1306Panel::drawRect(int16_t x, int16_t y, int16_t width, int16_t height)
{
    _display.drawRect(x, y, width, height);
}

void Ssd1306Panel::fillRect(int16_t x, int16_t y, int16_t width, int16_t height)
{
    _display.fillRect(x, y, width, height);
}

}  // namespace hal

#endif
