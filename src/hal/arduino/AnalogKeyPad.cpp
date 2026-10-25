#include "hal/arduino/AnalogKeyPad.hpp"

#include "../../../Configuration.hpp"

// A board without a menu panel compiles none of this: the panel backends are the only place the
// display libraries are named, so leaving them out keeps a headless build free of them.
#if DISPLAY_TYPE > 0

namespace hal
{

AnalogKeyPad::AnalogKeyPad(IGpioPin &sensePin) : _sensePin(sensePin)
{
}

Button AnalogKeyPad::pressed()
{
    const int value = static_cast<int>(rawValue());
    if (value > 1000)
    {
        return Button::None;
    }
    if (value < 50)
    {
        return Button::Right;
    }
    if (value < 240)
    {
        return Button::Up;
    }
    if (value < 400)
    {
        return Button::Down;
    }
    if (value < 600)
    {
        return Button::Left;
    }
    if (value < 920)
    {
        return Button::Select;
    }
    return Button::None;
}

uint16_t AnalogKeyPad::rawValue()
{
    return _sensePin.analogRead();
}

}  // namespace hal

#endif
