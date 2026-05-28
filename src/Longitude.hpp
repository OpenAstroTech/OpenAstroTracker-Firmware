#pragma once

#include "core/types/Longitude.hpp"
#include "DayTime.hpp"

// Forward declarations
class String;

/// Longitude overlay — adds Arduino-dependent formatting and parsing
/// over core::Longitude.
/// -180..180 range, 0 is at the prime meridian (through Greenwich),
/// negative going west, positive going east.
class Longitude : public core::Longitude
{
  public:
    Longitude() : core::Longitude()
    {
    }
    Longitude(const Longitude &other);
    Longitude(int h, int m, int s);
    Longitude(float inDegrees);

    virtual const char *formatString(char *targetBuffer, const char *format, long *pSeconds = nullptr) const;
    const char *formatStringForMeade(char *targetBuffer) const;
    virtual const char *ToString() const;

    static Longitude ParseFromMeade(String const &s);
};
