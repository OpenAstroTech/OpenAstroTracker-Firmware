#pragma once

#include <stdint.h>

#include "../../../Configuration.hpp"

// A board without a menu panel compiles none of this: the panel backends are the only place the
// display libraries are named, so leaving them out keeps a headless build free of them.
#if DISPLAY_TYPE > 0

    #include "hal/WarningGuard.hpp"

PUSH_NO_WARNINGS
    #include <Arduino.h>
POP_NO_WARNINGS

    #include "hal/IButtonMatrix.hpp"
    #include "hal/IGpioPin.hpp"

namespace hal
{

/// `IButtonMatrix` over the resistive key ladder of the LCD keypad shields.
///
/// The five keys share one analog pin: each key closes a different divider tap, so the reading maps
/// to a key by threshold. The thresholds are the shield's resistor values, measured once.
class AnalogKeyPad final : public IButtonMatrix
{
  public:
    explicit AnalogKeyPad(IGpioPin &sensePin);

    Button pressed() override;

    uint16_t rawValue() override;

  private:
    IGpioPin &_sensePin;
};

}  // namespace hal

#endif
