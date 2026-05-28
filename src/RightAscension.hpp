#pragma once

#include "core/types/RightAscension.hpp"
#include "DayTime.hpp"

// Forward declarations
class String;

/// Right Ascension overlay — adds Arduino-dependent formatting and parsing
/// over core::RightAscension.
/// RA is 0–24h, wrapping at 24h, always non-negative.
class RightAscension : public core::RightAscension
{
  public:
    using core::RightAscension::RightAscension;

    // Convert to a standard string (like 12:34:56)
    virtual const char *ToString() const;

    static RightAscension ParseFromMeade(String const &s);
};
