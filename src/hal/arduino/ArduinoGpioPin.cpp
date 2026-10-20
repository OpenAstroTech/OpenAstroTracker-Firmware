#include "hal/WarningGuard.hpp"

PUSH_NO_WARNINGS
#include <Arduino.h>
POP_NO_WARNINGS

#include "hal/arduino/ArduinoGpioPin.hpp"

namespace hal
{

ArduinoGpioPin::ArduinoGpioPin(uint8_t pin) : _pin(pin)
{
}

void ArduinoGpioPin::configure(PinMode mode)
{
    switch (mode)
    {
        case PinMode::Input:
            ::pinMode(_pin, INPUT);
            break;
        case PinMode::InputPullup:
            ::pinMode(_pin, INPUT_PULLUP);
            break;
        case PinMode::Output:
            ::pinMode(_pin, OUTPUT);
            break;
    }
}

void ArduinoGpioPin::write(PinLevel level)
{
    ::digitalWrite(_pin, level == PinLevel::High ? HIGH : LOW);
}

PinLevel ArduinoGpioPin::read() const
{
    return ::digitalRead(_pin) == HIGH ? PinLevel::High : PinLevel::Low;
}

uint16_t ArduinoGpioPin::analogRead() const
{
    return static_cast<uint16_t>(::analogRead(_pin));
}

void ArduinoGpioPin::writeAnalog(uint8_t duty)
{
    ::analogWrite(_pin, duty);
}

}  // namespace hal
