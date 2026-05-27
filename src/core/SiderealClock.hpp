#pragma once

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

    /// Calculate hour angle for Polaris given LST total hours.
    static double calculateHaDegrees(double lstTotalHours, int polarisRaHour = 3, int polarisRaMinute = 0, int polarisRaSecond = 8);
};

}  // namespace core
