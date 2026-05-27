#include "inc/Globals.hpp"
#include "../Configuration.hpp"
#include "Sidereal.hpp"
#include "core/SiderealClock.hpp"

#if USE_GPS == 1
PUSH_NO_WARNINGS
    #include <TinyGPS++.h>
POP_NO_WARNINGS
DayTime Sidereal::calculateByGPS(TinyGPSPlus *gps)
{
    DayTime timeUTC = DayTime(gps->time.hour(), gps->time.minute(), gps->time.second());
    int deltaJd     = core::SiderealClock::calculateDeltaJd(gps->date.year(), gps->date.month(), gps->date.day());
    double deltaJ   = static_cast<float>(deltaJd) + (timeUTC.getTotalHours() / 24.0f);
    return DayTime(static_cast<float>(core::SiderealClock::calculateTheta(deltaJ, gps->location.lng(), timeUTC.getTotalHours()) / 15.0));
}
#endif  // USE_GPS

DayTime Sidereal::calculateByDateAndTime(double longitude, int year, int month, int day, DayTime *timeUTC)
{
    int deltaJd   = core::SiderealClock::calculateDeltaJd(year, month, day);
    double deltaJ = deltaJd + ((timeUTC->getTotalHours()) / 24.0f);
    return DayTime(static_cast<float>(core::SiderealClock::calculateTheta(deltaJ, longitude, timeUTC->getTotalHours()) / 15.0));
}

double Sidereal::calculateTheta(double deltaJ, double longitude, float timeUTC)
{
    return core::SiderealClock::calculateTheta(deltaJ, longitude, timeUTC);
}

int Sidereal::calculateDeltaJd(int year, int month, int day)
{
    return core::SiderealClock::calculateDeltaJd(year, month, day);
}

DayTime Sidereal::calculateHa(float lstTotalHours)
{
    float lstDeg = static_cast<float>(
        core::SiderealClock::calculateHaDegrees(static_cast<double>(lstTotalHours), POLARIS_RA_HOUR, POLARIS_RA_MINUTE, POLARIS_RA_SECOND));

    return DayTime(lstDeg / 15.0f);
}