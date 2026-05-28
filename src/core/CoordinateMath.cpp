#include "CoordinateMath.hpp"

namespace core
{

SlewResult calculateStepperPositions(const SlewGeometry &geo)
{
    SlewResult result;

    float moveRA  = geo.moveRA;
    float moveDEC = geo.moveDEC;

    // Pre-compute all three candidate solutions
    result.solutions[0] = static_cast<long>(-moveRA) * static_cast<long>(geo.stepsPerSiderealHour);
    result.solutions[1] = static_cast<long>(moveDEC) * static_cast<long>(geo.stepsPerDECDegree);
    result.solutions[2] = static_cast<long>(-(moveRA - 12.0f)) * static_cast<long>(geo.stepsPerSiderealHour);
    result.solutions[3] = static_cast<long>(-moveDEC) * static_cast<long>(geo.stepsPerDECDegree);
    result.solutions[4] = static_cast<long>(-(moveRA + 12.0f)) * static_cast<long>(geo.stepsPerSiderealHour);
    result.solutions[5] = static_cast<long>(-moveDEC) * static_cast<long>(geo.stepsPerDECDegree);

    // Select the appropriate solution based on RA limits
    if (geo.homeTargetDeltaRA > geo.raLimitR)
    {
        // Past upper limit: flip both axes (Solution 2)
        moveRA -= 12.0f;
        moveDEC = -moveDEC;
    }
    else if (geo.homeTargetDeltaRA < geo.raLimitL)
    {
        // Past lower limit: flip both axes (Solution 3)
        moveRA += 12.0f;
        moveDEC = -moveDEC;
    }
    // else: Solution 1 (direct, no flip)

    // Apply DEC zero offset
    moveDEC -= geo.zeroPosDEC;

    result.targetRASteps  = static_cast<long>(-moveRA * geo.stepsPerSiderealHour);
    result.targetDECSteps = static_cast<long>(moveDEC * geo.stepsPerDECDegree);

    return result;
}

}  // namespace core
