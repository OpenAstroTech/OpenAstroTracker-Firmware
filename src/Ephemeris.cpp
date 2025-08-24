#include "Ephemeris.hpp"
#include "Sidereal.hpp"
#include <math.h>

// Constants
const double PI = 3.14159265358979323846;
const double DEG_TO_RAD = PI / 180.0;
const double RAD_TO_DEG = 180.0 / PI;
const double J2000 = 2451545.0;  // Julian Date for J2000.0

// Calculate Julian Date
double Ephemeris::calculateJulianDate(int year, int month, int day, const DayTime& utcTime) {
    // Adjust for January and February
    if (month <= 2) {
        year -= 1;
        month += 12;
    }
    
    int A = year / 100;
    int B = 2 - A + (A / 4);
    
    double JD = floor(365.25 * (year + 4716)) + floor(30.6001 * (month + 1)) + day + B - 1524.5;
    JD += utcTime.getTotalHours() / 24.0;
    
    return JD;
}

// Calculate Julian Century from J2000.0
double Ephemeris::calculateJulianCentury(double jd) {
    return (jd - J2000) / 36525.0;
}

// Solar calculations
EphemerisData Ephemeris::calculateSolar(int year, int month, int day, 
                                       const DayTime& utcTime,
                                       const Latitude& latitude, 
                                       const Longitude& longitude) {
    EphemerisData solar;
    
    double jd = calculateJulianDate(year, month, day, utcTime);
    double T = calculateJulianCentury(jd);
    
    // Calculate mean anomaly
    double M = calculateSolarMeanAnomaly(T);
    
    // Calculate equation of center
    double C = calculateSolarEquationOfCenter(M);
    
    // Calculate true longitude
    double lambda = calculateSolarTrueLongitude(T, M, C);
    
    // Calculate declination
    solar.dec = calculateSolarDeclination(lambda);
    
    // Calculate right ascension
    solar.ra = calculateSolarRightAscension(lambda);
    
    // Calculate rates (approximate)
    // Solar RA rate is approximately 0 (mean solar time)
    // Solar DEC rate varies throughout the year
    solar.raRate = 0.0;  // hours/hour (relative to mean solar time)
    
    // DEC rate calculation (maximum at equinoxes, zero at solstices)
    // Maximum rate is about 0.4 degrees/day at equinoxes
    double dayOfYear = jd - calculateJulianDate(year, 1, 1, DayTime(0, 0, 0)) + 1;
    double angleFromEquinox = (dayOfYear - 80) * 2.0 * PI / 365.25;  // 80 is approx spring equinox day
    solar.decRate = 0.4 * cos(angleFromEquinox) / 24.0;  // degrees/hour
    
    // Calculate horizontal coordinates for display
    DayTime lst = Sidereal::calculateByDateAndTime(longitude.getTotalHours(), year, month, day, const_cast<DayTime*>(&utcTime));
    equatorialToHorizontal(solar.ra, solar.dec, lst, latitude, solar.altitude, solar.azimuth);
    
    solar.phase = 0;  // Not applicable for sun
    
    return solar;
}

// Lunar calculations (simplified)
EphemerisData Ephemeris::calculateLunar(int year, int month, int day,
                                       const DayTime& utcTime,
                                       const Latitude& latitude,
                                       const Longitude& longitude) {
    EphemerisData lunar;
    
    double jd = calculateJulianDate(year, month, day, utcTime);
    double T = calculateJulianCentury(jd);
    
    // Simplified lunar position calculation
    // For more accuracy, use full ELP2000 or similar theory
    
    // Mean longitude
    double L = calculateLunarMeanLongitude(T);
    
    // Mean anomaly
    double M = calculateLunarMeanAnomaly(T);
    
    // Mean elongation (unused but kept for future improvements)
    // double D = calculateLunarMeanElongation(T);
    
    // Argument of latitude (unused but kept for future improvements)
    // double F = calculateLunarArgumentOfLatitude(T);
    
    // Calculate position
    float moon_longitude, latitude_moon;
    calculateLunarPosition(T, moon_longitude, latitude_moon);
    
    // Convert to RA/DEC
    // Simplified conversion (ignoring nutation and other corrections)
    double epsilon = 23.439291 - 0.0130042 * T;  // Obliquity of ecliptic
    double lambda = moon_longitude * DEG_TO_RAD;
    double beta = latitude_moon * DEG_TO_RAD;
    
    double ra_rad = atan2(sin(lambda) * cos(epsilon * DEG_TO_RAD) - tan(beta) * sin(epsilon * DEG_TO_RAD), cos(lambda));
    double dec_rad = asin(sin(beta) * cos(epsilon * DEG_TO_RAD) + cos(beta) * sin(epsilon * DEG_TO_RAD) * sin(lambda));
    
    lunar.ra = normalizeAngle(ra_rad * RAD_TO_DEG) / 15.0;  // Convert to hours
    lunar.dec = dec_rad * RAD_TO_DEG;
    
    // Calculate rates
    // Lunar motion is approximately 13.2 degrees/day in longitude
    // This translates to different RA/DEC rates depending on position
    lunar.raRate = -0.549 / 15.0;  // Approximate: -0.549 deg/hour in RA = -0.0366 hours/hour
    
    // DEC rate varies considerably
    // Maximum is about ±5 degrees/day
    double moonAngle = fmod(L + M, 360.0);
    lunar.decRate = 5.0 * sin(moonAngle * DEG_TO_RAD) / 24.0;  // degrees/hour
    
    // Calculate horizontal coordinates for display
    DayTime lst = Sidereal::calculateByDateAndTime(longitude.getTotalHours(), year, month, day, const_cast<DayTime*>(&utcTime));
    equatorialToHorizontal(lunar.ra, lunar.dec, lst, latitude, lunar.altitude, lunar.azimuth);
    
    // Calculate phase
    lunar.phase = calculateLunarPhase(T);
    
    return lunar;
}

// Solar calculation helpers
float Ephemeris::calculateSolarMeanAnomaly(double T) {
    return normalizeDegrees(357.52911 + 35999.05029 * T - 0.0001537 * T * T);
}

float Ephemeris::calculateSolarEquationOfCenter(double M) {
    double M_rad = M * DEG_TO_RAD;
    return (1.914602 - 0.004817 * M - 0.000014 * M * M) * sin(M_rad)
         + (0.019993 - 0.000101 * M) * sin(2 * M_rad)
         + 0.000289 * sin(3 * M_rad);
}

float Ephemeris::calculateSolarTrueLongitude(double T, double M, double C) {
    double L0 = normalizeDegrees(280.46646 + 36000.76983 * T + 0.0003032 * T * T);
    return normalizeDegrees(L0 + C);
}

float Ephemeris::calculateSolarDeclination(double lambda) {
    double epsilon = 23.439291;  // Obliquity of ecliptic (simplified)
    return radiansToDegrees(asin(sin(degreesToRadians(epsilon)) * sin(degreesToRadians(lambda))));
}

float Ephemeris::calculateSolarRightAscension(double lambda) {
    double epsilon = 23.439291;  // Obliquity of ecliptic (simplified)
    double ra = atan2(cos(degreesToRadians(epsilon)) * sin(degreesToRadians(lambda)), 
                     cos(degreesToRadians(lambda)));
    return normalizeAngle(radiansToDegrees(ra)) / 15.0;  // Convert to hours
}

// Lunar calculation helpers
float Ephemeris::calculateLunarMeanLongitude(double T) {
    return normalizeDegrees(218.3164477 + 481267.88123421 * T - 0.0015786 * T * T);
}

float Ephemeris::calculateLunarMeanAnomaly(double T) {
    return normalizeDegrees(134.9633964 + 477198.8675055 * T + 0.0087414 * T * T);
}

float Ephemeris::calculateLunarMeanElongation(double T) {
    return normalizeDegrees(297.8501921 + 445267.1114034 * T - 0.0018819 * T * T);
}

float Ephemeris::calculateLunarArgumentOfLatitude(double T) {
    return normalizeDegrees(93.2720950 + 483202.0175233 * T - 0.0036539 * T * T);
}

void Ephemeris::calculateLunarPosition(double T, float& longitude, float& latitude) {
    // Simplified calculation
    double L = calculateLunarMeanLongitude(T);
    double M = calculateLunarMeanAnomaly(T);
    double F = calculateLunarArgumentOfLatitude(T);
    
    // Main perturbations
    longitude = L + 6.289 * sin(M * DEG_TO_RAD);
    latitude = 5.128 * sin(F * DEG_TO_RAD);
    
    longitude = normalizeDegrees(longitude);
}

float Ephemeris::calculateLunarPhase(double T) {
    // Calculate phase angle
    double D = calculateLunarMeanElongation(T);
    // Phase: 0 = New Moon, 50 = First/Last Quarter, 100 = Full Moon
    return (1.0 - cos(D * DEG_TO_RAD)) * 50.0;
}

// Convert equatorial to horizontal coordinates
void Ephemeris::equatorialToHorizontal(float ra, float dec, 
                                      const DayTime& lst,
                                      const Latitude& latitude,
                                      float& altitude, float& azimuth) {
    double ha = (lst.getTotalHours() - ra) * 15.0;  // Hour angle in degrees
    double ha_rad = ha * DEG_TO_RAD;
    double dec_rad = dec * DEG_TO_RAD;
    double lat_rad = latitude.getTotalHours() * DEG_TO_RAD;
    
    // Calculate altitude
    double sin_alt = sin(dec_rad) * sin(lat_rad) + cos(dec_rad) * cos(lat_rad) * cos(ha_rad);
    altitude = radiansToDegrees(asin(sin_alt));
    
    // Calculate azimuth
    double cos_az = (sin(dec_rad) - sin_alt * sin(lat_rad)) / (cos(asin(sin_alt)) * cos(lat_rad));
    double sin_az = -cos(dec_rad) * sin(ha_rad) / cos(asin(sin_alt));
    
    azimuth = radiansToDegrees(atan2(sin_az, cos_az));
    azimuth = normalizeDegrees(azimuth + 180.0);  // Convert to 0-360 from North
}

// Utility functions
float Ephemeris::normalizeAngle(float angle) {
    while (angle < 0) angle += 360.0;
    while (angle >= 360.0) angle -= 360.0;
    return angle;
}

float Ephemeris::normalizeDegrees(float degrees) {
    return normalizeAngle(degrees);
}

float Ephemeris::degreesToRadians(float degrees) {
    return degrees * DEG_TO_RAD;
}

float Ephemeris::radiansToDegrees(float radians) {
    return radians * RAD_TO_DEG;
}
