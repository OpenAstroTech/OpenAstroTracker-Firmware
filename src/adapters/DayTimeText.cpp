#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "adapters/DayTimeText.hpp"

namespace adapters
{
namespace
{
/// One buffer per renderer, so a log line can carry two of them at once. Larger than the 32 bytes
/// the four overlay classes used, which was not enough for `-180:00:00 (-180.0000, -180.0000)`.
constexpr size_t kCoordinateTextCapacity = 48;

/// `types::Declination::arcSecondsPerHemisphere` is protected, and this is the same 180 degrees.
constexpr long kArcSecondsPerHemisphere = 180L * 60L * 60L;

char dayTimeBuffer[kCoordinateTextCapacity];
char latitudeBuffer[kCoordinateTextCapacity];
char longitudeBuffer[kCoordinateTextCapacity];
char declinationBuffer[kCoordinateTextCapacity];
}  // namespace

const char *formatDayTime(const types::DayTime &t)
{
    int hours   = 0;
    int minutes = 0;
    int seconds = 0;
    t.getTime(hours, minutes, seconds);

    // getTime() can hand back a signed hour, so the magnitude is rendered and the sign added once.
    snprintf(dayTimeBuffer,
             sizeof(dayTimeBuffer),
             "%s%02d:%02d:%02d (%.5f)",
             t.getTotalSeconds() < 0 ? "-" : "",
             abs(hours),
             abs(minutes),
             abs(seconds),
             t.getTotalHours());
    return dayTimeBuffer;
}

const char *formatLatitude(const types::Latitude &l)
{
    return l.formatString(latitudeBuffer, "{+}{d}:{m}:{s}");
}

const char *formatLongitude(const types::Longitude &l)
{
    const long totalSeconds = l.getTotalSeconds();
    long wrapped            = totalSeconds;
    if (wrapped < 0)
    {
        wrapped += 360L * 3600L;
    }

    snprintf(longitudeBuffer,
             sizeof(longitudeBuffer),
             "%.2f (%.2f%s",
             static_cast<double>(wrapped) / 3600.0,
             static_cast<double>(labs(totalSeconds)) / 3600.0,
             totalSeconds < 0 ? "W)" : "E)");
    return longitudeBuffer;
}

const char *formatDeclination(const types::Declination &d, bool northernHemisphere)
{
    long celestialSeconds = types::Declination::axisToCelestialSeconds(d.getTotalSeconds(), northernHemisphere);
    d.formatString(declinationBuffer, "{d}*{m}:{s}", &celestialSeconds);

    const size_t used      = strlen(declinationBuffer);
    const size_t remaining = sizeof(declinationBuffer) - used;
    const double display   = northernHemisphere ? 90.0 - fabs(d.getTotalHours()) : -90.0 + fabs(d.getTotalHours());
    snprintf(declinationBuffer + used, remaining, " (%.4f, %.4f)", display, static_cast<double>(d.getTotalHours()));
    return declinationBuffer;
}

types::Declination decFromCelestialArcSeconds(long seconds, bool northernHemisphere)
{
    const auto secondsFloat    = static_cast<float>(seconds);
    const auto halfHemisphere  = static_cast<float>(kArcSecondsPerHemisphere);
    const auto degreesFromZero = (halfHemisphere / 2.0f - secondsFloat) / 3600.0f;
    const auto degreesFromPole = (halfHemisphere / 2.0f + secondsFloat) / 3600.0f;
    return types::Declination(northernHemisphere ? degreesFromZero : degreesFromPole);
}

types::Declination decFromCelestialSeconds(long celestialSeconds, bool northernHemisphere)
{
    return types::Declination::fromTotalSeconds(types::Declination::celestialToAxisSeconds(celestialSeconds, northernHemisphere));
}

}  // namespace adapters
