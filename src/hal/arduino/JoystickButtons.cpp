#include "hal/arduino/JoystickButtons.hpp"

#include "../../../Configuration.hpp"

// A board without a menu panel compiles none of this: the panel backends are the only place the
// display libraries are named, so leaving them out keeps a headless build free of them.
#if DISPLAY_TYPE > 0

namespace hal
{

JoystickButtons::JoystickButtons(IGpioPin &xPin, IGpioPin &yPin, IGpioPin &pushPin) : _xPin(xPin), _yPin(yPin), _pushPin(pushPin)
{
}

Button JoystickButtons::pressed()
{
    const int16_t x    = static_cast<int16_t>(_xPin.analogRead());
    const int16_t y    = static_cast<int16_t>(_yPin.analogRead());
    const int16_t push = static_cast<int16_t>(_pushPin.analogRead());

    Button key = Button::None;
    if (x > (kMidScale + kDeadband))
    {
        key = Button::Right;
    }
    if (x < (kMidScale - kDeadband))
    {
        key = Button::Left;
    }
    if (y > (kMidScale + kDeadband))
    {
        key = Button::Down;  // The Y axis reads reversed on this module.
    }
    if (y < (kMidScale - kDeadband))
    {
        key = Button::Up;
    }
    if (push < kMidScale)
    {
        key = Button::Select;
    }
    return key;
}

uint16_t JoystickButtons::rawValue()
{
    return _yPin.analogRead();
}

}  // namespace hal

#endif
