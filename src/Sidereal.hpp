#pragma once

#include <math.h>
#include "DayTime.hpp"

// Forward declaration
class TinyGPSPlus;

class Sidereal
{
 public:
    static DayTime calculateByGPS(TinyGPSPlus* gps);

    static DayTime calculateByDateAndTime( double longitude, int year, int month, int day, DayTime *timeUTC );
    static DayTime calculateHa( float lstTotalHours );

 private:
    static const double calculateTheta(double deltaJ, double longitude, float timeUTC);
    static const int calculateDeltaJd(int year, int month, int day);
};
