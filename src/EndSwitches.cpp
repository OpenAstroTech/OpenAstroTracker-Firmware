#include "../Configuration.hpp"
#include "Utility.hpp"
#include "EPROMStore.hpp"
#include "EndSwitches.hpp"
#include "libs/MappedDict/MappedDict.hpp"

/////////////////////////////////
//
// getSwitchState
//
/////////////////////////////////
String EndSwitch::getSwitchState(EndSwitchState state) const
{
    MappedDict<EndSwitchState, String>::DictEntry_t lookupTable[] = {
        {SWITCH_RA_EAST_ACTIVE, F("EAST_ACTIVE")},
        {SWITCH_RA_WEST_ACTIVE, F("WEST_ACTIVE")},
        {SWITCH_DEC_UP_ACTIVE, F("UP_ACTIVE")},
        {SWITCH_DEC_DOWN_ACTIVE, F("DOWN_ACTIVE")},
        {SWITCH_NOT_ACTIVE, F("NOT_ACTIVE")},
    };

    auto strLookup = MappedDict<EndSwitchState, String>(lookupTable, ARRAY_SIZE(lookupTable));
    String rtnStr;
    if (strLookup.tryGet(state, &rtnStr))
    {
        return rtnStr;
    }
    return F("WTF_STATE");
}

EndSwitchState EndSwitch::getSwitchState() const
{
    return _endSwitchData.state;
}

/////////////////////////////////
//
// processHomingProgress
//
/////////////////////////////////
void EndSwitch::processEndSwitchState()
{
    if (_axis == StepperAxis::RA_STEPS)
    {
        if (digitalRead(_minPin) == LOW)
        {
            _endSwitchData.state = EndSwitchState::SWITCH_RA_EAST_ACTIVE;
            LOG(DEBUG_MOUNT, "[ENDSWITCH]: EndSwitch East signalled");
        }
        
        if (digitalRead(_maxPin) == LOW)
        {
            _endSwitchData.state = EndSwitchState::SWITCH_RA_WEST_ACTIVE;
            LOG(DEBUG_MOUNT, "[ENDSWITCH]: EndSwitch West signalled");
        }
    }

    if (_axis == StepperAxis::DEC_STEPS)
    {
        if (digitalRead(_minPin) == LOW)
        {
            _endSwitchData.state = EndSwitchState::SWITCH_DEC_UP_ACTIVE;
            LOG(DEBUG_MOUNT, "[ENDSWITCH]: EndSwitch Up signalled");
        }

        if (digitalRead(_maxPin) == LOW)
        {
            _endSwitchData.state = EndSwitchState::SWITCH_DEC_DOWN_ACTIVE;
            LOG(DEBUG_MOUNT, "[ENDSWITCH]: EndSwitch Down signalled");
        }
    }
}
