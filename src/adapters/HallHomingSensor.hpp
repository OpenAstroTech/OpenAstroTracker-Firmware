#pragma once

#include "hal/IGpioPin.hpp"
#include "ports/IHomingSensor.hpp"

namespace adapters
{

/// `ports::IHomingSensor` over one GPIO pin.
///
/// Like `GpioEndSwitch`, the wired polarity is an adapter detail: the homing controller only ever
/// asks "does the sensor see the magnet".
class HallHomingSensor final : public ports::IHomingSensor
{
  public:
    HallHomingSensor(hal::IGpioPin &pin, hal::PinLevel activeLevel) : _pin(pin), _activeLevel(activeLevel)
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
