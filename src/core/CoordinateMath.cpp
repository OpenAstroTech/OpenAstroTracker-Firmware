#include "CoordinateMath.hpp"

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

}  // namespace core
