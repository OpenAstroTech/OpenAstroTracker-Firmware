#include "hal/arduino/I2cExpanderButtons.hpp"

#include "../../../Configuration.hpp"

// A board without a menu panel compiles none of this: the panel backends are the only place the
// display libraries are named, so leaving them out keeps a headless build free of them.
#if DISPLAY_TYPE > 0

    #include "hal/arduino/I2cCharLcd.hpp"

namespace hal
{

I2cExpanderButtons::I2cExpanderButtons(I2cCharLcd &panel) : _panel(panel)
{
}

Button I2cExpanderButtons::pressed()
{
    const uint8_t buttons = _panel.readExpanderButtons();
    if (buttons == 0)
    {
        return Button::None;
    }
    // The backpack reports one bit per key; the later assignments win, as the original scan did.
    Button key = Button::None;
    if (buttons & BUTTON_UP)
    {
        key = Button::Up;
    }
    if (buttons & BUTTON_DOWN)
    {
        key = Button::Down;
    }
    if (buttons & BUTTON_LEFT)
    {
        key = Button::Left;
    }
    if (buttons & BUTTON_RIGHT)
    {
        key = Button::Right;
    }
    if (buttons & BUTTON_SELECT)
    {
        key = Button::Select;
    }
    return key;
}

uint16_t I2cExpanderButtons::rawValue()
{
    return 0;  // No analog ladder on this panel.
}

}  // namespace hal

#endif
