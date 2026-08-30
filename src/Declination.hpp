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

    // Split into celestial (hemisphere-corrected) degree components, as used
    // on the Meade wire: signed degrees, unsigned minutes/seconds.
    void getCelestialDegrees(int &deg, int &min, int &sec) const;

    // Build from celestial (Meade wire) components: signed degrees, unsigned
    // minutes/seconds.
    static Declination fromCelestialDegrees(int deg, int min, int sec);

    const char *ToDisplayString(char sep1, char sep2) const;

    static Declination ParseFromMeade(String const &s);
    static Declination FromSeconds(long seconds);
};
