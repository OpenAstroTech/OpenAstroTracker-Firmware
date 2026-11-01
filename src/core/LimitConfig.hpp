#pragma once

namespace core
{
/// The travel limits the mount enforces, as the board's configuration states them.
///
/// RA limits are in hours from home; DEC limits are in degrees. A DEC limit of 0 means "no
/// limit", which is how the firmware has always encoded it, so the field keeps the raw value
/// rather than translating it.
///
/// Defaults are the `Configuration_adv.hpp` defaults; a board builder overwrites them.
struct LimitConfig {
    float raLimitLeftHours       = 5.0f;
    float raLimitRightHours      = 7.0f;
    float raTrackingLimitHours   = 7.0f;
    float raPhysicalLimitDegrees = 7.0f;
    float decLimitUpDegrees      = 0.0f;
    float decLimitDownDegrees    = 0.0f;
};

}  // namespace core
