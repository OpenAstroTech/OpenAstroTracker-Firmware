#include "EndSwitchMonitor.hpp"

#include "../Configuration.hpp"
#include "../Utility.hpp"

#if DEBUG_LEVEL != 0
namespace
{

/// Debug-only: the axis name as it appears in the log lines below.
const char *axisName(StepperAxis axis)
{
    return (axis == StepperAxis::RA_STEPS) ? "RA" : "DEC";
}

}  // namespace
#endif

namespace app
{

EndSwitchMonitor::EndSwitchMonitor(UiPorts &ports, StepperAxis axis, ports::IEndSwitch &minSwitch, ports::IEndSwitch &maxSwitch)
    : _ui(ports), _axis(axis), _dir((axis == StepperAxis::RA_STEPS) ? (EAST | WEST) : (NORTH | SOUTH)), _posWhenTriggered(0),
      _state(core::LimitState::NotActive), _minSwitch(minSwitch), _maxSwitch(maxSwitch)
{
}

/////////////////////////////////
//
// processEndSwitchState
//
// This function runs in the interrupt of the stepping code
// so it needs to be very fast. It samples the two switches and lets the
// core state machine say what that means.
//
/////////////////////////////////
void EndSwitchMonitor::processEndSwitchState()
{
    const core::LimitPoll poll {_minSwitch.isTriggered(), _maxSwitch.isTriggered()};
    const core::LimitEvent event = core::pollLimit(_state, poll);

    if ((event == core::LimitEvent::MinimumReached) || (event == core::LimitEvent::MaximumReached))
    {
        // The one side effect the ISR half owns: where the axis was when the switch fired, which
        // the main loop needs to back off by.
        _posWhenTriggered = _ui.read->getCurrentStepperPosition(_dir);
    }

    logEvent(event);
}

/////////////////////////////////
//
// checkSwitchState
//
// This runs in the main loop, next to the mount's tick, where stopping a slew and
// moving off the switch is allowed.
//
/////////////////////////////////
void EndSwitchMonitor::checkSwitchState()
{
    const core::LimitAction action = core::checkLimit(_state, _ui.motion->isSlewingRAorDEC(), _ui.motion->isSlewingTRK());

    switch (action)
    {
        case core::LimitAction::StopSlew:
            {
                int stopSlewDir = _dir;
                // Tracking is stopped as well when RA runs into its maximum.
                if ((_state == core::LimitState::AtMaximum) && (_axis == StepperAxis::RA_STEPS))
                {
                    stopSlewDir |= TRACKING;
                }

                // Set the state first: waitUntilStopped() calls back into this method.
                const bool atMinimum = (_state == core::LimitState::AtMinimum);
                _state               = atMinimum ? core::LimitState::WaitForStopAtMinimum : core::LimitState::WaitForStopAtMaximum;
                _ui.motion->stopSlewing(stopSlewDir);
                _ui.waitUntilStopped(_dir);

                const long currentPos      = _ui.read->getCurrentStepperPosition(_dir);
                const long backSlewDegrees = (_axis == StepperAxis::DEC_STEPS) ? DEC_ENDSWITCH_BACKSLEW_DEG : RA_ENDSWITCH_BACKSLEW_DEG;
                const long distance
                    = core::backOffDistance(_posWhenTriggered, currentPos, backSlewDegrees, _ui.config->getStepsPerDegree(_axis));

                LOG(DEBUG_MOUNT,
                    "[ENDSWITCH]: %s axis stopped at %l after tripping at %l, backing off %l. State is SWITCH_SLEWING_OFF",
                    axisName(_axis),
                    currentPos,
                    _posWhenTriggered,
                    distance);

                _ui.motion->moveStepperBy(_axis, distance);
                _state = atMinimum ? core::LimitState::SlewingOffMinimum : core::LimitState::SlewingOffMaximum;
            }
            break;

        case core::LimitAction::StopTracking:
            LOG(DEBUG_MOUNT,
                "[ENDSWITCH]: Reached maximum position on %s axis while tracking. Stopping tracking. State is now SWITCH_SLEWING_OFF",
                axisName(_axis));
            _ui.motion->stopSlewing(TRACKING);
            _state = core::LimitState::SlewingOffMaximum;
            break;

        case core::LimitAction::ResetAfterMinimum:
            LOG(DEBUG_MOUNT, "[ENDSWITCH]: Reached minimum position on %s axis while NOT tracking. Unexpected.", axisName(_axis));
            _state = core::LimitState::SlewingOffMinimum;
            break;

        case core::LimitAction::None:
        default:
            break;
    }
}

void EndSwitchMonitor::logEvent(core::LimitEvent event)
{
    switch (event)
    {
        case core::LimitEvent::MinimumReached:
            LOG(DEBUG_MOUNT,
                "[ENDSWITCH]: Reached minimum position on %s axis at %l. AxisRunning=%d, Slewing=%d. State is now SWITCH_AT_MINIMUM",
                axisName(_axis),
                _posWhenTriggered,
                (int) _ui.read->isAxisRunning(_axis),
                (int) _ui.motion->isSlewingRAorDEC());
            break;

        case core::LimitEvent::MaximumReached:
            LOG(DEBUG_MOUNT,
                "[ENDSWITCH]: Reached maximum position on %s axis at %l. AxisRunning=%d, Slewing=%d. State is now SWITCH_AT_MAXIMUM",
                axisName(_axis),
                _posWhenTriggered,
                (int) _ui.read->isAxisRunning(_axis),
                (int) _ui.motion->isSlewingRAorDEC());
            break;

        case core::LimitEvent::LeftMinimum:
            LOG(DEBUG_MOUNT, "[ENDSWITCH]: Finished moving off %s axis minimum! State is now SWITCH_INACTIVE", axisName(_axis));
            break;

        case core::LimitEvent::LeftMaximum:
            LOG(DEBUG_MOUNT, "[ENDSWITCH]: Finished moving off %s axis maximum! State is now SWITCH_INACTIVE", axisName(_axis));
            break;

        case core::LimitEvent::None:
        default:
            break;
    }
}

}  // namespace app
