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

}  // namespace core
