#pragma once

#include <stdint.h>

#include "core/Button.hpp"

namespace ports
{

/// The navigation keys, debounced and edge-reported.
///
/// Debouncing lives in the adapter (`core::ButtonDebounce`), so a screen sees a committed key and a
/// single edge per press however the keys are wired: a resistive ladder, discrete GPIOs or a
/// joystick behind an I2C expander.
class IButtons
{
  public:
    virtual ~IButtons()
    {
    }

    /// The key held right now, once it has settled.
    virtual core::Button pressed() = 0;

    /// True once per change of the committed key, with the new key in `out`.
    virtual bool keyChanged(core::Button *out) = 0;

    /// Raw reading of a resistive ladder, for the key diagnostic screen. 0 when the panel has none.
    virtual uint16_t rawValue() = 0;
};

}  // namespace ports
