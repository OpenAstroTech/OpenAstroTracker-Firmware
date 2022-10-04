#ifndef _ENDSWITCHES_HPP
#define _ENDSWITCHES_HPP

#include "Types.hpp"
#include "Mount.hpp"

class Mount;

enum EndSwitchState
{
    SWITCH_RA_EAST_ACTIVE,
    SWITCH_RA_WEST_ACTIVE,
    SWITCH_DEC_UP_ACTIVE,
    SWITCH_DEC_DOWN_ACTIVE,
    
    SWITCH_NOT_ACTIVE,
};

struct EndSwitchData {
    EndSwitchState state;
    EndSwitchState previousState;
    int pinState;
};

/////////////////////////////////
//
// class EndSwitchState
//
/////////////////////////////////
class EndSwitch
{
  private:
    EndSwitchData _endSwitchData;
    Mount *_pMount;
    StepperAxis _axis;
    long _stepsPerDegree;
    int _minPin;
    int _maxPin;

  public:
    EndSwitch(Mount *mount, StepperAxis axis, long stepsPerDegree, int minPin, int maxPin)
    {
        _endSwitchData.state = EndSwitchState::SWITCH_NOT_ACTIVE;
        _pMount         = mount;
        _axis           = axis;
        _stepsPerDegree   = stepsPerDegree;
        _minPin         = minPin;
        _maxPin         = maxPin;
    }

    void processEndSwitchState();
    String getSwitchState(EndSwitchState state) const;
    EndSwitchState getSwitchState() const;
};

#endif
