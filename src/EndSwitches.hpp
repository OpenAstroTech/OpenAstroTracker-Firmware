#ifndef _ENDSWITCHES_HPP
#define _ENDSWITCHES_HPP

#include "Types.hpp"
#include "app/UiPorts.hpp"
#include "ports/IEndSwitch.hpp"

/// Polls one axis' two end switches.
///
/// Defined on every board: whether one exists is a runtime question (`_raEndSwitch` and
/// `_decEndSwitch` are null when the board has none), so nothing that polls them needs a
/// compile-time branch. A board without end switches emits no code for this class.

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
    volatile EndSwitchState _state;
    app::UiPorts &_ui;
    StepperAxis _axis;
    volatile long _posWhenTriggered;
    int _dir;
    ports::IEndSwitch &_minSwitch;
    ports::IEndSwitch &_maxSwitch;

  public:
    EndSwitch(app::UiPorts &ports, StepperAxis axis, ports::IEndSwitch &minSwitch, ports::IEndSwitch &maxSwitch);
    void processEndSwitchState();
    void checkSwitchState();
};

#endif
