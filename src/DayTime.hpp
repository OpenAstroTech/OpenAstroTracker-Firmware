#pragma once

#include "core/types/DayTime.hpp"

// A class to handle hours, minutes, seconds in a unified manner, allowing
// addition of hours, minutes, seconds, other times and conversion to string.

// Forward declarations
class String;

/// Thin overlay over core::DayTime that adds Arduino-dependent methods
/// (String-based parsing, formatting, logging).
class DayTime : public core::DayTime
{
  public:
    // Inherit constructors from core
    using core::DayTime::DayTime;

    // Convert to a standard string (like 14:45:06)
    virtual const char *ToString() const;

    static DayTime ParseFromMeade(String const &s);
};
