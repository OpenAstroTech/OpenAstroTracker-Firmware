#include "../Configuration.hpp"
#include "Utility.hpp"
#include "EPROMStore.hpp"
#include "HallSensorHoming.hpp"
#include "libs/MappedDict/MappedDict.hpp"

#define HOMING_START_PIN_POSITION 0
#define HOMING_END_PIN_POSITION   1

/////////////////////////////////
//
// getHomingState
//
/////////////////////////////////
String HallSensorHoming::getHomingState(HomingState state) const
{
    MappedDict<HomingState, String>::DictEntry_t lookupTable[] = {
        {HOMING_MOVE_OFF, F("MOVE_OFF")},
        {HOMING_MOVING_OFF, F("MOVING_OFF")},
        {HOMING_STOP_AT_TIME, F("STOP_AT_TIME")},
        {HOMING_WAIT_FOR_STOP, F("WAIT_FOR_STOP")},
        {HOMING_START_FIND_START, F("START_FIND_START")},
        {HOMING_FINDING_START, F("FINDING_START")},
        {HOMING_FINDING_START_REVERSE, F("FINDING_START_REVERSE")},
        {HOMING_FINDING_END, F("FINDING_END")},
        {HOMING_RANGE_FOUND, F("RANGE_FOUND")},
        {HOMING_FAILED, F("FAILED")},
        {HOMING_SUCCESSFUL, F("SUCCESSFUL")},
        {HOMING_NOT_ACTIVE, F("NOT_ACTIVE")},
    };

    auto strLookup = MappedDict<HomingState, String>(lookupTable, ARRAY_SIZE(lookupTable));
    String rtnStr;
    if (strLookup.tryGet(state, &rtnStr))
    {
        return rtnStr;
    }
    return F("WTF_STATE");
}

HomingState HallSensorHoming::getHomingState() const
{
    return _homingData.state;
}

/////////////////////////////////
//
// findHomeByHallSensor
//
/////////////////////////////////
bool HallSensorHoming::findHomeByHallSensor(int initialDirection, int searchDistance)
{
    _homingData.startPos       = _pMount->getCurrentStepperPosition(_axis);
    _homingData.savedRate      = _pMount->getSlewRate();
    _homingData.initialDir     = initialDirection;
    _homingData.searchDistance = searchDistance;

    _pMount->setSlewRate(4);
    _pMount->setStatusFlag(STATUS_FINDING_HOME);

    LOG(DEBUG_STEPPERS,
        "[HOMING]: Start homing procedure. Axis %d, StepsPerDegree: %l, SearchDist: %d",
        (int) _axis,
        _stepsPerDegree,
        searchDistance);

    // Check where we are over the sensor already
    if (digitalRead(_sensorPin) == LOW)
    {
        _homingData.state = HomingState::HOMING_MOVE_OFF;
        LOG(DEBUG_STEPPERS, "[HOMING]: Sensor is signalled, move off sensor started");
    }
    else
    {
        _homingData.state = HomingState::HOMING_START_FIND_START;
        LOG(DEBUG_STEPPERS, "[HOMING]: Sensor is not signalled, find start of range");
    }

    return true;
}

/////////////////////////////////
//
// processHomingProgress
//
/////////////////////////////////
void HallSensorHoming::processHomingProgress()
{
    switch (_homingData.state)
    {
        case HomingState::HOMING_NOT_ACTIVE:
            break;

        case HomingState::HOMING_STOP_AT_TIME:
            {
                if (millis() > _homingData.stopAt)
                {
                    LOG(DEBUG_STEPPERS,
                        "[HOMING]: Initiating stop at requested time. Advance to state %s",
                        getHomingState(HomingState::HOMING_WAIT_FOR_STOP).c_str());
                    _pMount->stopSlewing(_axis);
                    _homingData.state = HomingState::HOMING_WAIT_FOR_STOP;
                }
            }
            break;

        case HomingState::HOMING_WAIT_FOR_STOP:
            {
                if (!_pMount->isAxisRunning(_axis))
                {
                    LOG(DEBUG_STEPPERS,
                        "[HOMING]: Stepper has stopped as expected, advancing to next state %s",
                        getHomingState(_homingData.nextState).c_str());
                    _homingData.state     = _homingData.nextState;
                    _homingData.nextState = HomingState::HOMING_NOT_ACTIVE;
                }
            }
            break;

        case HomingState::HOMING_MOVE_OFF:
            {
                LOG(DEBUG_STEPPERS,
                    "[HOMING]: Currently over Sensor, so moving off of it by reverse 15deg. (%l steps). Advance to %s",
                    (-_homingData.initialDir * _stepsPerDegree * 15),
                    getHomingState(HomingState::HOMING_MOVING_OFF).c_str());
                _pMount->moveStepperBy(_axis, -_homingData.initialDir * _stepsPerDegree * 15);
                _homingData.state = HomingState::HOMING_MOVING_OFF;
            }
            break;

        case HomingState::HOMING_MOVING_OFF:
            {
                if (_pMount->isAxisRunning(_axis))
                {
                    int homingPinState = digitalRead(_sensorPin);
                    if (homingPinState == HIGH)
                    {
                        LOG(DEBUG_STEPPERS,
                            "[HOMING]: Stepper has moved off sensor... stopping in 2s. Advance to %s",
                            getHomingState(HomingState::HOMING_STOP_AT_TIME).c_str());
                        _homingData.stopAt    = millis() + 2000;
                        _homingData.state     = HomingState::HOMING_STOP_AT_TIME;
                        _homingData.nextState = HomingState::HOMING_START_FIND_START;
                    }
                }
                else
                {
                    LOG(DEBUG_STEPPERS,
                        "[HOMING]: Stepper was unable to move off sensor... homing failed! Advance to %s",
                        getHomingState(HomingState::HOMING_FAILED).c_str());
                    _homingData.state = HomingState::HOMING_FAILED;
                }
            }
            break;

        case HomingState::HOMING_START_FIND_START:
            {
                long distance = _homingData.initialDir * _stepsPerDegree * _homingData.searchDistance;
                LOG(DEBUG_STEPPERS,
                    "[HOMING]: Finding start on forward pass by moving by %l deg (%l steps). Advance to %s",
                    _homingData.searchDistance,
                    distance,
                    getHomingState(HomingState::HOMING_FINDING_START).c_str());
                _homingData.pinState = _homingData.lastPinState = digitalRead(_sensorPin);
                _homingData.position[HOMING_START_PIN_POSITION] = 0;
                _homingData.position[HOMING_END_PIN_POSITION]   = 0;

                // Move in initial direction
                _pMount->moveStepperBy(_axis, distance);

                _homingData.state = HomingState::HOMING_FINDING_START;
            }
            break;

        case HomingState::HOMING_FINDING_START:
            {
                if (_pMount->isAxisRunning(_axis))
                {
                    int homingPinState = digitalRead(_sensorPin);
                    if (_homingData.lastPinState != homingPinState)
                    {
                        LOG(DEBUG_STEPPERS,
                            "[HOMING]: Found start of sensor, continuing until end is found. Advance to %s",
                            getHomingState(HomingState::HOMING_FINDING_END).c_str());
                        // Found the start of the sensor, keep going until we find the end
                        _homingData.position[HOMING_START_PIN_POSITION] = _pMount->getCurrentStepperPosition(_axis);
                        _homingData.lastPinState                        = homingPinState;
                        _homingData.state                               = HomingState::HOMING_FINDING_END;
                    }
                }
                else
                {
                    // Did not find start. Go reverse direction for twice the distance
                    long distance = -_homingData.initialDir * _stepsPerDegree * _homingData.searchDistance * 2L;
                    LOG(DEBUG_STEPPERS,
                        "[HOMING]: Hall not found on forward pass. Moving reverse by %l deg (%l steps). Advance to %s",
                        2 * _homingData.searchDistance,
                        distance,
                        getHomingState(HomingState::HOMING_FINDING_START_REVERSE).c_str());
                    _pMount->moveStepperBy(_axis, distance);
                    _homingData.state = HomingState::HOMING_FINDING_START_REVERSE;
                }
            }
            break;

        case HomingState::HOMING_FINDING_START_REVERSE:
            {
                if (_pMount->isAxisRunning(_axis))
                {
                    int homingPinState = digitalRead(_sensorPin);
                    if (_homingData.lastPinState != homingPinState)
                    {
                        LOG(DEBUG_STEPPERS,
                            "[HOMING]: Found start of sensor reverse, continuing until end is found. Advance to %s",
                            getHomingState(HomingState::HOMING_FINDING_END).c_str());
                        _homingData.position[HOMING_START_PIN_POSITION] = _pMount->getCurrentStepperPosition(_axis);
                        _homingData.lastPinState                        = homingPinState;
                        _homingData.state                               = HomingState::HOMING_FINDING_END;
                    }
                }
                else
                {
                    // Did not find start in either direction, abort.
                    LOG(DEBUG_STEPPERS,
                        "[HOMING]: Sensor not found on reverse pass either. Homing Failed. Advance to %s",
                        getHomingState(HomingState::HOMING_FAILED).c_str());
                    _homingData.state = HomingState::HOMING_FAILED;
                }
            }
            break;

        case HomingState::HOMING_FINDING_END:
            {
                if (_pMount->isAxisRunning(_axis))
                {
                    int homingPinState = digitalRead(_sensorPin);
                    if (_homingData.lastPinState != homingPinState)
                    {
                        LOG(DEBUG_STEPPERS,
                            "[HOMING]: Found end of sensor, stopping... Advance to %s",
                            getHomingState(HomingState::HOMING_WAIT_FOR_STOP).c_str());
                        _homingData.position[HOMING_END_PIN_POSITION] = _pMount->getCurrentStepperPosition(_axis);
                        _homingData.lastPinState                      = homingPinState;
                        _homingData.state                             = HomingState::HOMING_WAIT_FOR_STOP;
                        _homingData.nextState                         = HomingState::HOMING_RANGE_FOUND;
                        _pMount->stopSlewing(_axis);
                    }
                }
                else
                {
                    LOG(DEBUG_STEPPERS,
                        "[HOMING]: End of sensor not found! Advance to %s",
                        getHomingState(HomingState::HOMING_FAILED).c_str());
                    _homingData.state = HomingState::HOMING_FAILED;
                }
            }
            break;

        case HomingState::HOMING_RANGE_FOUND:
            {
                LOG(DEBUG_STEPPERS,
                    "[HOMING]: Stepper stopped, Hall sensor found! Range: [%l to %l] size: %l",
                    _homingData.position[HOMING_START_PIN_POSITION],
                    _homingData.position[HOMING_END_PIN_POSITION],
                    _homingData.position[HOMING_START_PIN_POSITION] - _homingData.position[HOMING_END_PIN_POSITION]);

                long midPos = (_homingData.position[HOMING_START_PIN_POSITION] + _homingData.position[HOMING_END_PIN_POSITION]) / 2;

                LOG(DEBUG_STEPPERS,
                    "[HOMING]: Moving home by %l - (%l) - (%l), so %l steps. Advance to %s",
                    midPos,
                    _pMount->getCurrentStepperPosition(_axis),
                    _homingData.offset,
                    midPos - _pMount->getCurrentStepperPosition(_axis) - _homingData.offset,
                    getHomingState(HomingState::HOMING_WAIT_FOR_STOP).c_str());

                _pMount->moveStepperBy(_axis, midPos - _pMount->getCurrentStepperPosition(_axis) - _homingData.offset);

                _homingData.state     = HomingState::HOMING_WAIT_FOR_STOP;
                _homingData.nextState = HomingState::HOMING_SUCCESSFUL;
            }
            break;

        case HomingState::HOMING_SUCCESSFUL:
            {
                LOG(DEBUG_STEPPERS,
                    "[HOMING]: Successfully homed! Setting home and restoring Rate setting. Advance to %s",
                    getHomingState(HomingState::HOMING_NOT_ACTIVE).c_str());
                _homingData.state = HomingState::HOMING_NOT_ACTIVE;
                _pMount->setHome(false);
                _pMount->setSlewRate(_homingData.savedRate);
                _pMount->clearStatusFlag(STATUS_FINDING_HOME);
                _pMount->startSlewing(TRACKING);
            }
            break;

        case HomingState::HOMING_FAILED:
            {
                LOG(DEBUG_STEPPERS,
                    "[HOMING]: Failed to home! Restoring Rate setting and slewing to start position. Advance to %s",
                    getHomingState(HomingState::HOMING_NOT_ACTIVE).c_str());
                _pMount->setSlewRate(_homingData.savedRate);
                _homingData.state = HomingState::HOMING_NOT_ACTIVE;
                _pMount->clearStatusFlag(STATUS_FINDING_HOME);
                _pMount->setStatusFlag(STATUS_SLEWING | STATUS_SLEWING_TO_TARGET);
                _pMount->moveStepperTo(_axis, _homingData.startPos);
            }
            break;

        default:
            LOG(DEBUG_STEPPERS, "[HOMING]: Unhandled state (%d)! ", _homingData.state);
            break;
    }
}

bool HallSensorHoming::isIdleOrComplete() const
{
    if (_homingData.state == HomingState::HOMING_FAILED)
        return true;
    if (_homingData.state == HomingState::HOMING_SUCCESSFUL)
        return true;
    if (_homingData.state == HomingState::HOMING_NOT_ACTIVE)
        return true;
    return false;
}