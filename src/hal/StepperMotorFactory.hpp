#pragma once

#include <stdint.h>

#include "IStepperMotor.hpp"

namespace hal
{

/// The engine each mounting point needs.
///
/// RA and DEC each drive two engines, because slewing and tracking/guiding run at different
/// microstep resolutions; the others are single.
enum class AxisRole
{
    RaSlew,
    RaTracking,
    DecSlew,
    DecGuide,
    Az,
    Alt,
    Focus
};

/// Builds the motion engine for one mounting point on the current platform.
///
/// This is the only place that knows which engines a platform has: on AVR the interrupt steppers
/// come from `hal/avr/`, elsewhere a plain `AccelStepper`. Callers name a role and two pins, so no
/// code above `hal/` selects a platform.
IStepperMotor *makeAxisMotor(AxisRole role, uint8_t pin1, uint8_t pin2);

}  // namespace hal
