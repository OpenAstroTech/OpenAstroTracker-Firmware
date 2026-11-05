#pragma once

namespace core
{

/// Input geometry for the slew calculation (pure, no Arduino deps).
struct SlewGeometry {
    float moveRA;                /// Target RA in hours, normalized to [-12, 12]
    float moveDEC;               /// Target DEC in degrees (already hemisphere-corrected)
    float homeTargetDeltaRA;     /// Delta between target RA and tracked home position (hours)
    float stepsPerSiderealHour;  /// u-steps per sidereal hour
    float stepsPerDECDegree;     /// u-steps per DEC degree
    float zeroPosDEC;            /// Accumulated DEC offset from sync (degrees)
    float raLimitL;              /// Lower RA limit (hours, negative)
    float raLimitR;              /// Upper RA limit (hours, positive)
};

/// The candidate the selector picked.
enum class SlewSolution
{
    Direct,     ///< Solution 1: target is within the RA limits, no flip
    FlipUpper,  ///< Solution 2: target past the upper RA limit, both axes inverted
    FlipLower,  ///< Solution 3: target past the lower RA limit, both axes inverted
};

/// Result of the slew calculation.
struct SlewResult {
    long targetRASteps;
    long targetDECSteps;

    /// Three candidate solutions (6 values: RA,DEC, RA,DEC, RA,DEC)
    long solutions[6];

    /// Which candidate was selected.
    SlewSolution selected;

    /// RA (hours) and DEC (degrees) after any meridian flip but before the
    /// accumulated sync offset (zeroPosDEC) is applied.
    float adjustedRA;
    float adjustedDEC;
};

/// Calculate RA and DEC stepper target positions given the geometry.
/// Implements the meridian-flip solution selector:
/// - Solution 1: direct (no flip)
/// - Solution 2: flip when past upper limit (moveRA -= 12, moveDEC = -moveDEC)
/// - Solution 3: flip when past lower limit (moveRA += 12, moveDEC = -moveDEC)
SlewResult calculateStepperPositions(const SlewGeometry &geo);

}  // namespace core
