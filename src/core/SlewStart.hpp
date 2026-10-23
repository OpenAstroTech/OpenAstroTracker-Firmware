#pragma once

#include <stdint.h>

namespace core
{

/// Which of the two "slew from where we are to a computed position" entry points is running.
///
/// They differ in one place only — whether tracking is stopped unconditionally or only when it
/// is actually running — and that difference is deliberate, so it is modelled rather than
/// smoothed away.
enum class SlewStartKind : uint8_t
{
    Target,  ///< a go-to: the target coordinates were just calculated
    Home,    ///< a move to the home position, offset by the tracking done so far
};

/// What the mount looked like when the request arrived.
struct SlewStartFacts {
    long raCurrentSteps = 0;
    long raTargetSteps  = 0;
    bool tracking       = false;
};

/// What starting the slew has to do before the axes are programmed.
struct SlewStartPlan {
    /// The RA axis has somewhere to go. A go-to that lands on the current position must not
    /// disturb tracking, and the tick only notices a completed slew if one was started.
    bool raMoves = false;

    /// Stop the tracking axis first.
    bool stopTracking = false;

    /// Switch RA to the slew microstep rate. Always accompanies `raMoves`; kept separate because
    /// it is a driver-programming step, not a motion one.
    bool enterRaSlewMicrosteps = false;
};

SlewStartPlan planSlewStart(SlewStartKind kind, const SlewStartFacts &facts);

/// The RA position that returns the axis to home, taking into account how far the tracking axis
/// has run.
///
/// The tracking axis counts in tracking microsteps while the slew axis counts in slew
/// microsteps, so the offset is converted before it is subtracted. The conversion is the integer
/// expression the firmware has always used, including its truncation.
long homeRaTargetSteps(long trackingPositionSteps, uint16_t raSlewMicrosteps, uint16_t raTrackingMicrosteps);

}  // namespace core
