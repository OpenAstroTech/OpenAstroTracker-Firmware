#include "SiderealClock.hpp"
#include <math.h>

namespace core
{

// Constants for sidereal calculation
// Source: http://www.stargazing.net/kepler/altaz.html
static const double C1      = 100.46;
static const double C2      = 0.985647;
static const double C3      = 15.0;
static const double C4      = -0.5125;
static const unsigned J2000 = 2000;

double SiderealClock::calculateTheta(double deltaJ, double longitude, float timeUTC)
{
    double theta = C1;
    theta += C2 * deltaJ;
    theta += C3 * static_cast<double>(timeUTC);
    theta += C4;
    theta += longitude;
    return fmod(theta, 360.0);
}

int SiderealClock::calculateDeltaJd(int year, int month, int day)
{
    const int daysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    // Calculating days without leapdays
    long int deltaJd = (year - static_cast<int>(J2000)) * 365 + day;
    for (int i = 0; i < month - 1; i++)
    {
        deltaJd += daysInMonth[i];
    }

    // Add leapdays
    if (month <= 2)
    {
        // Not counting current year if we have not passed february yet
        year--;
    }
    deltaJd += year / 4 - year / 100 + year / 400;
    deltaJd -= static_cast<int>(J2000) / 4 - static_cast<int>(J2000) / 100 + static_cast<int>(J2000) / 400;
    return static_cast<int>(deltaJd);
}

double SiderealClock::calculateHaDegrees(double lstTotalHours, int polarisRaHour, int polarisRaMinute, int polarisRaSecond)
{
    double lstDeg = lstTotalHours * 15.0;  // to deg

    // subtract Polaris RA
    lstDeg -= ((static_cast<double>(polarisRaSecond) / 3600.0 + static_cast<double>(polarisRaMinute) / 60.0
                + static_cast<double>(polarisRaHour))
               * 15.0);

    // ensure positive deg
    while (lstDeg < 0.0)
    {
        lstDeg += 360.0;
    }

    return lstDeg;
}

}  // namespace core
