#include "CoordinateMath.hpp"

#include "CoordinateRead.hpp"

namespace core
{

SlewResult calculateStepperPositions(const SlewGeometry &geo)
{
    SlewResult result;

    float moveRA  = geo.moveRA;
    float moveDEC = geo.moveDEC;

    // Pre-compute all three candidate solutions. The RA candidates truncate the
    // hour delta to whole hours before scaling, which is the rounding the mount
    // has always used for this diagnostic output; only the selected target below
    // is scaled from the unrounded delta.
    result.solutions[0] = static_cast<long>(static_cast<long>(-moveRA) * geo.stepsPerSiderealHour);
    result.solutions[1] = static_cast<long>(static_cast<long>(moveDEC) * geo.stepsPerDECDegree);
    result.solutions[2] = static_cast<long>(static_cast<long>(-(moveRA - 12.0f)) * geo.stepsPerSiderealHour);
    result.solutions[3] = static_cast<long>(static_cast<long>(-moveDEC) * geo.stepsPerDECDegree);
    result.solutions[4] = static_cast<long>(static_cast<long>(-(moveRA + 12.0f)) * geo.stepsPerSiderealHour);
    result.solutions[5] = static_cast<long>(static_cast<long>(-moveDEC) * geo.stepsPerDECDegree);

    // Select the appropriate solution based on RA limits
    result.selected = SlewSolution::Direct;
    if (geo.homeTargetDeltaRA > geo.raLimitR)
    {
        // Past upper limit: flip both axes (Solution 2)
        moveRA -= 12.0f;
        moveDEC         = -moveDEC;
        result.selected = SlewSolution::FlipUpper;
    }
    else if (geo.homeTargetDeltaRA < geo.raLimitL)
    {
        // Past lower limit: flip both axes (Solution 3)
        moveRA += 12.0f;
        moveDEC         = -moveDEC;
        result.selected = SlewSolution::FlipLower;
    }
    // else: Solution 1 (direct, no flip)

    result.adjustedRA  = moveRA;
    result.adjustedDEC = moveDEC;

    // Apply DEC zero offset
    moveDEC -= geo.zeroPosDEC;

    result.targetRASteps  = static_cast<long>(-moveRA * geo.stepsPerSiderealHour);
    result.targetDECSteps = static_cast<long>(moveDEC * geo.stepsPerDECDegree);

    return result;
}

SlewGeometry buildSlewGeometry(const SlewGeometryFacts &facts)
{
    SlewGeometry geometry;

    // The target relative to the RA zero point, folded into +-12 h so the mount takes the
    // shorter way round. The original only ever subtracted 24 h, because the value it started
    // from was already in [0, 24).
    float moveRA = facts.targetRaHours - facts.zeroPosRaHours;
    while (moveRA > 12.0f)
    {
        moveRA -= 24.0f;
    }
    while (moveRA < -12.0f)
    {
        moveRA += 24.0f;
    }

    // How far the target is from the home position as the tracker has moved it. This one is
    // wrapped in both directions: the target and the tracked home can be on either side.
    const float homeRA      = facts.zeroPosRaHours + facts.trackedHours;
    float homeTargetDeltaRA = facts.targetRaHours - homeRA;
    while (homeTargetDeltaRA > 12.0f)
    {
        homeTargetDeltaRA -= 24.0f;
    }
    while (homeTargetDeltaRA < -12.0f)
    {
        homeTargetDeltaRA += 24.0f;
    }

    geometry.moveRA               = moveRA;
    geometry.moveDEC              = facts.northernHemisphere ? facts.targetDecDegrees : -facts.targetDecDegrees;
    geometry.homeTargetDeltaRA    = homeTargetDeltaRA;
    geometry.stepsPerSiderealHour = facts.raStepsPerDegree * kSiderealDegreesPerHour;
    geometry.stepsPerDECDegree    = facts.decStepsPerDegree;
    geometry.zeroPosDEC           = facts.zeroPosDecDegrees;
    geometry.raLimitL             = facts.northernHemisphere ? -facts.raLimitLeftHours : -facts.raLimitRightHours;
    geometry.raLimitR             = facts.northernHemisphere ? facts.raLimitRightHours : facts.raLimitLeftHours;

    return geometry;
}

}  // namespace core
