#include "RightAscension.hpp"

namespace core
{

RightAscension::RightAscension() : DayTime()
{
}

RightAscension::RightAscension(const RightAscension &other) : DayTime(other)
{
}

RightAscension::RightAscension(int h, int m, int s) : DayTime(h, m, s)
{
    checkHours();
}

RightAscension::RightAscension(float timeInHours) : DayTime(timeInHours)
{
    checkHours();
}

void RightAscension::set(int h, int m, int s)
{
    RightAscension dt(h, m, s);
    totalSeconds = dt.totalSeconds;
    checkHours();
}

void RightAscension::addHours(float deltaHours)
{
    totalSeconds += static_cast<long>(deltaHours * 3600L);
    checkHours();
}

float RightAscension::getTotalHours() const
{
    return DayTime::getTotalHours();
}

void RightAscension::checkHours()
{
    while (totalSeconds >= 86400L)
    {
        totalSeconds -= 86400L;
    }

    while (totalSeconds < 0)
    {
        totalSeconds += 86400L;
    }
}

}  // namespace core
