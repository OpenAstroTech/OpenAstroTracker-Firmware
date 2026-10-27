#pragma once

#include "../Configuration.hpp"

PUSH_NO_WARNINGS
#include <WString.h>
POP_NO_WARNINGS
#include "ports/PortVocabulary.hpp"
#include "UiPorts.hpp"
#include "core/HomingController.hpp"
#include "core/MountState.hpp"
#include "ports/IHomingSensor.hpp"

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
    HallSensorHoming(UiPorts &ports, core::MountState &state, StepperAxis axis, ports::IHomingSensor &sensor);

    bool findHomeByHallSensor(int initialDirection, int searchDistanceDegrees);

    void processHomingProgress();

    String getHomingState(core::HomingState state) const;

    core::HomingState getHomingState() const;

    core::HomingResult homingResult() const;

    bool isIdleOrComplete() const;

    String getLastResult() const;

  private:
    bool sensorIsActive() const;

    /// Status writes take the interrupt guard the removed status-flags adapter used: the homing
    /// state machine can run from the step-pump context, and `MountState`'s read-modify-write is
    /// not atomic on AVR.
    void setStatus(uint16_t bits);

    void clearStatus(uint16_t bits);

    void logTransition(core::HomingState previous, const core::HomingStep &step);

    core::HomingController _controller;
    UiPorts &_ui;
    core::MountState &_state;
    StepperAxis _axis;
    ports::IHomingSensor &_sensor;
    uint8_t _savedRate;
    bool _wasTracking;
};

}  // namespace app
