#include "ManualSlew.hpp"

namespace core
{

namespace
{
bool has(uint8_t direction, SlewDirection bit)
{
    return (direction & static_cast<uint8_t>(bit)) != 0;
}
}  // namespace

ManualSlewTargets manualSlewTargets(uint8_t direction, const ManualSlewFacts &facts)
{
    ManualSlewTargets targets;

    // The hemisphere flips which way the RA ring runs.
    const int sign = facts.northernHemisphere ? 1 : -1;

    if (has(direction, SlewDirection::North))
    {
        // Full travel, unless the axis has been calibrated to a tighter limit.
        long target = static_cast<long>(facts.decStepsPerDegree * facts.decLimitUpDegrees);
        if (facts.decUpperLimitSteps != 0)
        {
            target = facts.decUpperLimitSteps;
        }
        targets.movesDec = true;
        targets.decSteps = target;
    }

    if (has(direction, SlewDirection::South))
    {
        long target = static_cast<long>(-facts.decStepsPerDegree * facts.decLimitDownDegrees);
        if (facts.decLowerLimitSteps != 0)
        {
            target = facts.decLowerLimitSteps;
        }
        // Programmed second when both directions are requested, so it wins.
        targets.movesDec = true;
        targets.decSteps = target;
    }

    if (has(direction, SlewDirection::East))
    {
        // Subtracts the distance tracked from the physical home coordinate.
        const long travel = static_cast<long>(facts.raStepsPerDegree * 15.0f * (facts.raPhysicalLimitDegrees + facts.trackedHours));
        targets.movesRa   = true;
        targets.raSteps   = -sign * travel;
    }

    if (has(direction, SlewDirection::West))
    {
        // Adds the tracked distance, but never past the tracking limit.
        const float limit
            = (facts.raTrackingLimitDegrees < facts.raPhysicalLimitDegrees) ? facts.raTrackingLimitDegrees : facts.raPhysicalLimitDegrees;
        const long travel = static_cast<long>(facts.raStepsPerDegree * 15.0f * (limit - facts.trackedHours));
        targets.movesRa   = true;
        targets.raSteps   = sign * travel;
    }

    return targets;
}

}  // namespace core
