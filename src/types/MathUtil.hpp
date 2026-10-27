#pragma once

#include <stddef.h>

namespace types
{

/// Element count of a C array, resolved at compile time.
///
/// Binds only to a real array: a pointer does not match the reference-to-array parameter, so the
/// call fails to compile on a decayed pointer rather than silently returning garbage. This replaces
/// the `ARRAY_SIZE` macro that the deleted `Utility.hpp` carried.
///
/// @tparam T Element type, deduced from the argument.
/// @tparam N Element count, deduced from the argument.
/// @return The number of elements in the array.
template <typename T, size_t N> constexpr size_t arraySize(const T (&)[N])
{
    return N;
}

// Small integer maths the firmware has always had, in one place so the UI and the settings format
// can use it without pulling in `Utility.hpp` (which drags the board configuration with it).

/// Adds @p adjustBy to @p current and wraps the result into the inclusive range
/// [@p minimum, @p maximum].
///
/// The wrap is a single step: the range width (`maximum + 1 - minimum`, i.e. the number of values
/// in the range) is subtracted or added exactly once when the sum leaves the range. A caller that
/// may pass a delta larger than the range width must reduce it first; this does not normalise an
/// arbitrary delta. Used for menu navigation and time/coordinate stepping, where @p adjustBy is one
/// keypress or one increment.
///
/// @param current  The value to adjust; expected to already be in range.
/// @param adjustBy The signed amount to add (negative steps backwards).
/// @param minimum  Lowest value of the range.
/// @param maximum  Highest value of the range; must be greater than @p minimum.
/// @return @p current + @p adjustBy, wrapped by one period into [@p minimum, @p maximum].
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

/// Adds @p adjustBy to @p current and saturates the result to the inclusive range
/// [@p minimum, @p maximum].
///
/// Unlike `adjustWrap()`, nothing wraps: the result stops at the bound it would have crossed, so a
/// large delta is safe and simply lands on the nearer end of the range.
///
/// @param current  The value to adjust.
/// @param adjustBy The signed amount to add (negative steps backwards).
/// @param minimum  Lower bound of the range.
/// @param maximum  Upper bound of the range; must be greater than or equal to @p minimum.
/// @return @p current + @p adjustBy, clamped into [@p minimum, @p maximum].
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

/// Limits @p value to the inclusive range [@p minimum, @p maximum].
///
/// @param value   The value to constrain.
/// @param minimum Lower bound (inclusive); must not exceed @p maximum.
/// @param maximum Upper bound (inclusive).
/// @return @p value when it is already in range, otherwise the nearer of the two bounds.
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

/// Integer overload of `clamp()`.
///
/// @param value   The value to constrain.
/// @param minimum Lower bound (inclusive); must not exceed @p maximum.
/// @param maximum Upper bound (inclusive).
/// @return @p value when it is already in range, otherwise the nearer of the two bounds.
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

/// Floating-point overload of `clamp()`.
///
/// @param value   The value to constrain.
/// @param minimum Lower bound (inclusive); must not exceed @p maximum.
/// @param maximum Upper bound (inclusive).
/// @return @p value when it is already in range, otherwise the nearer of the two bounds.
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
