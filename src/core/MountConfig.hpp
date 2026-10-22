#pragma once

#include <stdint.h>

namespace core
{

/// The mount's motion shape, as the board's `Configuration*.hpp` defines it.
///
/// This is the half of the configuration that `Mount` reads while it moves: the microstep rate
/// each axis uses, and the steps-per-degree defaults a fresh EEPROM starts from. It is a plain
/// value filled once by the composition root (`app::buildMountConfig()`), so the macros are read
/// in one place instead of being re-expanded at forty-odd call sites inside `Mount`, and a board
/// can choose these at boot instead of at compile time.
///
/// Deliberately **not** here: anything the EEPROM owns after the first boot (the live
/// steps-per-degree, backlash steps, move rate). Those are runtime state; this is the starting
/// point they are initialized from.
///
/// Aggregate-initialized on every platform, including AVR's gnu++11, so it carries no default
/// member initializers.
struct MountConfig {
    uint16_t raSlewMicrosteps;    ///< RA microsteps while slewing
    uint16_t raTrackMicrosteps;   ///< RA microsteps while tracking
    uint16_t decSlewMicrosteps;   ///< DEC microsteps while slewing
    uint16_t decGuideMicrosteps;  ///< DEC microsteps while guiding
    float defaultRaStepsPerDegree;
    float defaultDecStepsPerDegree;

    /// The steps-per-degree values are the slew-mode ones, so tracking converts to the finer
    /// rate through this integer ratio. One when a board straps a single microstep mode for
    /// both, which is the A4988 shape.
    unsigned trackingMicrostepRatio() const
    {
        return (raSlewMicrosteps != 0) ? (raTrackMicrosteps / raSlewMicrosteps) : 1u;
    }

    /// The rate an axis runs at while tracking, in microsteps.
    uint16_t raTrackingMicrosteps() const
    {
        return raTrackMicrosteps;
    }

    /// The rate DEC runs at while guiding, in microsteps. Guiding happens at the tracking rate,
    /// so a board that straps one DEC rate gets it back for both.
    uint16_t decGuidingMicrosteps() const
    {
        return decGuideMicrosteps;
    }
};

}  // namespace core
