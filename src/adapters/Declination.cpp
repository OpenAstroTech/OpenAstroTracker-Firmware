#include <math.h>
#include <stdio.h>
#include <string.h>

#include "Configuration.hpp"
#include "ports/ILogger.hpp"
#include "adapters/Declination.hpp"

//////////////////////////////////////////////////////////////////////////////////////
//
// Declination
//
// A class to handle degrees, minutes, seconds of a declination

// In the NORTHERN hemisphere, 0 is north pole, 180 and -180 is south pole
//------------------ S---------------------------------------- N ---------------------------------- S
// Celestial       -90    -60    -30      0      30    60     90    60    30    0    -30   -60    -90
//                                    Celestial = 90 - abs(Declination)
// Declination    -180    -150   -120    -90    -60   -30     0     30    60    90   120   150    180

// In the SOUTHERN hemisphere, 0 is south pole, 180 and -180 is north pole
//------------------ N---------------------------------------- S ---------------------------------- N
// Celestial        90     60     30      0     -30   -60   -90    -60    -30    0    30   60    90
//                                    Celestial = -90 + abs(Declination)
// Declination    -180    -150   -120    -90    -60   -30     0     30    60    90   120   150    180

// Pure arithmetic (constructors, set, addDegrees, getTotalDegrees, checkHours) is in
// core::Declination (src/core/types/Declination.hpp/.cpp). The hemisphere corrections on the way
// to and from the celestial frame are core::Declination::axisToCelestialSeconds() and friends.

namespace
{
/// `ToString()`'s scratch, one buffer per value as the overlay family has always had.
char achBufDeg[32];
}  // namespace

const char *Declination::ToString(bool northernHemisphere) const
{
    long celestialSeconds = core::Declination::axisToCelestialSeconds(totalSeconds, northernHemisphere);
    core::DayTime::formatString(achBufDeg, "{d}*{m}:{s}", &celestialSeconds);

    const size_t used      = strlen(achBufDeg);
    const size_t remaining = sizeof(achBufDeg) - used;
    const float displayVal = northernHemisphere ? 90 - fabsf(getTotalHours()) : -90 + fabsf(getTotalHours());
    const String valStr    = String(displayVal, 4);
    const String hoursStr  = String(getTotalHours(), 4);
    snprintf(achBufDeg + used, remaining, " (%s, %s)", valStr.c_str(), hoursStr.c_str());

    return achBufDeg;
}

Declination Declination::fromCelestialSeconds(long celestialSeconds, bool northernHemisphere)
{
    // The sign lives in the total, so there is no zero-degrees blind spot here.
    Declination result;
    result.totalSeconds = core::Declination::celestialToAxisSeconds(celestialSeconds, northernHemisphere);
    result.checkHours();
    return result;
}

Declination Declination::FromSeconds(long seconds, bool northernHemisphere)
{
    const auto secondsFloat                 = static_cast<float>(seconds);
    const auto arcSecondsPerHemisphereFloat = static_cast<float>(core::Declination::arcSecondsPerHemisphere);
    if (northernHemisphere)
    {
        return Declination(((arcSecondsPerHemisphereFloat / 2.0f) - secondsFloat) / 3600.0f);
    }
    return Declination(((arcSecondsPerHemisphereFloat / 2.0f) + secondsFloat) / 3600.0f);
}
