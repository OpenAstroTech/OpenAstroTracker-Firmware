#pragma once

namespace core
{

/// A declination target after the configured travel limits have been applied.
struct DecLimitResult {
    /// The position to move to, in slewing microsteps.
    float steps = 0.0f;

    /// Whether a limit actually moved the target, so the caller can report it.
    bool clampedToUpper = false;
    bool clampedToLower = false;
};

/// Keeps a declination target inside the configured limits.
///
/// A limit of 0 means "no limit" — the firmware has always encoded "not set" that way, so an
/// uncalibrated mount can be moved anywhere.
DecLimitResult clampDecStepsToLimits(float targetSteps, long lowerLimitSteps, long upperLimitSteps);

}  // namespace core
