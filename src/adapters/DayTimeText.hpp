#pragma once

#include "types/DayTime.hpp"
#include "types/Declination.hpp"
#include "types/Latitude.hpp"
#include "types/Longitude.hpp"

namespace adapters
{

/// The coordinate renderings the old `adapters::DayTime`/`Declination`/`Latitude`/`Longitude`
/// subclasses added on top of the `types::` values, as free functions.
///
/// They exist for one caller shape: a debug log that wants `%s`. Nothing here feeds the Meade wire
/// format — that is `core::CoordinateFormatter` — so the strings are for humans only and their exact
/// spelling is not a contract.
///
/// Each function formats into its own static buffer and returns it, which is what makes
/// `OAT_LOG("RA %s, DEC %s", formatDayTime(ra), formatDeclination(dec, north))` work. One buffer per
/// function, never block-scoped, so two calls to the *same* function in one expression would clash.
/// That hazard is identical to the four static buffers these replace.

/// "14:45:06 (14.75000)" — the hour angle with its decimal-hours value in brackets.
const char *formatDayTime(const types::DayTime &t);

/// "+45:00:00" — latitude, sign always present.
const char *formatLatitude(const types::Latitude &l);

/// "-123:45:00 (-123.75E)" — longitude, with the 0..360 east-count and a hemisphere letter.
const char *formatLongitude(const types::Longitude &l);

/// "DEC as signed celestial arc-seconds on the mount axis (-90..+90)".
const char *formatDeclination(const types::Declination &d, bool northernHemisphere);

/// Axis declination from signed celestial arc-seconds, the mapping
/// `calculateStepperPositions()` takes. (Was `Declination::FromSeconds`.)
types::Declination decFromCelestialArcSeconds(long seconds, bool northernHemisphere);

/// Axis declination from signed celestial arc-seconds on the home-side branch.
/// (Was `Declination::fromCelestialSeconds`.) The sign lives in the total, so a coordinate
/// between 0 and -1 degree survives the trip.
types::Declination decFromCelestialSeconds(long celestialSeconds, bool northernHemisphere);

}  // namespace adapters
