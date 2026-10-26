#pragma once

#include "core/types/Declination.hpp"
#include "adapters/DayTime.hpp"

#include <WString.h>  // `String`, which these overlays format

/// Declination overlay — Arduino-dependent formatting over `core::Declination`.
///
/// The axis↔celestial conversion depends on the hemisphere the site is in, so every method that
/// crosses that boundary takes it as an argument: the caller has it (the read model, the command
/// adapter) and this class deliberately holds no copy of a runtime fact.
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
    const char *ToString(bool northernHemisphere) const;

    /// Build from signed celestial (Meade wire) seconds. The sign lives in the total, so a
    /// coordinate between 0 and -1 degree survives the trip.
    static Declination fromCelestialSeconds(long celestialSeconds, bool northernHemisphere);

    /// Build from signed celestial arc-seconds in the units `calculateStepperPositions()` takes.
    static Declination FromSeconds(long seconds, bool northernHemisphere);
};
