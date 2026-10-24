#pragma once

#include "core/types/DayTime.hpp"

namespace core
{

/// Pure sidereal time calculation (no Arduino/GPS deps).
/// Uses the algorithm from http://www.stargazing.net/kepler/altaz.html
class SiderealClock
{
  public:
    /// Calculate Local Sidereal Time (LST) from date/time and longitude.
    /// Returns DayTime representing the LST in hours.
    static double calculateTheta(double deltaJ, double longitude, float timeUTC);

    /// Calculate delta-Julian days since J2000.
    static int calculateDeltaJd(int year, int month, int day);

    /// Local sidereal time for a UTC date and time at a longitude, in the same shape the mount
    /// exposes: the delta-Julian computation and the sidereal angle in one call.
    static DayTime localSiderealTime(double longitude, int year, int month, int day, float utcHours);

    /// Hour angle of Polaris from local sidereal time, in hours.
    static DayTime hourAngle(float lstHours, int polarisRaHour, int polarisRaMinute, int polarisRaSecond);

    /// Local sidereal time from a Polaris hour angle — the inverse of `hourAngle()`, and what
    /// `:SH#` (set hour angle) means. The mount has always added the pole star's right ascension
    /// in hour arithmetic, so this does the same.
    static DayTime lstFromHourAngle(const DayTime &ha, int polarisRaHour, int polarisRaMinute, int polarisRaSecond);

    /// Calculate hour angle for Polaris given LST total hours.
    static double calculateHaDegrees(double lstTotalHours, int polarisRaHour = 3, int polarisRaMinute = 0, int polarisRaSecond = 8);
};

}  // namespace core
