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
EndSwitch::EndSwitch(Mount *mount, StepperAxis axis, int minPin, int maxPin)
{
    _state            = EndSwitchState::SWITCH_NOT_ACTIVE;
    _pMount           = mount;
    _axis             = axis;
    _dir              = (_axis == StepperAxis::RA_STEPS) ? (EAST | WEST) : (NORTH | SOUTH);
    _minPin           = minPin;
    _maxPin           = maxPin;
    _posWhenTriggered = 0;
    pinMode(_minPin, INPUT);
    pinMode(_maxPin, INPUT);
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
                if (digitalRead(_minPin) == LOW)
                {
                    _state            = EndSwitchState::SWITCH_AT_MINIMUM;
                    _posWhenTriggered = _pMount->getCurrentStepperPosition(_dir);
                    LOG(DEBUG_MOUNT, "[ENDSWITCH]: Reached minimum position on %s axis!", _axis == StepperAxis::RA_STEPS ? "RA" : "DEC");
                }

                if (digitalRead(_maxPin) == LOW)
                {
                    _state            = EndSwitchState::SWITCH_AT_MAXIMUM;
                    _posWhenTriggered = _pMount->getCurrentStepperPosition(_dir);
                    LOG(DEBUG_MOUNT, "[ENDSWITCH]: Reached maximum position on %s axis!", _axis == StepperAxis::RA_STEPS ? "RA" : "DEC");
                }
            }
            break;

        case EndSwitchState::SWITCH_AT_MAXIMUM:
        case EndSwitchState::SWITCH_AT_MINIMUM:
            break;

        case EndSwitchState::SWITCH_SLEWING_OFF_MINIMUM:
            {
                if (digitalRead(_minPin) == HIGH)
                {
                    _state = EndSwitchState::SWITCH_NOT_ACTIVE;
                    LOG(DEBUG_MOUNT, "[ENDSWITCH]: Finished moving off %s axis minimum!", _axis == StepperAxis::RA_STEPS ? "RA" : "DEC");
                }
            }
            break;
        case EndSwitchState::SWITCH_SLEWING_OFF_MAXIMUM:
            {
                if (digitalRead(_maxPin) == HIGH)
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
            _pMount->stopSlewing(_dir);
            _pMount->waitUntilStopped(_dir);
            if ((_state == EndSwitchState::SWITCH_AT_MAXIMUM) && (_axis == StepperAxis::RA_STEPS))
            {
                _pMount->stopSlewing(TRACKING);
            }
            long currentPos       = _pMount->getCurrentStepperPosition(_dir);
            long backDistance     = _posWhenTriggered - currentPos;
            long backSlewDistance = (12 * backDistance) / 10;  // Go back 120% distance that we ran past the switch
            LOG(DEBUG_MOUNT,
                "[ENDSWITCH]: Loop: Reached maximum at %l. Stopped at %l (%l delta), moving back %l. Stopped tracking.",
                _posWhenTriggered,
                currentPos,
                backDistance,
                backSlewDistance);
            _pMount->moveStepperBy(_axis, backSlewDistance);
            _state = _state == EndSwitchState::SWITCH_AT_MINIMUM ? EndSwitchState::SWITCH_SLEWING_OFF_MINIMUM
                                                                 : EndSwitchState::SWITCH_SLEWING_OFF_MAXIMUM;
        }
    }
}

#endif
