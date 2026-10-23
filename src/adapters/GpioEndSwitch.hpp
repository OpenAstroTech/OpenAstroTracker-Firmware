#pragma once

#include "hal/IGpioPin.hpp"
#include "ports/IEndSwitch.hpp"

namespace adapters
{

/// `ports::IEndSwitch` over one GPIO pin.
///
/// Polarity is normalised here: the caller says which level means "pressed", and `isTriggered()`
/// answers the domain's question the same way whichever way the switch is wired.
class GpioEndSwitch final : public ports::IEndSwitch
{
  public:
    GpioEndSwitch(hal::IGpioPin &pin, hal::PinLevel activeLevel) : _pin(pin), _activeLevel(activeLevel)
    {
    }

    bool isTriggered() override
    {
        return _pin.read() == _activeLevel;
    }

  private:
    hal::IGpioPin &_pin;
    hal::PinLevel _activeLevel;
};

}  // namespace adapters
