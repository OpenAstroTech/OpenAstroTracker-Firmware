#pragma once

#include <stddef.h>

namespace types
{

/// Element count of a C array. Replaces the `ARRAY_SIZE` macro the deleted `Utility.hpp` carried.
template <typename T, size_t N> constexpr size_t arraySize(const T (&)[N])
{
    return N;
}

/// Small integer maths the firmware has always had, in one place so the UI and the settings format
/// can use it without pulling in `Utility.hpp` (which drags the board configuration with it).
inline int adjustWrap(int current, int adjustBy, int minimum, int maximum)
{
    current += adjustBy;
    if (current > maximum)
    {
        current -= (maximum + 1 - minimum);
    }
    if (current < minimum)
    {
        current += (maximum + 1 - minimum);
    }
    return current;
}

inline int adjustClamp(int current, int adjustBy, int minimum, int maximum)
{
    current += adjustBy;
    if (current > maximum)
    {
        current = maximum;
    }
    if (current < minimum)
    {
        current = minimum;
    }
    return current;
}

inline long clamp(long value, long minimum, long maximum)
{
    if (value < minimum)
    {
        return minimum;
    }
    if (value > maximum)
    {
        return maximum;
    }
    return value;
}

inline int clamp(int value, int minimum, int maximum)
{
    if (value < minimum)
    {
        return minimum;
    }
    if (value > maximum)
    {
        return maximum;
    }
    return value;
}

inline float clamp(float value, float minimum, float maximum)
{
    if (value < minimum)
    {
        return minimum;
    }
    if (value > maximum)
    {
        return maximum;
    }
    return value;
}

}  // namespace types
