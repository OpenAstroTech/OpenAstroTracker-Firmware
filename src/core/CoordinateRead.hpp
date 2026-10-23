#pragma once

#include "core/types/DayTime.hpp"
#include "core/types/Declination.hpp"

namespace core
{

/// The numbers that turn stepper positions into celestial coordinates.
///
/// Gathered by the mount from its axes and its two zero-points, so the arithmetic below has no
/// state of its own and runs on the host.
struct PositionFacts {
    /// Current stepper positions, in slewing microsteps.
    long raSteps;
    long decSteps;
    /// Slew-mode microsteps per degree for each axis.
    float raStepsPerDegree;
    float decStepsPerDegree;
    /// The RA the axis reads at position 0, in hours (after the 6-hour mount shift).
    float zeroPosRaHours;
    /// The DEC offset the axis reads at position 0, in degrees.
    float zeroPosDecDegrees;
};

/// Sidereal degrees per hour of right ascension.
constexpr float kSiderealDegreesPerHour = 14.95904348958f;

/// Current right ascension from the two axes. A mount whose DEC is below the pole is read on the
/// other side of the sky, which is the 12-hour flip.
DayTime currentRa(const PositionFacts &facts);

/// Current declination from the DEC axis.
Declination currentDec(const PositionFacts &facts);

}  // namespace core
