#pragma once

#include "DayTime.hpp"

// 90 at north pole, -90 at south pole
class Latitude : public DayTime
{
  public:
    Latitude() : DayTime()
    {
    }
    Latitude(const Latitude &other);
    Latitude& operator=(const Latitude&) = default;
    Latitude(int h, int m, int s);
    Latitude(float inDegrees);

    static Latitude ParseFromMeade(String const &s);

  protected:
    virtual void checkHours() override;
};
