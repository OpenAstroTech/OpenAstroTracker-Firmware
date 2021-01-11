#pragma once

#include "DayTime.hpp"

// Forward declaration
class TinyGPSPlus;

class Sidereal
{
 public:
    static DayTime calculateByGPS(TinyGPSPlus* gps);

 private:
    static const double calculateTheta(double deltaJ, double longitude, float timeUTC);
    static const int calculateDeltaJd(int year, int month, int day);
};
