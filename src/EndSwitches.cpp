#include "../Configuration.hpp"
#include "Utility.hpp"
#include "EPROMStore.hpp"
#include "EndSwitches.hpp"
#include "libs/MappedDict/MappedDict.hpp"

#if (USE_RA_END_SWITCH == 1 || USE_DEC_END_SWITCH == 1)

/////////////////////////////////
//
// EndSwitch ctor
//
/////////////////////////////////
EndSwitch::EndSwitch(Mount *mount, StepperAxis axis, int minPin, int maxPin, int activeState)
{
    _state            = EndSwitchState::SWITCH_NOT_ACTIVE;
    _pMount           = mount;
    _activeState      = activeState;
    _inactiveState    = activeState == HIGH ? LOW : HIGH;
    _axis             = axis;
    _dir              = (_axis == StepperAxis::RA_STEPS) ? (EAST | WEST) : (NORTH | SOUTH);
    _minPin           = minPin;
    _maxPin           = maxPin;
    _posWhenTriggered = 0;
    pinMode(_minPin, INPUT_PULLUP);
    pinMode(_maxPin, INPUT_PULLUP);
}

/////////////////////////////////
//
// processEndSwitchState
//
// This function runs in the interrupt of the stepping code
// so it needs to be very fast. We only set a flag and remember
// when we triggered the switch.
//
/////////////////////////////////
void EndSwitch::processEndSwitchState()
{
    switch (_state)
    {
        case EndSwitchState::SWITCH_NOT_ACTIVE:
            {
                if (digitalRead(_minPin) == _activeState)
                {
                    _state            = EndSwitchState::SWITCH_AT_MINIMUM;
                    _posWhenTriggered = _pMount->getCurrentStepperPosition(_dir);
                    LOG(DEBUG_MOUNT,
                        "[ENDSWITCH]: Reached minimum position on %s axis at %l. MountState is %x, %x. State is now SWITCH_AT_LIMIT",
                        _axis == StepperAxis::RA_STEPS ? "RA" : "DEC",
                        _posWhenTriggered,
                        _pMount->slewStatus(),
                        _pMount->mountStatus());
                }

                if (digitalRead(_maxPin) == _activeState)
                {
                    _state            = EndSwitchState::SWITCH_AT_MAXIMUM;
                    _posWhenTriggered = _pMount->getCurrentStepperPosition(_dir);
                    LOG(DEBUG_MOUNT,
                        "[ENDSWITCH]: Reached maximum position on %s axis at %l. MountState is %x, %x. State is now SWITCH_AT_LIMIT",
                        _axis == StepperAxis::RA_STEPS ? "RA" : "DEC",
                        _posWhenTriggered,
                        _pMount->slewStatus(),
                        _pMount->mountStatus());
                }
            }
            break;

        case EndSwitchState::WAIT_FOR_STOP_AT_MAXIMUM:
        case EndSwitchState::WAIT_FOR_STOP_AT_MINIMUM:
        case EndSwitchState::SWITCH_AT_MAXIMUM:
        case EndSwitchState::SWITCH_AT_MINIMUM:
            break;

        case EndSwitchState::SWITCH_SLEWING_OFF_MINIMUM:
            {
                if (digitalRead(_minPin) == _inactiveState)
                {
                    _state = EndSwitchState::SWITCH_NOT_ACTIVE;
                    LOG(DEBUG_MOUNT,
                        "[ENDSWITCH]: Finished moving off %s axis minimum! State is now SWITCH_INACTIVE",
                        _axis == StepperAxis::RA_STEPS ? "RA" : "DEC");
                }
            }
            break;
        case EndSwitchState::SWITCH_SLEWING_OFF_MAXIMUM:
            {
                if (digitalRead(_maxPin) == _inactiveState)
                {
                    _state = EndSwitchState::SWITCH_NOT_ACTIVE;
                    LOG(DEBUG_MOUNT,
                        "[ENDSWITCH]: Finished moving off %s axis maximum! State is now SWITCH_INACTIVE",
                        _axis == StepperAxis::RA_STEPS ? "RA" : "DEC");
                }
            }
            break;
    }
}

///////////////////////////
//
// checkSwitchState
// This runs in the Mount::loop() function where all the logic
// about stopping, parking, etc. happens.
//
///////////////////////////
void EndSwitch::checkSwitchState()
{
    if (!((_state == EndSwitchState::SWITCH_AT_MINIMUM) || (_state == EndSwitchState::SWITCH_AT_MAXIMUM)))
    {
        // Don't run any logic if we're not at one of the stops.
        return;
    }

    if (_pMount->isSlewingRAorDEC())
    {
        LOG(DEBUG_MOUNT,
            "[ENDSWITCH]: Switch Active while slewing. (%x, %x). State is SWITCH_AT_LIMIT",
            _pMount->slewStatus(),
            _pMount->mountStatus());
        int stopSlewDir = _dir;
        // Check if we should stop tracking, which we do if RA is at maximum.
        if ((_state == EndSwitchState::SWITCH_AT_MAXIMUM) && (_axis == StepperAxis::RA_STEPS))
        {
            stopSlewDir |= TRACKING;
        }

        // Set state first thing to avoid re-entrancy in call to waitUntilStopped() below.
        _state = _state == EndSwitchState::SWITCH_AT_MINIMUM ? EndSwitchState::WAIT_FOR_STOP_AT_MINIMUM
                                                             : EndSwitchState::WAIT_FOR_STOP_AT_MAXIMUM;
        LOG(DEBUG_MOUNT,
            "[ENDSWITCH]: Slewing is active, so stopping slew on %s axis (%x, %x). State is WAIT_FOR_STOP",
            _axis == StepperAxis::RA_STEPS ? "RA" : "DEC",
            stopSlewDir,
            _dir);
        _pMount->stopSlewing(stopSlewDir);
        _pMount->waitUntilStopped(_dir);  // This function will call checkSwitchState
        LOG(DEBUG_MOUNT,
            "[ENDSWITCH]: Slewing stopped on %s axis. (%x, %x)",
            _axis == StepperAxis::RA_STEPS ? "RA" : "DEC",
            _pMount->slewStatus(),
            _pMount->mountStatus());
        long currentPos      = _pMount->getCurrentStepperPosition(_dir);
        long backDistance    = _posWhenTriggered - currentPos;
        long backSign        = sign(backDistance);
        long backslewDegrees = (_axis == StepperAxis::DEC_STEPS) ? DEC_ENDSWITCH_BACKSLEW_DEG : RA_ENDSWITCH_BACKSLEW_DEG;
        // Go back the distance that we ran past the switch plus the number of degrees to overcome the hysteresis of the switch
        long backSlewDistance = backDistance + (backSign * backslewDegrees * _pMount->getStepsPerDegree(_axis));
        LOG(DEBUG_MOUNT,
            "[ENDSWITCH]: Reached maximum at %l. Stopped at %l (%l delta), moving back 1.2x (%l). Stopped tracking (if RA at Max). "
            "State is SWITCH_SLEWING_OFF",
            _posWhenTriggered,
            currentPos,
            backDistance,
            backSlewDistance);
        _pMount->moveStepperBy(_axis, backSlewDistance);
        _state = _state == EndSwitchState::WAIT_FOR_STOP_AT_MINIMUM ? EndSwitchState::SWITCH_SLEWING_OFF_MINIMUM
                                                                    : EndSwitchState::SWITCH_SLEWING_OFF_MAXIMUM;
    }
    else if ((_state == EndSwitchState::SWITCH_AT_MAXIMUM) && _pMount->isSlewingTRK())
    {
        // If we are not slewing, we must have tracked into the maximum, so just stop tracking and wait for it to be moved off maximum manually.
        LOG(DEBUG_MOUNT,
            "[ENDSWITCH]: Reached maximum position on %s axis while tracking. Stopping tracking. State is now SWITCH_SLEWING_OFF",
            _axis == StepperAxis::RA_STEPS ? "RA" : "DEC");
        _pMount->stopSlewing(TRACKING);
        _state = EndSwitchState::SWITCH_SLEWING_OFF_MAXIMUM;
    }
    else if (_state == EndSwitchState::SWITCH_AT_MINIMUM)
    {
        // This shouldn't really happen, since we can't really reach minimum if we're not slewing, since tracking goes to maximum.
        // But just so that the state machine resets correctly once we move off this switch we transition to the state that waits for that.
        _state = EndSwitchState::SWITCH_SLEWING_OFF_MINIMUM;
        LOG(DEBUG_MOUNT,
            "[ENDSWITCH]: Reached minimum position on %s axis while NOT tracking. Unexpected.",
            _axis == StepperAxis::RA_STEPS ? "RA" : "DEC");
    }
}

#endif
