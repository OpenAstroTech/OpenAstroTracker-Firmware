#pragma once

#include "../Configuration.hpp"

PUSH_NO_WARNINGS
#include <WString.h>
POP_NO_WARNINGS
#include "../Types.hpp"
#include "UiPorts.hpp"
#include "core/HomingController.hpp"
#include "ports/IHomingSensor.hpp"
#include "ports/IStatusFlags.hpp"

namespace app
{

/// Binds the hall-sensor homing state machine to one mount axis.
///
/// The decisions live in `core::HomingController`; this class reads the sensor and the axis each
/// tick, hands them over, and performs the single action that comes back. It also owns the two
/// pieces of mount policy the state machine does not: the slew rate to restore and whether
/// tracking was on when the mount was set up.
class HallSensorHoming
{
  public:
    HallSensorHoming(UiPorts &ports, ports::IStatusFlags &status, StepperAxis axis, ports::IHomingSensor &sensor);

    bool findHomeByHallSensor(int initialDirection, int searchDistanceDegrees);

    void processHomingProgress();

    String getHomingState(core::HomingState state) const;

    core::HomingState getHomingState() const;

    core::HomingResult homingResult() const;

    bool isIdleOrComplete() const;

    String getLastResult() const;

  private:
    bool sensorIsActive() const;

    void logTransition(core::HomingState previous, const core::HomingStep &step);

    core::HomingController _controller;
    UiPorts &_ui;
    ports::IStatusFlags &_status;
    StepperAxis _axis;
    ports::IHomingSensor &_sensor;
    uint8_t _savedRate;
    bool _wasTracking;
};

}  // namespace app
