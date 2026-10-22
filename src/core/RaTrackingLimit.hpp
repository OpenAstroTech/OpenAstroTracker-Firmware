#pragma once

#include <stdint.h>

namespace core
{
/// What the RA tracking limit needs to know, gathered by the caller.
///
/// A value rather than a set of accessors: the rule is pure arithmetic over five numbers, and
/// keeping it that way is what lets the folding and the hemisphere flip be tested without a
/// mount, an axis or a clock.
struct RaLimitFacts {
    float homeRaHours;     ///< the RA the home position corresponds to, plus the hours tracked
    float currentRaHours;  ///< `currentRA()` as the mount reports it
    float decAxisDegrees;  ///< the DEC *axis* coordinate (0 = pole above the mount)
    bool northernHemisphere;
    float limitHours;  ///< how far from home the mount may track (RA_TRACKING_LIMIT)
};

/// The answer, and whether tracking has to stop this tick.
struct RaLimitResult {
    float remainingHours;  ///< still available before the limit
    bool limitReached;     ///< the caller stops tracking when this is set
};

/// The RA tracking limit: how long the mount can keep tracking before it wraps its own axis.
///
/// Extracted from `Mount::checkRALimit()` (Phase 6), which held the arithmetic, the log lines
/// and the "stop tracking" side effect in one method. The side effect stays in `Mount` — it owns
/// the axes — and everything that decides *whether* to stop is here.
///
/// The rules, in the order the original applied them:
/// * A DEC position on the far side of the pole reports the opposite half of the hour circle, so
///   the current RA is shifted by 12 hours. Which side is "far" depends on the hemisphere.
/// * The delta from home is folded into ±12 hours, so it reads the same either side of the
///   wrap.
/// * The limit is reached when the folded delta is *greater* than the configured limit; equal
///   is still allowed, which is the boundary the original used.
class RaTrackingLimit
{
  public:
    static RaLimitResult evaluate(const RaLimitFacts &facts)
    {
        float currentRaHours = facts.currentRaHours;

        const bool decAcrossThePole = facts.northernHemisphere ? (facts.decAxisDegrees < 0.0f) : (facts.decAxisDegrees > 0.0f);
        if (decAcrossThePole)
        {
            currentRaHours -= 12.0f;
            if (currentRaHours < 0.0f)
            {
                currentRaHours += 24.0f;
            }
        }

        float deltaFromHome = facts.homeRaHours - currentRaHours;
        while (deltaFromHome > 12.0f)
        {
            deltaFromHome -= 24.0f;
        }
        while (deltaFromHome < -12.0f)
        {
            deltaFromHome += 24.0f;
        }

        RaLimitResult result;
        result.remainingHours = facts.limitHours - deltaFromHome;
        result.limitReached   = deltaFromHome > facts.limitHours;
        return result;
    }
};

}  // namespace core
