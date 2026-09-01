#include "Declination.hpp"

namespace core
{

Declination::Declination() : DayTime()
{
}

Declination::Declination(const Declination &other) : DayTime(other)
{
}

Declination::Declination(int h, int m, int s) : DayTime(h, m, s)
{
}

Declination::Declination(float inDegrees) : DayTime(inDegrees)
{
}

void Declination::set(int h, int m, int s)
{
    Declination dt(h, m, s);
    totalSeconds = dt.totalSeconds;
    checkHours();
}

void Declination::addDegrees(int deltaDegrees)
{
    addHours(deltaDegrees);
}

float Declination::getTotalDegrees() const
{
    return getTotalHours();
}

void Declination::checkHours()
{
    if (totalSeconds > arcSecondsPerHemisphere)
    {
        totalSeconds = arcSecondsPerHemisphere;
    }
    if (totalSeconds < -arcSecondsPerHemisphere)
    {
        totalSeconds = -arcSecondsPerHemisphere;
    }
}

Declination Declination::fromTotalSeconds(long totalSeconds)
{
    Declination d;
    d.totalSeconds = totalSeconds;
    d.checkHours();
    return d;
}

long Declination::axisToCelestialSeconds(long axisSeconds, bool northernHemisphere)
{
    // Northern: celestial = 90 - |axis|. Southern: celestial = |axis| - 90.
    // Mirrors the hemisphere tables in src/Declination.cpp.
    const long hemiArcsecs = arcSecondsPerHemisphere / 2;
    return northernHemisphere ? hemiArcsecs - labs(axisSeconds) : labs(axisSeconds) - hemiArcsecs;
}

long Declination::celestialToAxisSeconds(long celestialSeconds, bool northernHemisphere)
{
    // Inverse of axisToCelestialSeconds on the mount's home-side branch:
    // northern mounts keep the axis non-negative, southern non-positive.
    // Note this intentionally does NOT take labs() of the input — celestial
    // values on the far side of the equator push the axis past 90 degrees.
    const long hemiArcsecs = arcSecondsPerHemisphere / 2;
    return northernHemisphere ? hemiArcsecs - celestialSeconds : -hemiArcsecs - celestialSeconds;
}

}  // namespace core
