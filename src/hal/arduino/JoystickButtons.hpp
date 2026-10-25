#pragma once

#include <stdint.h>

#include "../../../Configuration.hpp"

// A board without a menu panel compiles none of this: the panel backends are the only place the
// display libraries are named, so leaving them out keeps a headless build free of them.
#if DISPLAY_TYPE > 0

    #include "hal/IButtonMatrix.hpp"
    #include "hal/IGpioPin.hpp"

namespace hal
{

/// `IButtonMatrix` over the joystick of the 128x32 OLED module: two analog axes and a push switch.
///
/// The axes idle at mid-scale and a key counts once the reading leaves the deadband; the push
/// switch is active low. Assumes the platform's default 12-bit ADC.
class JoystickButtons final : public IButtonMatrix
{
  public:
    JoystickButtons(IGpioPin &xPin, IGpioPin &yPin, IGpioPin &pushPin);

    Button pressed() override;

    uint16_t rawValue() override;

  private:
    static constexpr int16_t kMidScale = 4096 / 2;
    static constexpr int16_t kDeadband = 500;

    IGpioPin &_xPin;
    IGpioPin &_yPin;
    IGpioPin &_pushPin;
};

}  // namespace hal

#endif
