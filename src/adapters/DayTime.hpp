#pragma once

#include "types/DayTime.hpp"

// A class to handle hours, minutes, seconds in a unified manner, allowing
// addition of hours, minutes, seconds, other times and conversion to string.

#include <WString.h>  // `String`, which these overlays parse and format

/// Thin overlay over types::DayTime that adds Arduino-dependent methods
/// (String-based parsing, formatting, logging).
class DayTime : public types::DayTime
{
  public:
    // Inherit constructors from core
    using types::DayTime::DayTime;

    /// Copies the pure value. Used when a `ports` accessor hands back a
    /// `types::DayTime` and the caller holds the Arduino-aware overlay.
    DayTime(const types::DayTime &other) : types::DayTime(other)
    {
    }

    // Convert to a standard string (like 14:45:06)
    virtual const char *ToString() const;

    static DayTime ParseFromMeade(String const &s);
};
