#include "../Configuration.hpp"
#include "Utility.hpp"
#include "Latitude.hpp"

//////////////////////////////////////////////////////////////////////////////////////
//
// 90 is north pole, -90 is south pole
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
        LOG(DEBUG_GENERAL, "[LATITUDE]: CheckHours: Degrees is more than 90, clamping");
        totalSeconds = 90L * 3600L;
    }
    if (totalSeconds < (-90L * 3600L))
    {
        LOG(DEBUG_GENERAL, "[LATITUDE]: CheckHours: Degrees is less than -90, clamping");
        totalSeconds = -90L * 3600L;
    }
}

Latitude Latitude::ParseFromMeade(String const &s)
{
    Latitude result(0.0);

    LOG(DEBUG_MEADE, "[LATITUDE]: Latitude.Parse(%s)", s.c_str());
    // Use the DayTime code to parse it.
    DayTime dt          = DayTime::ParseFromMeade(s);
    result.totalSeconds = dt.getTotalSeconds();
    result.checkHours();
    LOG(DEBUG_MEADE, "[LATITUDE]: Latitude.Parse(%s) -> %s", s.c_str(), result.ToString());
    return result;
}
