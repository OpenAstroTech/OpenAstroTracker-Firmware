#pragma once

#include <stdint.h>

namespace hal
{

enum class Button : uint8_t
{
    None,
    Right,
    Up,
    Down,
    Left,
    Select,
};

/// The five navigation keys, however they are wired: a resistive ladder on one
/// analog pin, discrete GPIOs, or a joystick behind an I2C expander.
///
/// Debouncing and repeat handling stay above this layer: the interface reports
/// what is held right now.
class IButtonMatrix
{
  public:
    virtual ~IButtonMatrix()
    {
    }

    virtual Button pressed() = 0;

    /// Raw analog reading of a resistive ladder, 0 when the hardware has no such pin.
    virtual uint16_t rawValue() = 0;
};

}  // namespace hal
