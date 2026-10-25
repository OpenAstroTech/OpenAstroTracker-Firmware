#pragma once

#include "../Configuration.hpp"

PUSH_NO_WARNINGS
#include <WString.h>
POP_NO_WARNINGS
#include "../Types.hpp"
#include "UiPorts.hpp"
#include "hal/WarningGuard.hpp"
#include "ports/IEndSwitch.hpp"

namespace app
{

/// Polls one axis' two end switches.
///
/// Defined on every board: whether one exists is a runtime question (the two switch references are
/// null when the board has none), so nothing that polls them needs a compile-time branch. This runs
/// inside the step generator, so the poll is a plain state machine over PODs: no allocation, no
/// logging, no new virtual dispatch.
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

class EndSwitch
{
  public:
    EndSwitch(UiPorts &ports, StepperAxis axis, ports::IEndSwitch &minSwitch, ports::IEndSwitch &maxSwitch);

    void processEndSwitchState();

    void checkSwitchState();

  private:
    volatile EndSwitchState _state;
    UiPorts &_ui;
    StepperAxis _axis;
    volatile long _posWhenTriggered;
    int _dir;
    ports::IEndSwitch &_minSwitch;
    ports::IEndSwitch &_maxSwitch;
};

}  // namespace app
