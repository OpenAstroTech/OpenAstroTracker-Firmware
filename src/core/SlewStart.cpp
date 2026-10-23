#include "SlewStart.hpp"

namespace core
{

SlewStartPlan planSlewStart(SlewStartKind kind, const SlewStartFacts &facts)
{
    SlewStartPlan plan;

    plan.raMoves               = facts.raTargetSteps != facts.raCurrentSteps;
    plan.enterRaSlewMicrosteps = plan.raMoves;

    if (kind == SlewStartKind::Target)
    {
        // A go-to only stops the tracker when it is running: stopping it when it is not would
        // publish a spurious TrackingStopped event.
        plan.stopTracking = plan.raMoves && facts.tracking;
    }
    else
    {
        // Homing stops it unconditionally, which is what the original did; `stopSlewing` on the
        // tracking axis is harmless when it is already stopped.
        plan.stopTracking = plan.raMoves;
    }

    return plan;
}

long homeRaTargetSteps(long trackingPositionSteps, uint16_t raSlewMicrosteps, uint16_t raTrackingMicrosteps)
{
    // u-steps(trk) * u-steps(slew)/u-step(trk) = u-steps(slew). Integer division truncates, as it
    // always has. A zero tracking rate is a broken build configuration, not a runtime case.
    const long trackingOffset = trackingPositionSteps * raSlewMicrosteps / raTrackingMicrosteps;
    return -trackingOffset;
}

}  // namespace core
