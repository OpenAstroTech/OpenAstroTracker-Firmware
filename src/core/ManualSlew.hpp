#pragma once

#include <stdint.h>

namespace core
{

/// The direction a manual slew was asked for, as bits.
///
/// The values match the `B0xxx` constants `Mount` has always used; `Mount.cpp` asserts the
/// agreement at compile time.
enum class SlewDirection : uint8_t
{
    None     = 0x00,
    North    = 0x01,
    East     = 0x02,
    South    = 0x04,
    West     = 0x08,
    All      = 0x0F,
    Tracking = 0x10,
};

/// Everything a manual slew's targets depend on.
///
/// This is deliberately a snapshot of settings rather than a reference to them: the arithmetic
/// below is the whole decision, and a value in and a value out is what makes it testable.
struct ManualSlewFacts {
    float raStepsPerDegree       = 0.0f;
    float decStepsPerDegree      = 0.0f;
    float raPhysicalLimitDegrees = 0.0f;  ///< the RA ring's travel from home, in hours
    float raTrackingLimitDegrees = 0.0f;  ///< the point past which tracking must stop
    float decLimitUpDegrees      = 0.0f;  ///< build-time default, used when uncalibrated
    float decLimitDownDegrees    = 0.0f;
    long decUpperLimitSteps      = 0;  ///< calibrated limit; 0 means "not calibrated"
    long decLowerLimitSteps      = 0;
    float trackedHours           = 0.0f;
    bool northernHemisphere      = true;
};

/// Where a manual slew programs the axes, in slewing microsteps.
///
/// `movesRa`/`movesDec` say whether the request touches that axis at all; when both a positive
/// and a negative direction are requested the later one is programmed last and wins, exactly as
/// the sequential `moveTo` calls did.
struct ManualSlewTargets {
    bool movesDec = false;
    bool movesRa  = false;
    long decSteps = 0;
    long raSteps  = 0;
};

ManualSlewTargets manualSlewTargets(uint8_t direction, const ManualSlewFacts &facts);

}  // namespace core
