#include "../Configuration.hpp"
#include "Utility.hpp"
#include "Mount.hpp"
#include "EPROMStore.hpp"
#include "HallSensorHoming.hpp"
#include "Utility.hpp"
#include "libs/MappedDict/MappedDict.hpp"

/////////////////////////////////
//
// getHomingState
//
/////////////////////////////////
String HallSensorHoming::getHomingState(core::HomingState state) const
{
    MappedDict<core::HomingState, String>::DictEntry_t lookupTable[] = {
        {core::HomingState::MoveOff, F("MOVE_OFF")},
        {core::HomingState::MovingOff, F("MOVING_OFF")},
        {core::HomingState::StopAtTime, F("STOP_AT_TIME")},
        {core::HomingState::WaitForStop, F("WAIT_FOR_STOP")},
        {core::HomingState::StartFindStart, F("START_FIND_START")},
        {core::HomingState::FindingStart, F("FINDING_START")},
        {core::HomingState::FindingStartReverse, F("FINDING_START_REVERSE")},
        {core::HomingState::FindingEnd, F("FINDING_END")},
        {core::HomingState::RangeFound, F("RANGE_FOUND")},
        {core::HomingState::Failed, F("FAILED")},
        {core::HomingState::Successful, F("SUCCESSFUL")},
        {core::HomingState::NotActive, F("NOT_ACTIVE")},
    };

    auto strLookup = MappedDict<core::HomingState, String>(lookupTable, ARRAY_SIZE(lookupTable));
    String rtnStr;
    if (strLookup.tryGet(state, &rtnStr))
    {
        return rtnStr;
    }
    return F("WTF_STATE");
}

/////////////////////////////////
//
// getHomingState
//
/////////////////////////////////
core::HomingState HallSensorHoming::getHomingState() const
{
    return _controller.state();
}

/////////////////////////////////
//
// getLastResult
//
/////////////////////////////////
core::HomingResult HallSensorHoming::homingResult() const
{
    return _controller.result();
}

String HallSensorHoming::getLastResult() const
{
    MappedDict<core::HomingResult, String>::DictEntry_t lookupTable[] = {
        {core::HomingResult::Succeeded, F("SUCCEEDED")},
        {core::HomingResult::NeverRun, F("NEVER RUN")},
        {core::HomingResult::InProgress, F("IN PROGRESS")},
        {core::HomingResult::CantMoveOffSensor, F("CANT MOVE OFF SENSOR")},
        {core::HomingResult::CantFindSensorOnReverse, F("CANT FIND SENSOR ON REVERSE")},
        {core::HomingResult::CantFindSensorEnd, F("CANT FIND SENSOR END")},
    };

    auto strLookup = MappedDict<core::HomingResult, String>(lookupTable, ARRAY_SIZE(lookupTable));
    String rtnStr;
    if (strLookup.tryGet(_controller.result(), &rtnStr))
    {
        return rtnStr;
    }
    return F("WTF_STATE");
}

/////////////////////////////////
//
// CTOR
//
/////////////////////////////////
HallSensorHoming::HallSensorHoming(Mount *mount, StepperAxis axis, ports::IHomingSensor &sensor)
    : _pMount(mount), _axis(axis), _sensor(sensor), _savedRate(0),
      _wasTracking(mount->isSlewingTRK())
{
}

/////////////////////////////////
//
// findHomeByHallSensor
//
/////////////////////////////////
bool HallSensorHoming::findHomeByHallSensor(int initialDirection, int searchDistance)
{
    const long stepsPerDegree = (long) _pMount->getStepsPerDegree(_axis);
    _savedRate                = (uint8_t) _pMount->getSlewRate();
    _controller.start(initialDirection,
                      searchDistance,
                      stepsPerDegree,
                      _pMount->getCurrentStepperPosition(_axis),
                      _pMount->getHomingOffset(_axis),
                      sensorIsActive());

    _pMount->setSlewRate(4);
    _pMount->setStatusFlag(STATUS_FINDING_HOME);

    LOG(DEBUG_STEPPERS,
        "[HOMING]: Start homing procedure. Axis %d, StepsPerDegree: %l, SearchDist: %d",
        (int) _axis,
        stepsPerDegree,
        searchDistance);
    if (sensorIsActive())
    {
        LOG(DEBUG_STEPPERS, "[HOMING]: Sensor is signalled, starting in %s", getHomingState(_controller.state()).c_str());
    }
    else
    {
        LOG(DEBUG_STEPPERS, "[HOMING]: Sensor is not signalled, starting in %s", getHomingState(_controller.state()).c_str());
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
    const core::HomingState previous = _controller.state();
    const core::HomingStep step
        = _controller.update(sensorIsActive(), _pMount->isAxisRunning(_axis), _pMount->getCurrentStepperPosition(_axis), millis());

    logTransition(previous, step);

    switch (step.action)
    {
        case core::HomingAction::None:
            break;

        case core::HomingAction::MoveBy:
            _pMount->moveStepperBy(_axis, step.steps);
            break;

        case core::HomingAction::StopAxis:
            _pMount->stopSlewing(_axis);
            break;

        case core::HomingAction::Succeeded:
            _pMount->setHome(false);
            _pMount->setSlewRate(_savedRate);
            _pMount->clearStatusFlag(STATUS_FINDING_HOME);
            if (_wasTracking)
            {
                _pMount->startSlewing(TRACKING);
            }
            break;

        case core::HomingAction::Failed:
            _pMount->setSlewRate(_savedRate);
            _pMount->clearStatusFlag(STATUS_FINDING_HOME);
            _pMount->setStatusFlag(STATUS_SLEWING | STATUS_SLEWING_TO_TARGET);
            _pMount->moveStepperTo(_axis, _controller.startPosition());
            break;
    }
}

/////////////////////////////////
//
// isIdleOrComplete
//
/////////////////////////////////
bool HallSensorHoming::isIdleOrComplete() const
{
    return !_controller.isActive();
}

bool HallSensorHoming::sensorIsActive() const
{
    return _sensor.isTriggered();
}

/// One line per state change, carrying the action that change produced. The state names
/// and the move distances are the ones the previous per-state logging printed.
void HallSensorHoming::logTransition(core::HomingState previous, const core::HomingStep &step)
{
    const core::HomingState current = _controller.state();
    if (current == previous)
    {
        return;
    }

    LOG(DEBUG_STEPPERS, "[HOMING]: %s -> %s", getHomingState(previous).c_str(), getHomingState(current).c_str());

    switch (step.action)
    {
        case core::HomingAction::MoveBy:
            LOG(DEBUG_STEPPERS, "[HOMING]: moving by %l steps", step.steps);
            break;
        case core::HomingAction::StopAxis:
            LOG(DEBUG_STEPPERS, "[HOMING]: stopping axis");
            break;
        case core::HomingAction::Succeeded:
            LOG(DEBUG_STEPPERS,
                "[HOMING]: homed, sensor range [%l, %l]",
                _controller.sensorStartPosition(),
                _controller.sensorEndPosition());
            break;
        case core::HomingAction::Failed:
            LOG(DEBUG_STEPPERS, "[HOMING]: failed (%s), returning to %l", getLastResult().c_str(), _controller.startPosition());
            break;
        case core::HomingAction::None:
            break;
    }
}
