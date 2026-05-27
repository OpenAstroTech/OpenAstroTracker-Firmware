#pragma once

#include "DayTime.hpp"

namespace core
{

/// Pure Right Ascension coordinate. Range: 0h to 24h (wrapped).
/// Stored as total seconds, wrapping at 24h (86400 seconds).
class RightAscension : public DayTime
{
  public:
    RightAscension();
    RightAscension(const RightAscension &other);
    RightAscension(int h, int m, int s);
    RightAscension(float timeInHours);

    virtual void set(int h, int m, int s) override;

    // Add hours, wrapping at 24h and keeping non-negative
    void addHours(float deltaHours) override;

    // Get RA in hours (0..24)
    float getTotalHours() const;

  protected:
    virtual void checkHours() override;
};

}  // namespace core
