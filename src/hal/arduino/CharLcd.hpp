#pragma once

#include <stdint.h>

#include "../../../Configuration.hpp"

// A board without a menu panel compiles none of this: the panel backends are the only place the
// display libraries are named, so leaving them out keeps a headless build free of them.
#if DISPLAY_TYPE > 0

    #include "hal/WarningGuard.hpp"

PUSH_NO_WARNINGS
    #include <LiquidCrystal.h>
POP_NO_WARNINGS

    #include "hal/IGpioPin.hpp"
    #include "hal/ICharLcd.hpp"

namespace hal
{

/// `ICharLcd` over the parallel HD44780 shield driven by `LiquidCrystal`.
///
/// The backlight is not part of the LCD library: it is a PWM pin on the shield, so it is injected as
/// a pin rather than implemented here. A board without dimming passes nullptr.
class CharLcd final : public ICharLcd
{
  public:
    /// `backlightShorted` is the result of `backlightPinIsShorted()` on a shield that ties the
    /// transistor base straight to the pin without a resistor: such a board can only switch the
    /// backlight, and the pin must stay an input for "on".
    CharLcd(uint8_t rs,
            uint8_t enable,
            uint8_t d4,
            uint8_t d5,
            uint8_t d6,
            uint8_t d7,
            IGpioPin *backlightPin = nullptr,
            bool backlightShorted  = false);

    bool begin(uint8_t columns, uint8_t rows) override;

    void clear() override;

    void setCursor(uint8_t column, uint8_t row) override;

    void print(const char *text) override;

    void write(uint8_t value) override;

    bool hasDefinableSymbols() const override;

    void defineSymbol(CharSymbol symbol, const uint8_t bitmap[8]) override;

    void writeSymbol(CharSymbol symbol) override;

    void setBacklightLevel(uint8_t level) override;

  private:
    LiquidCrystal _lcd;
    IGpioPin *_backlightPin;
    bool _backlightShorted;
};

/// Probes the known-bad backlight circuit of some LCD shields.
///
/// The 'broken' designs connect a GPIO straight to the base of an NPN transistor with no current
/// limiting, so driving the pin HIGH shorts it. The probe leaves the pin as a floating input, which
/// is safe on every shield. Adapted from https://forum.arduino.cc/index.php?topic=96747.0, which
/// also documents the hardware fix that restores the full brightness range.
bool backlightPinIsShorted(uint8_t pin);

}  // namespace hal

#endif
