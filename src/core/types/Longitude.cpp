#include "Longitude.hpp"

namespace core
{

Longitude::Longitude() : DayTime()
{
}

Longitude::Longitude(const Longitude &other) : DayTime(other)
{
}

Longitude::Longitude(int h, int m, int s) : DayTime(h, m, s)
{
}

Longitude::Longitude(float inDegrees) : DayTime(inDegrees)
{
}

void Longitude::checkHours()
{
    while (totalSeconds > 180L * 3600L)
    {
        totalSeconds -= 360L * 3600L;
    }
    while (totalSeconds < (-180L * 3600L))
    {
        totalSeconds += 360L * 3600L;
    }
}

}  // namespace core
