#include "core/EndSwitchState.hpp"

namespace core
{

LimitEvent pollLimit(LimitState &state, const LimitPoll &poll)
{
    switch (state)
    {
        case LimitState::NotActive:
            // Maximum is tested first on purpose: the original sampled both switches and let the
            // maximum win when a board reports both, so a stuck minimum cannot mask a hard stop.
            if (poll.maximumActive)
            {
                state = LimitState::AtMaximum;
                return LimitEvent::MaximumReached;
            }
            if (poll.minimumActive)
            {
                state = LimitState::AtMinimum;
                return LimitEvent::MinimumReached;
            }
            return LimitEvent::None;

        case LimitState::SlewingOffMinimum:
            if (!poll.minimumActive)
            {
                state = LimitState::NotActive;
                return LimitEvent::LeftMinimum;
            }
            return LimitEvent::None;

        case LimitState::SlewingOffMaximum:
            if (!poll.maximumActive)
            {
                state = LimitState::NotActive;
                return LimitEvent::LeftMaximum;
            }
            return LimitEvent::None;

        case LimitState::AtMinimum:
        case LimitState::AtMaximum:
        case LimitState::WaitForStopAtMaximum:
        case LimitState::WaitForStopAtMinimum:
        default:
            return LimitEvent::None;
    }
}

LimitAction checkLimit(LimitState state, bool slewing, bool tracking)
{
    if ((state != LimitState::AtMinimum) && (state != LimitState::AtMaximum))
    {
        return LimitAction::None;
    }

    if (slewing)
    {
        return LimitAction::StopSlew;
    }
    if ((state == LimitState::AtMaximum) && tracking)
    {
        return LimitAction::StopTracking;
    }
    if (state == LimitState::AtMinimum)
    {
        return LimitAction::ResetAfterMinimum;
    }
    return LimitAction::None;
}

long backOffDistance(long positionWhenTriggered, long currentPosition, long backSlewDegrees, float stepsPerDegree)
{
    const long backDistance = positionWhenTriggered - currentPosition;
    const long backSign     = (backDistance < 0) ? -1 : 1;
    return backDistance + static_cast<long>(backSign * backSlewDegrees * stepsPerDegree);
}

}  // namespace core
