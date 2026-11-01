#pragma once

namespace core
{
/// How much faster than the tracking rate a guiding pulse moves each axis.
///
/// RA pulses are `(multiplier + 1)` and `(multiplier - 1)` of the tracking rate; DEC pulses are
/// `+multiplier` and `-multiplier`. The multipliers are build-time tuning values, so they travel
/// as one value.
struct GuideConfig {
    float raPulseMultiplier  = 1.5f;
    float decPulseMultiplier = 0.5f;
};

}  // namespace core
