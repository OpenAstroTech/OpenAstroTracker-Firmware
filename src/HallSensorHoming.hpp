#ifndef _HALLSENSORHOMING_HPP
#define _HALLSENSORHOMING_HPP

#include "Types.hpp"
#include "core/HomingController.hpp"
#include "app/UiPorts.hpp"
#include "ports/IHomingSensor.hpp"
#include "ports/IStatusFlags.hpp"

/////////////////////////////////
//
// class HallSensorHoming
//
/////////////////////////////////
/// Binds the hall-sensor homing state machine to one mount axis.
///
/// The decisions live in `core::HomingController`; this class reads the sensor and the
/// axis each tick, hands them over, and performs the single action that comes back. It
/// also owns the two pieces of mount policy the state machine does not: the slew rate
/// to restore and whether tracking was on when the mount was set up.
class HallSensorHoming
{
  private:
    core::HomingController _controller;
    app::UiPorts &_ui;
    ports::IStatusFlags &_status;
    StepperAxis _axis;
    ports::IHomingSensor &_sensor;
    uint8_t _savedRate;
    bool _wasTracking;

  public:
    HallSensorHoming(app::UiPorts &ports, ports::IStatusFlags &status, StepperAxis axis, ports::IHomingSensor &sensor);

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
};

#endif
