#pragma once

#include "types/Latitude.hpp"
#include "adapters/DayTime.hpp"

#include <WString.h>  // `String`, which these overlays parse and format

/// Latitude overlay — adds Arduino-dependent parsing over types::Latitude.
/// 90 at north pole, -90 at south pole.
class Latitude : public types::Latitude
{
  public:
    Latitude() : types::Latitude()
    {
    }
    Latitude(const Latitude &other);
    Latitude &operator=(const Latitude &) = default;
    Latitude(int h, int m, int s);
    Latitude(float inDegrees);
    Latitude(const types::Latitude &other) : types::Latitude(other)
    {
    }

    // Convert to a standard string (like +45:00:00)
    virtual const char *ToString() const;

    static Latitude ParseFromMeade(String const &s);
};
