#pragma once

#include "core/types/DayTime.hpp"

// A class to handle hours, minutes, seconds in a unified manner, allowing
// addition of hours, minutes, seconds, other times and conversion to string.

#include <Arduino.h>  // `String`, which these overlays parse and format

/// Thin overlay over core::DayTime that adds Arduino-dependent methods
/// (String-based parsing, formatting, logging).
class DayTime : public core::DayTime
{
  public:
    // Inherit constructors from core
    using core::DayTime::DayTime;

    /// Copies the pure value. Used when a `ports` accessor hands back a
    /// `core::DayTime` and the caller holds the Arduino-aware overlay.
    DayTime(const core::DayTime &other) : core::DayTime(other)
    {
    }

    // Convert to a standard string (like 14:45:06)
    virtual const char *ToString() const;

    static DayTime ParseFromMeade(String const &s);
};
