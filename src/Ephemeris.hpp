#ifndef _EPHEMERIS_HPP_
#define _EPHEMERIS_HPP_

#include "DayTime.hpp"
#include "Latitude.hpp"
#include "Longitude.hpp"

// Structure to hold ephemeris data
struct EphemerisData {
    float ra;           // Right Ascension in hours
    float dec;          // Declination in degrees
    float raRate;       // RA rate in hours/hour
    float decRate;      // DEC rate in degrees/hour
    float altitude;     // Altitude in degrees (for display)
    float azimuth;      // Azimuth in degrees (for display)
    float phase;        // Moon phase (0-100%) or solar elongation
};

class Ephemeris {
public:
    // Calculate solar position and rates
    static EphemerisData calculateSolar(int year, int month, int day, 
                                       const DayTime& utcTime,
                                       const Latitude& latitude, 
                                       const Longitude& longitude);
    
    // Calculate lunar position and rates
    static EphemerisData calculateLunar(int year, int month, int day,
                                       const DayTime& utcTime,
                                       const Latitude& latitude,
                                       const Longitude& longitude);
    
    // Calculate Julian Date
    static double calculateJulianDate(int year, int month, int day, const DayTime& utcTime);
    
    // Calculate Julian Century from J2000.0
    static double calculateJulianCentury(double jd);
    
    // Convert equatorial to horizontal coordinates
    static void equatorialToHorizontal(float ra, float dec, 
                                      const DayTime& lst,
                                      const Latitude& latitude,
                                      float& altitude, float& azimuth);
    
private:
    // Solar calculation helpers
    static float calculateSolarMeanAnomaly(double T);
    static float calculateSolarEquationOfCenter(double M);
    static float calculateSolarTrueLongitude(double T, double M, double C);
    static float calculateSolarDeclination(double lambda);
    static float calculateSolarRightAscension(double lambda);
    
    // Lunar calculation helpers
    static float calculateLunarMeanLongitude(double T);
    static float calculateLunarMeanAnomaly(double T);
    static float calculateLunarMeanElongation(double T);
    static float calculateLunarArgumentOfLatitude(double T);
    static void calculateLunarPosition(double T, float& longitude, float& latitude);
    static float calculateLunarPhase(double T);
    
    // Utility functions
    static float normalizeAngle(float angle);
    static float normalizeDegrees(float degrees);
    static float degreesToRadians(float degrees);
    static float radiansToDegrees(float radians);
};

#endif // _EPHEMERIS_HPP_
