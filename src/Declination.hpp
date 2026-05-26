#pragma once

#include "core/types/Declination.hpp"
#include "DayTime.hpp"

// Forward declarations
class String;

/// Declination overlay — adds Arduino-dependent formatting and parsing
/// over core::Declination.
class Declination : public core::Declination
{
  public:
    using core::Declination::Declination;

    // Convert to a standard string (like +54:45:06)
    virtual const char *ToString() const;
    virtual const char *formatString(char *targetBuffer, const char *format, long *pSeconds = nullptr) const;

    const char *ToDisplayString(char sep1, char sep2) const;

    static Declination ParseFromMeade(String const &s);
    static Declination FromSeconds(long seconds);
};
