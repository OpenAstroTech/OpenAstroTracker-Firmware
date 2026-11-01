#pragma once

#include <stdint.h>

namespace core
{
/// The build-time facts about *where* the mount is and how it reads the sky.
///
/// Filled once by `app::buildSiteConfig()` from the board's `Configuration*.hpp`; the rest of the
/// firmware then reads fields instead of re-expanding the macros. Polaris is stored as three
/// plain integers rather than a `types::DayTime`, because `DayTime` is polymorphic and therefore
/// neither literal nor foldable — a `SiteConfig` built from literals stays a compile-time value.
///
/// Defaults match the firmware's long-standing assumptions (northern hemisphere, the 3h 0m 8s
/// Polaris catalogue position) so a host test can construct one without naming any board macro.
struct SiteConfig {
    float siderealSecondsPerDay = 86164.0905f;
    float raZeroPointShiftHours = 0.0f;
    bool northernHemisphere     = true;
    uint8_t polarisRaHour       = 3;
    uint8_t polarisRaMinute     = 0;
    uint8_t polarisRaSecond     = 8;
};

}  // namespace core
