#ifndef _HALLSENSORHOMING_HPP
#define _HALLSENSORHOMING_HPP

#include "Types.hpp"
#include "Mount.hpp"

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

  public:
    HallSensorHoming(Mount *mount, StepperAxis axis, long stepsPerDegree, int sensorPin, int32_t offset)
    {
        _homingData.state = HomingState::HOMING_NOT_ACTIVE;
        _homingData.offset = offset;
        _pMount           = mount;
        _axis             = axis;
        _stepsPerDegree   = stepsPerDegree;
        _sensorPin        = sensorPin;
    }

    bool findHomeByHallSensor(int initialDirection, int searchDistanceDegrees);
    void processHomingProgress();
    String getHomingState(HomingState state) const;
    HomingState getHomingState() const;
    bool isIdleOrComplete() const;
};

#endif
