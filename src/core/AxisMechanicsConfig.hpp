#pragma once

#include <stdint.h>

namespace core
{
/// The speed and direction a single axis was built with.
///
/// `speed`/`acceleration` are in the units the stepper engine takes (steps/sec and steps/sec²);
/// they are what `core::AxisLimits` is filled from and what the slew-rate menu scales. The AVR
/// `NEW_STEPPER_LIB` backend derives its own speeds in `hal/avr/StepperConfiguration.hpp` and
/// does not read these.
struct AxisSpec {
    unsigned long speed        = 0;
    unsigned long acceleration = 0;
    bool invertDirection       = false;
};

/// The mechanical half of the mount: gearing, microstep modes and per-axis motion tuning.
///
/// One value for the whole mount rather than one struct per axis, because the derived numbers
/// (`*Transmission`, `*StepsPerRev`) are what the rest of the firmware quotes when it reports the
/// mount's geometry, and keeping them together is what makes "what is this board's gearing"
/// answerable in one place.
///
/// A board without the optional AZ/ALT/Focus axes still fills those fields (with the inert
/// defaults `Configuration_adv.hpp` supplies), so no consumer needs a `#if` to read the struct.
struct AxisMechanicsConfig {
    AxisSpec ra;
    AxisSpec dec;
    AxisSpec az;
    AxisSpec alt;
    AxisSpec focus;

    uint16_t raPulleyTeeth  = 16;
    uint16_t decPulleyTeeth = 16;
    uint16_t raStepperSpr   = 400;
    uint16_t decStepperSpr  = 400;
    float raTransmission    = 0.0f;
    float decTransmission   = 0.0f;

    uint16_t azMicrosteps  = 0;
    uint16_t altMicrosteps = 0;

    uint16_t azStepperSpr  = 0;
    uint16_t altStepperSpr = 0;

    float azCircumference           = 0.0f;
    float altCircumference          = 0.0f;
    float azimuthStepsPerRev        = 0.0f;
    float azimuthStepsPerArcMinute  = 0.0f;
    float altitudeStepsPerRev       = 0.0f;
    float altitudeStepsPerArcMinute = 0.0f;

    /// How often the platform services the stepper engines, in milliseconds: the AVR timer period
    /// and the RP2040 interval. It belongs with the mechanics because it is the resolution the step
    /// timing is generated at, not a communication setting.
    float stepperServiceIntervalMs = 0.5f;
};

}  // namespace core
