#include "Latitude.hpp"

namespace core
{

Latitude::Latitude() : DayTime()
{
}

Latitude::Latitude(const Latitude &other) : DayTime(other)
{
}

Latitude::Latitude(int h, int m, int s) : DayTime(h, m, s)
{
}

Latitude::Latitude(float inDegrees) : DayTime(inDegrees)
{
}

void Latitude::checkHours()
{
    if (totalSeconds > 90L * 3600L)
    {
        totalSeconds = 90L * 3600L;
    }
    if (totalSeconds < (-90L * 3600L))
    {
        totalSeconds = -90L * 3600L;
    }
}

}  // namespace core
