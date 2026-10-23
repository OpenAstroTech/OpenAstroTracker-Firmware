#include "SyncAdjustment.hpp"

#include <math.h>

namespace core
{

SyncAdjustment syncAdjustment(
    float currentRaHours, float targetRaHours, float currentDecDegrees, float targetDecDegrees, float decDegreePosition)
{
    SyncAdjustment adjustment;

    adjustment.raHours = targetRaHours - currentRaHours;
    while (adjustment.raHours > 12)
    {
        adjustment.raHours -= 24;
    }
    while (adjustment.raHours < -12)
    {
        adjustment.raHours += 24;
    }

    // Declination is tracked as a signed distance from the pole, so both values are compared
    // by magnitude: -30 deg and +30 deg are the same distance from home.
    adjustment.decDegrees = fabsf(targetDecDegrees) - fabsf(currentDecDegrees);
    if (decDegreePosition < 0)
    {
        adjustment.decDegrees = -adjustment.decDegrees;
    }

    return adjustment;
}

}  // namespace core
