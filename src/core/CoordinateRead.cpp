#include "CoordinateRead.hpp"

namespace core
{

DayTime currentRa(const PositionFacts &facts)
{
    // u-steps/degree * degrees/hour = u-steps/hour
    const float stepsPerSiderealHour = facts.raStepsPerDegree * kSiderealDegreesPerHour;
    float hourPos                    = -facts.raSteps / stepsPerSiderealHour;

    hourPos += facts.zeroPosRaHours;

    const float degreePos = (facts.decSteps / facts.decStepsPerDegree) + facts.zeroPosDecDegrees;
    if (degreePos < 0)
    {
        hourPos += 12;
        if (hourPos > 24)
        {
            hourPos -= 24;
        }
    }

    if (hourPos < 0)
    {
        hourPos += 24;
    }
    if (hourPos > 24)
    {
        hourPos -= 24;
    }

    return DayTime(hourPos);
}

Declination currentDec(const PositionFacts &facts)
{
    return Declination((facts.decSteps / facts.decStepsPerDegree) + facts.zeroPosDecDegrees);
}

}  // namespace core
