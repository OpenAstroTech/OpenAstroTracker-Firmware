#include "./inc/Globals.hpp"
#include "../Configuration.hpp"
#include "Utility.hpp"
#include "Declination.hpp"

//////////////////////////////////////////////////////////////////////////////////////
//
// Declination
//
// A class to handle degrees, minutes, seconds of a declination

// Parses the RA or DEC from a string that has an optional sign, a two digit degree, a seperator, a two digit minute, a seperator and a two digit second.
// For example:   -45*32:11 or 23:44:22

// In the NORTHERN hemisphere, 0 is north pole, 180 and -180 is south pole
//------------------ S---------------------------------------- N ---------------------------------- S
// Celestial       -90    -60    -30      0      30    60     90    60    30    0    -30   -60    -90
//                                    Celestial = 90 - abs(Declination)
// Declination    -180    -150   -120    -90    -60   -30     0     30    60    90   120   150    180

// In the SOUTHERN hemisphere, 0 is south pole, 180 and -180 is north pole
//------------------ N---------------------------------------- S ---------------------------------- N
// Celestial        90     60     30      0     -30   -60     -90    -60    -30    0    30   60    90
//                                    Celestial = -90 + abs(Declination)
// Declination    -180    -150   -120    -90    -60   -30     0     30    60    90   120   150    180
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
        LOG(DEBUG_GENERAL, "[DECLINATION]: CheckHours: Degrees is more than 180, clamping");
        totalSeconds = arcSecondsPerHemisphere;
    }
    if (totalSeconds < -arcSecondsPerHemisphere)
    {
        LOG(DEBUG_GENERAL, "[DECLINATION]: CheckHours: Degrees is less than -180, clamping");
        totalSeconds = -arcSecondsPerHemisphere;
    }
}

char achBufDeg[32];

// Convert to a standard string (like 14:45:06), specifying separators if needed
const char *Declination::ToDisplayString(char sep1, char sep2) const
{
    char achFormat[16];
    sprintf(achFormat, "{d}%c{m}%c{s}", sep1, sep2);
    return formatString(achBufDeg, achFormat);
}

const char *Declination::ToString() const
{
    ToDisplayString('*', ':');

    char *p = achBufDeg + strlen(achBufDeg);

    *p++ = ' ';
    *p++ = '(';
    strcpy(p, String(inNorthernHemisphere ? 90 - fabsf(getTotalHours()) : -90 + fabsf(getTotalHours()), 4).c_str());
    strcat(p, ", ");
    strcat(p, String(getTotalHours(), 4).c_str());
    strcat(p, ")");

    return achBufDeg;
}

Declination Declination::ParseFromMeade(String const &s)
{
    Declination result;
    LOG(DEBUG_MEADE, "[DECLINATION]: Declination.Parse(%s) for %s Hemi", s.c_str(), inNorthernHemisphere ? "N" : "S");

    // Use the DayTime code to parse it...
    DayTime dt = DayTime::ParseFromMeade(s);
    LOG(DEBUG_MEADE, "[DECLINATION]: Declination DayTime is %l secs", dt.getTotalSeconds());

    // ...and then correct for hemisphere
    result.totalSeconds = inNorthernHemisphere ? (arcSecondsPerHemisphere / 2) - dt.getTotalSeconds()
                                               : -(arcSecondsPerHemisphere / 2) - dt.getTotalSeconds();
    LOG(DEBUG_MEADE, "[DECLINATION]: Adjust for hemisphere. %s -> %s (%l secs)", s.c_str(), result.ToString(), result.totalSeconds);
    return result;
}

Declination Declination::FromSeconds(long seconds)
{
    const auto secondsFloat                 = static_cast<float>(seconds);
    const auto arcSecondsPerHemisphereFloat = static_cast<float>(arcSecondsPerHemisphere);
    if (inNorthernHemisphere)
    {
        return Declination(((arcSecondsPerHemisphereFloat / 2.0f) - secondsFloat) / 3600.0f);
    }
    return Declination(((arcSecondsPerHemisphereFloat / 2.0f) + secondsFloat) / 3600.0f);
}

const char *Declination::formatString(char *targetBuffer, const char *format, long *) const
{
    long secs
        = inNorthernHemisphere ? (arcSecondsPerHemisphere / 2) - labs(totalSeconds) : -(arcSecondsPerHemisphere / 2) + labs(totalSeconds);
    return DayTime::formatString(targetBuffer, format, &secs);
}
