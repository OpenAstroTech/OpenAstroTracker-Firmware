#include "../Configuration.hpp"
#include "Utility.hpp"
#include "Latitude.hpp"

//////////////////////////////////////////////////////////////////////////////////////
//
// Latitude — Arduino-dependent overlay methods
// Pure arithmetic is in core::Latitude (src/core/types/Latitude.hpp/.cpp)
//
// 90 is north pole, -90 is south pole
Latitude::Latitude(const Latitude &other) : core::Latitude(other)
{
}

Latitude::Latitude(int h, int m, int s) : core::Latitude(h, m, s)
{
}

Latitude::Latitude(float inDegrees) : core::Latitude(inDegrees)
{
}

Latitude Latitude::ParseFromMeade(String const &s)
{
    Latitude result(0.0);

    LOG(DEBUG_MEADE, "[LATITUDE]: Latitude.Parse(%s)", s.c_str());
    // Use the DayTime code to parse it.
    ::DayTime dt        = ::DayTime::ParseFromMeade(s);
    result.totalSeconds = dt.getTotalSeconds();
    result.checkHours();
    LOG(DEBUG_MEADE, "[LATITUDE]: Latitude.Parse(%s) -> %s", s.c_str(), result.ToString());
    return result;
}
