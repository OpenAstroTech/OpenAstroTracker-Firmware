#pragma once

#include "core/types/Declination.hpp"
#include "adapters/DayTime.hpp"

#include <WString.h>  // `String`, which these overlays parse and format

/// Declination overlay — adds Arduino-dependent formatting and parsing
/// over core::Declination.
class Declination : public core::Declination
{
  public:
    using core::Declination::Declination;

    /// Copies the pure value. Used where a port hands back a `core::Declination` and the
    /// caller holds the Arduino-aware overlay — the same convenience `DayTime` and
    /// `Latitude` already carry.
    Declination(const core::Declination &other) : core::Declination(other)
    {
    }

    // Convert to a standard string (like +54:45:06)
    virtual const char *ToString() const;
    virtual const char *formatString(char *targetBuffer, const char *format, long *pSeconds = nullptr) const;

    // Split into celestial (hemisphere-corrected) degree components, as used
    // on the Meade wire: signed degrees, unsigned minutes/seconds.
    void getCelestialDegrees(int &deg, int &min, int &sec) const;

    // Build from celestial (Meade wire) components: signed degrees, unsigned
    // minutes/seconds.
    static Declination fromCelestialDegrees(int deg, int min, int sec);

    // Build from signed celestial arc-seconds. Preferred over
    // fromCelestialDegrees at the wire boundary: a signed degrees component
    // cannot express a coordinate between 0 and -1 degree. Pair it with
    // core::Declination::celestialSecondsFrom to do the join.
    static Declination fromCelestialSeconds(long celestialSeconds);

    const char *ToDisplayString(char sep1, char sep2) const;

    static Declination ParseFromMeade(String const &s);
    static Declination FromSeconds(long seconds);
};
