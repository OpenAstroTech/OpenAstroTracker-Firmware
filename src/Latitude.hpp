#pragma once

#include "core/types/Latitude.hpp"
#include "DayTime.hpp"

// Forward declarations
class String;

/// Latitude overlay — adds Arduino-dependent parsing over core::Latitude.
/// 90 at north pole, -90 at south pole.
class Latitude : public core::Latitude
{
  public:
    Latitude() : core::Latitude()
    {
    }
    Latitude(const Latitude &other);
    Latitude(int h, int m, int s);
    Latitude(float inDegrees);

    static Latitude ParseFromMeade(String const &s);
};
