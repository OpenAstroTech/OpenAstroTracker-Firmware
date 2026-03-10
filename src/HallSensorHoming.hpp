#ifndef _HALLSENSORHOMING_HPP
#define _HALLSENSORHOMING_HPP

#include "Types.hpp"
#include "Mount.hpp"

#define HOMING_RESULT_SUCCEEDED                   1
#define HOMING_RESULT_HOMING_NEVER_RUN            0
#define HOMING_RESULT_HOMING_IN_PROGRESS          -1
#define HOMING_RESULT_CANT_MOVE_OFF_SENSOR        -2
#define HOMING_RESULT_CANT_FIND_SENSOR_ON_REVERSE -3
#define HOMING_RESULT_CANT_FIND_SENSOR_END        -4

class Mount;

enum HomingState
{
    HOMING_MOVE_OFF,
    HOMING_MOVING_OFF,
    HOMING_STOP_AT_TIME,
    HOMING_WAIT_FOR_STOP,
    HOMING_START_FIND_START,
    HOMING_FINDING_START,
    HOMING_FINDING_START_REVERSE,
    HOMING_FINDING_END,
    HOMING_RANGE_FOUND,
    HOMING_FAILED,
    HOMING_SUCCESSFUL,

    HOMING_NOT_ACTIVE
};

struct HomingData {
    HomingState state;
    HomingState nextState;
    int pinState;
    int lastPinState;
    int pinChangeCount;
    int savedRate;
    int initialDir;
    long searchDistance;
    long position[2];
    long offset;
    long startPos;
    unsigned long stopAt;
};

/////////////////////////////////
//
// class HallSensorHoming
//
/////////////////////////////////
class HallSensorHoming
{
  private:
    HomingData _homingData;
    Mount *_pMount;
    StepperAxis _axis;
    long _stepsPerDegree;
    int _sensorPin;
    int _activeState;
    int _lastResult;
    bool _wasTracking;

  public:
    HallSensorHoming(Mount *mount, StepperAxis axis, long stepsPerDegree, int sensorPin, int activeState, int32_t offset)
    {
        _homingData.state          = HomingState::HOMING_NOT_ACTIVE;
        _homingData.offset         = offset;
        _homingData.pinChangeCount = 0;
        _pMount                    = mount;
        _axis                      = axis;
        _stepsPerDegree            = stepsPerDegree;
        _sensorPin                 = sensorPin;
        _activeState               = activeState;
        _lastResult                = HOMING_RESULT_HOMING_NEVER_RUN;
        _wasTracking               = mount->isSlewingTRK();
    }

    bool findHomeByHallSensor(int initialDirection, int searchDistanceDegrees);
    void processHomingProgress();
    String getHomingState(HomingState state) const;
    HomingState getHomingState() const;
    bool isIdleOrComplete() const;
    String getLastResult() const;
};

#endif
