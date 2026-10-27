#include "adapters/InfoDisplayLayout.hpp"

#include <math.h>

namespace adapters
{

int xRAPixel(const InfoDisplayScale &scale, float raHours)
{
    const float rangeRa = scale.rightRa - scale.leftRa;
    const int offset    = 4 + static_cast<int>(round(1.0f * (scale.raSize - 9) * ((raHours - scale.leftRa) / rangeRa)));
    return scale.leftEdgeMount + offset;
}

int yDECPixel(const InfoDisplayScale &scale, float decDegrees)
{
    const int offset = static_cast<int>(round(1.0f * scale.decSize * ((decDegrees - scale.bottomDec) / scale.rangeDec)));
    return scale.topEdgeMount + scale.decSize - offset;
}

int progressBarWidth(int percent)
{
    return static_cast<int>(round(1.28f * percent));
}

float markerRaHours(long trackingSteps, long slewSteps, float raStepsPerDegree, int trackingMicrosteps, int slewMicrosteps)
{
    const float ratio   = 1.0f * trackingMicrosteps / slewMicrosteps;
    const float tracked = 1.0f * trackingSteps / ratio;
    return (tracked + slewSteps) / raStepsPerDegree / 15.0f;
}

float markerDecDegrees(long decSteps, float decStepsPerDegree)
{
    return decSteps / decStepsPerDegree;
}

}  // namespace adapters
