#pragma once

#include "../Configuration.hpp"
#include "core/PortVocabulary.hpp"
#include "UiPorts.hpp"
#include "core/EndSwitchState.hpp"
#include "ports/IEndSwitch.hpp"

namespace app
{

/// Binds the end-switch state machine to one mount axis.
///
/// The state machine itself is [`core::pollLimit`](../core/EndSwitchState.hpp) and
/// `core::checkLimit`; this class samples the two switches, keeps the axis' own bookkeeping (the
/// position it latched at, the direction bits) and performs the actions the core asks for.
///
/// `processEndSwitchState()` runs inside the step generator, so its half stays a poll plus a
/// position read — no allocation, no motion calls.
class EndSwitchMonitor
{
  public:
    EndSwitchMonitor(UiPorts &ports, StepperAxis axis, ports::IEndSwitch &minSwitch, ports::IEndSwitch &maxSwitch);

    void processEndSwitchState();

    void checkSwitchState();

  private:
    void logEvent(core::LimitEvent event);

    UiPorts &_ui;
    StepperAxis _axis;
    int _dir;
    volatile long _posWhenTriggered;
    core::LimitState _state;
    ports::IEndSwitch &_minSwitch;
    ports::IEndSwitch &_maxSwitch;
};

}  // namespace app
