#pragma once

#include <stdint.h>

namespace core
{

/// How a homing run ended. The values match the firmware's `HOMING_RESULT_*` constants.
enum class HomingResult
{
    NeverRun,
    InProgress,
    Succeeded,
    CantMoveOffSensor,
    CantFindSensorOnReverse,
    CantFindSensorEnd,
};

enum class HomingState
{
    NotActive,
    MoveOff,
    MovingOff,
    StopAtTime,
    WaitForStop,
    StartFindStart,
    FindingStart,
    FindingStartReverse,
    FindingEnd,
    RangeFound,
    Failed,
    Successful,
};

/// What the caller must do after `update()`. The state machine itself never touches a
/// motor or a pin; it only decides, so the caller keeps every side effect.
enum class HomingAction
{
    None,       ///< Nothing to do this tick.
    MoveBy,     ///< Move the axis by `HomingStep::steps`.
    StopAxis,   ///< Stop the axis where it is.
    Succeeded,  ///< Homing finished: set home, restore the slew rate, resume tracking.
    Failed,     ///< Homing failed: restore the slew rate and slew back to `startPosition()`.
};

struct HomingStep {
    HomingAction action;
    long steps;
};

/// The hall-sensor homing state machine.
///
/// A port of the procedure that used to live in `HallSensorHoming`, split so the
/// decisions are testable without a mount: `update()` takes one tick of observations
/// (sensor level, axis running, axis position, time) and returns the single action the
/// caller must perform. The transition graph, the debounce counters and the arithmetic
/// are unchanged — including the two-tick hand-off through `HomingState::WaitForStop`,
/// which the original used to wait for the motor to come to rest.
///
/// This is deliberately not a `ports::IHomingSensor` owner yet: on ESP32 and RP2040 the
/// firmware still calls the state machine from the step generator, so the caller has to
/// keep reading the pin there. Moving the call to the main loop (and with it the
/// ISR-side mount writes) needs hardware verification and is tracked in PLAN.md.
///
/// Threading: it inherits whatever context the caller runs it in; it has no locks and
/// no side effects, so the same instance must not be driven from two contexts at once.
class HomingController
{
  public:
    /// Starts the procedure. `startPosition` is where the axis was when homing was
    /// requested, `originOffset` the configured home offset in steps. `sensorActive`
    /// decides whether the run begins by moving off the sensor or by looking for it.
    void start(
        int initialDirection, int searchDistanceDegrees, long stepsPerDegree, long startPosition, long originOffset, bool sensorActive)
    {
        _result                 = HomingResult::InProgress;
        _startPosition          = startPosition;
        _initialDirection       = initialDirection;
        _searchDistance         = searchDistanceDegrees;
        _stepsPerDegree         = stepsPerDegree;
        _offset                 = originOffset;
        _pinChangeCount         = 0;
        _lastPinState           = false;
        _position[HOMING_START] = 0;
        _position[HOMING_END]   = 0;
        _state                  = sensorActive ? HomingState::MoveOff : HomingState::StartFindStart;
    }

    /// Advances the state machine by one tick. The caller passes what it observed and
    /// performs the returned action.
    HomingStep update(bool sensorActive, bool axisRunning, long axisPosition, uint32_t now)
    {
        HomingStep step = {HomingAction::None, 0};

        switch (_state)
        {
            case HomingState::NotActive:
                break;

            case HomingState::StopAtTime:
                if ((int32_t) (now - _stopAt) > 0)
                {
                    stopAxis(step);
                    _state = HomingState::WaitForStop;
                }
                break;

            case HomingState::WaitForStop:
                if (!axisRunning)
                {
                    _state     = _nextState;
                    _nextState = HomingState::NotActive;
                }
                break;

            case HomingState::MoveOff:
                moveBy(step, -_initialDirection * _stepsPerDegree * 15);
                _state = HomingState::MovingOff;
                break;

            case HomingState::MovingOff:
                if (!axisRunning)
                {
                    _state  = HomingState::Failed;
                    _result = HomingResult::CantMoveOffSensor;
                }
                else if (!sensorActive)
                {
                    _stopAt    = now + 2000;
                    _state     = HomingState::StopAtTime;
                    _nextState = HomingState::StartFindStart;
                }
                break;

            case HomingState::StartFindStart:
                // Move in the initial direction, far enough to leave the sensor behind.
                _lastPinState           = sensorActive;
                _position[HOMING_START] = 0;
                _position[HOMING_END]   = 0;
                moveBy(step, _initialDirection * _stepsPerDegree * _searchDistance);
                _state = HomingState::FindingStart;
                break;

            case HomingState::FindingStart:
                if (!axisRunning)
                {
                    // Not found on the forward pass: look twice as far the other way.
                    moveBy(step, -_initialDirection * _stepsPerDegree * _searchDistance * 2L);
                    _state = HomingState::FindingStartReverse;
                }
                else
                {
                    scanEdge(sensorActive, axisPosition, HOMING_START);
                    if (_pinChangeCount > kDebounceTicks)
                    {
                        // The sensor edge is stable: keep going so the far edge is reached.
                        moveBy(step, _initialDirection * _stepsPerDegree * _searchDistance);
                        _lastPinState   = sensorActive;
                        _pinChangeCount = 0;
                        _state          = HomingState::FindingEnd;
                    }
                }
                break;

            case HomingState::FindingStartReverse:
                if (!axisRunning)
                {
                    _state  = HomingState::Failed;
                    _result = HomingResult::CantFindSensorOnReverse;
                }
                else
                {
                    scanEdge(sensorActive, axisPosition, HOMING_START);
                    if (_pinChangeCount > kDebounceTicks)
                    {
                        moveBy(step, -_initialDirection * _stepsPerDegree * _searchDistance);
                        _lastPinState   = sensorActive;
                        _pinChangeCount = 0;
                        _state          = HomingState::FindingEnd;
                    }
                }
                break;

            case HomingState::FindingEnd:
                if (!axisRunning)
                {
                    _state  = HomingState::Failed;
                    _result = HomingResult::CantFindSensorEnd;
                }
                else
                {
                    scanEdge(sensorActive, axisPosition, HOMING_END);
                    if (_pinChangeCount > kDebounceTicks)
                    {
                        _lastPinState   = sensorActive;
                        _pinChangeCount = 0;
                        _state          = HomingState::WaitForStop;
                        _nextState      = HomingState::RangeFound;
                        stopAxis(step);
                    }
                }
                break;

            case HomingState::RangeFound:
                {
                    const long midpoint = (_position[HOMING_START] + _position[HOMING_END]) / 2;
                    moveBy(step, midpoint - axisPosition - _offset);
                    _state     = HomingState::WaitForStop;
                    _nextState = HomingState::Successful;
                }
                break;

            case HomingState::Successful:
                _result     = HomingResult::Succeeded;
                _state      = HomingState::NotActive;
                step.action = HomingAction::Succeeded;
                break;

            case HomingState::Failed:
                _state      = HomingState::NotActive;
                step.action = HomingAction::Failed;
                break;
        }

        return step;
    }

    bool isActive() const
    {
        return _state != HomingState::NotActive;
    }

    HomingState state() const
    {
        return _state;
    }

    HomingResult result() const
    {
        return _result;
    }

    long startPosition() const
    {
        return _startPosition;
    }

    int searchDistance() const
    {
        return _searchDistance;
    }

    /// Where the sensor edge was first seen on the pass that found it, and where the far
    /// edge was seen. Zero until recorded.
    long sensorStartPosition() const
    {
        return _position[HOMING_START];
    }

    long sensorEndPosition() const
    {
        return _position[HOMING_END];
    }

  private:
    /// Number of consecutive differing samples that make a sensor edge count. The
    /// original called this "5 cycles" and tested `> 4`.
    static const int kDebounceTicks = 4;

    static const int HOMING_START = 0;
    static const int HOMING_END   = 1;

    static void moveBy(HomingStep &step, long steps)
    {
        step.action = HomingAction::MoveBy;
        step.steps  = steps;
    }

    static void stopAxis(HomingStep &step)
    {
        step.action = HomingAction::StopAxis;
        step.steps  = 0;
    }

    /// Records the first sample of a sensor edge into `slot`, and counts the ticks for
    /// which it persists. `_lastPinState` is the level seen when the state was entered,
    /// not the previous sample, which is what the original debounce did.
    void scanEdge(bool sensorActive, long axisPosition, int slot)
    {
        if (_lastPinState != sensorActive)
        {
            if (_pinChangeCount == 0)
            {
                _position[slot] = axisPosition;
            }
            _pinChangeCount++;
        }
        else
        {
            _position[slot] = 0;
            _pinChangeCount = 0;
        }
    }

    HomingResult _result   = HomingResult::NeverRun;
    HomingState _state     = HomingState::NotActive;
    HomingState _nextState = HomingState::NotActive;
    long _stepsPerDegree   = 0;
    int _initialDirection  = 0;
    int _searchDistance    = 0;
    long _offset           = 0;
    long _startPosition    = 0;
    long _position[2]      = {0, 0};
    int _pinChangeCount    = 0;
    bool _lastPinState     = false;
    uint32_t _stopAt       = 0;
};

}  // namespace core
