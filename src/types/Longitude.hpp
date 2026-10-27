#pragma once

#include "DayTime.hpp"

namespace types
{

/// Pure Longitude coordinate. Range: -180° to +180° (wrapped).
/// 0 = prime meridian, negative = west, positive = east.
class Longitude : public DayTime
{
  public:
    Longitude();
    Longitude(const Longitude &other);
    Longitude &operator=(const Longitude &) = default;
    Longitude(int h, int m, int s);
    Longitude(float inDegrees);

  protected:
    virtual void checkHours() override;
};

}  // namespace types
