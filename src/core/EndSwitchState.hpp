#pragma once

#include <stdint.h>

namespace core
{

/// Where an axis' end-switch poll thinks it is.
///
/// The poll runs inside the step generator, so the whole machine is a plain enum plus two free
/// functions over PODs: no allocation, no logging, no virtual dispatch.
enum class LimitState : uint8_t
{
    NotActive,
    AtMinimum,
    AtMaximum,
    WaitForStopAtMaximum,
    WaitForStopAtMinimum,
    SlewingOffMinimum,
    SlewingOffMaximum,
};

/// What the two switches read on this tick.
struct LimitPoll {
    bool minimumActive;
    bool maximumActive;
};

/// What the poll noticed, so the caller can record the position and log it.
enum class LimitEvent : uint8_t
{
    None,
    MinimumReached,
    MaximumReached,
    LeftMinimum,
    LeftMaximum,
};

/// The step-generator half of the machine: advances `state` and reports the edge.
///
/// The switch check order matters and is preserved: both switches are sampled, and a board that
/// reports both active ends up "at maximum", which is what the original two `if`s did.
LimitEvent pollLimit(LimitState &state, const LimitPoll &poll);

/// What the main-loop half should do about the state the poll left behind.
enum class LimitAction : uint8_t
{
    None,
    /// A slew is running into the switch: stop it and back off.
    StopSlew,
    /// Tracking carried the axis into the maximum: stop tracking and leave it there.
    StopTracking,
    /// At the minimum while not slewing: only the state has to move on.
    ResetAfterMinimum,
};

LimitAction checkLimit(LimitState state, bool slewing, bool tracking);

/// How far to back off after tripping a switch, in microsteps.
///
/// The axis ran `posWhenTriggered - currentPosition` past the switch; the extra `backslewDegrees`
/// covers the switch's hysteresis.
long backOffDistance(long positionWhenTriggered, long currentPosition, long backSlewDegrees, float stepsPerDegree);

}  // namespace core
