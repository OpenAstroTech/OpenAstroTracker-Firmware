#pragma once

#include "types/Longitude.hpp"
#include "adapters/DayTime.hpp"

#include <WString.h>  // `String`, which these overlays parse and format

/// Longitude overlay — adds Arduino-dependent formatting and parsing
/// over types::Longitude.
/// -180..180 range, 0 is at the prime meridian (through Greenwich),
/// negative going west, positive going east.
class Longitude : public types::Longitude
{
  public:
    Longitude() : types::Longitude()
    {
    }
    Longitude(const Longitude &other);
    Longitude &operator=(const Longitude &) = default;
    Longitude(int h, int m, int s);
    Longitude(float inDegrees);
    Longitude(const types::Longitude &other) : types::Longitude(other)
    {
    }

    virtual const char *formatString(char *targetBuffer, const char *format, long *pSeconds = nullptr) const;
    const char *formatStringForMeade(char *targetBuffer) const;
    virtual const char *ToString() const;

    static Longitude ParseFromMeade(String const &s);
};
