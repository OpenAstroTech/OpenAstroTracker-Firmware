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
// getSwitchState
//
/////////////////////////////////
EndSwitchState EndSwitch::getSwitchState() const
{
    return _state;
}

/////////////////////////////////
//
// setSwitchState
//
/////////////////////////////////
void EndSwitch::setSwitchState(EndSwitchState state)
{
    _state = state;
}

/////////////////////////////////
//
// getPosWhenTriggered
//
/////////////////////////////////
long EndSwitch::getPosWhenTriggered() const
{
    return _posWhenTriggered;
}

/////////////////////////////////
//
// processHomingProgress
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
                        "[ENDSWITCH]: Reached minimum position on %s axis at %l",
                        _axis == StepperAxis::RA_STEPS ? "RA" : "DEC",
                        _posWhenTriggered);
                }

                if (digitalRead(_maxPin) == _activeState)
                {
                    _state            = EndSwitchState::SWITCH_AT_MAXIMUM;
                    _posWhenTriggered = _pMount->getCurrentStepperPosition(_dir);
                    LOG(DEBUG_MOUNT,
                        "[ENDSWITCH]: Reached maximum position on %s axis at %l",
                        _axis == StepperAxis::RA_STEPS ? "RA" : "DEC",
                        _posWhenTriggered);
                }
            }
            break;

        case EndSwitchState::SWITCH_AT_MAXIMUM:
        case EndSwitchState::SWITCH_AT_MINIMUM:
            break;

        case EndSwitchState::SWITCH_SLEWING_OFF_MINIMUM:
            {
                if (digitalRead(_minPin) == _inactiveState)
                {
                    _state = EndSwitchState::SWITCH_NOT_ACTIVE;
                    LOG(DEBUG_MOUNT, "[ENDSWITCH]: Finished moving off %s axis minimum!", _axis == StepperAxis::RA_STEPS ? "RA" : "DEC");
                }
            }
            break;
        case EndSwitchState::SWITCH_SLEWING_OFF_MAXIMUM:
            {
                if (digitalRead(_maxPin) == _inactiveState)
                {
                    _state = EndSwitchState::SWITCH_NOT_ACTIVE;
                    LOG(DEBUG_MOUNT, "[ENDSWITCH]: Finished moving off %s axis maximum!", _axis == StepperAxis::RA_STEPS ? "RA" : "DEC");
                }
            }
            break;
    }
}

///////////////////////////
//
// checkSwitchState
//
///////////////////////////
void EndSwitch::checkSwitchState()
{
    if ((_state == EndSwitchState::SWITCH_AT_MINIMUM) || (_state == EndSwitchState::SWITCH_AT_MAXIMUM))
    {
        if (_pMount->isSlewingRAorDEC())
        {
            int stopSlewDir = _dir;
            // Check if we should stop tracking, which we do if RA is at maximum.
            if ((_state == EndSwitchState::SWITCH_AT_MAXIMUM) && (_axis == StepperAxis::RA_STEPS))
            {
                stopSlewDir |= TRACKING;
            }

            // Set state first thing to avoid re-entrancy in call to waitUntilStopped() below.
            _state = _state == EndSwitchState::SWITCH_AT_MINIMUM ? EndSwitchState::SWITCH_SLEWING_OFF_MINIMUM
                                                                 : EndSwitchState::SWITCH_SLEWING_OFF_MAXIMUM;
            LOG(DEBUG_MOUNT,
                "[ENDSWITCH]: Switch activated, stopping and reversing %s axis",
                _axis == StepperAxis::RA_STEPS ? "RA" : "DEC");
            _pMount->stopSlewing(stopSlewDir);
            _pMount->waitUntilStopped(_dir);
            long currentPos       = _pMount->getCurrentStepperPosition(_dir);
            long backDistance     = _posWhenTriggered - currentPos;
            long backSlewDistance = (12 * backDistance) / 10;  // Go back 120% distance that we ran past the switch
            LOG(DEBUG_MOUNT,
                "[ENDSWITCH]: Reached maximum at %l. Stopped at %l (%l delta), moving back 1.2x (%l). Stopped tracking (if RA at Max).",
                _posWhenTriggered,
                currentPos,
                backDistance,
                backSlewDistance);
            _pMount->moveStepperBy(_axis, backSlewDistance);
        }
    }
}

#endif
