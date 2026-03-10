#ifndef _ENDSWITCHES_HPP
#define _ENDSWITCHES_HPP

#include "Types.hpp"
#include "Mount.hpp"

#if (USE_RA_END_SWITCH == 1 || USE_DEC_END_SWITCH == 1)

class Mount;

enum EndSwitchState
{
    SWITCH_NOT_ACTIVE,
    SWITCH_AT_MINIMUM,
    SWITCH_AT_MAXIMUM,
    WAIT_FOR_STOP_AT_MAXIMUM,
    WAIT_FOR_STOP_AT_MINIMUM,
    SWITCH_SLEWING_OFF_MINIMUM,
    SWITCH_SLEWING_OFF_MAXIMUM,
};

/////////////////////////////////
//
// class EndSwitchState
//
/////////////////////////////////
class EndSwitch
{
  private:
    EndSwitchState _state;
    Mount *_pMount;
    StepperAxis _axis;
    long _posWhenTriggered;
    int _activeState;
    int _inactiveState;
    int _dir;
    int _minPin;
    int _maxPin;

  public:
    EndSwitch(Mount *mount, StepperAxis axis, int minPin, int maxPin, int activeState);
    void processEndSwitchState();
    void checkSwitchState();
};

#endif
#endif
