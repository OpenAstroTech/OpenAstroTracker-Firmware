#pragma once

namespace core
{

/// How far the hand controller's "sync" moves the mount's zero point, given where the mount
/// thinks it is pointing and where the user says it actually points.
struct SyncAdjustment {
    /// Added to the zero position's right ascension, wrapped into (-12h, +12h] so the shorter
    /// way round the sky is taken.
    float raHours = 0.0f;

    /// Added to the zero position's declination. Declination is stored as a signed offset from
    /// home, so this keeps the sign that leaves the mount on the side of the sky it started on.
    float decDegrees = 0.0f;
};

/// The zero-point correction for a sync.
///
/// `decDegreePosition` is the declination the stepper position maps to before the correction —
/// negative when the mount sits on the other side of the home position, which flips the sign of
/// the declination correction.
SyncAdjustment syncAdjustment(
    float currentRaHours, float targetRaHours, float currentDecDegrees, float targetDecDegrees, float decDegreePosition);

}  // namespace core
