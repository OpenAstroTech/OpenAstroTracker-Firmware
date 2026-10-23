#pragma once

#include <stdint.h>

namespace core
{

/// The speed and acceleration each axis was programmed with, so the mount can restore them when
/// the slew rate changes or a slew ends. These are hardware facts the composition root knows when
/// it builds the engines, not preferences the domain decides.
struct AxisLimits
{
    unsigned long raSpeed          = 0;
    unsigned long raAcceleration   = 0;
    unsigned long decSpeed         = 0;
    unsigned long decAcceleration  = 0;
    int azSpeed                    = 0;
    int azAcceleration             = 0;
    int altSpeed                   = 0;
    int altAcceleration            = 0;
    int focusSpeed                 = 0;
    int focusAcceleration          = 0;
};

}  // namespace core
