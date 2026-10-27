#pragma once

#include "DayTime.hpp"

namespace types
{

/// Pure Latitude coordinate. Range: -90° to +90° (clamped).
/// 90 = north pole, -90 = south pole.
class Latitude : public DayTime
{
  public:
    Latitude();
    Latitude(const Latitude &other);
    Latitude &operator=(const Latitude &) = default;
    Latitude(int h, int m, int s);
    Latitude(float inDegrees);

  protected:
    virtual void checkHours() override;
};

}  // namespace types
