#pragma once

#include <stdint.h>

#include "ports/IStepperAxis.hpp"

namespace fakes
{

/// A stepper axis that records what was asked of it. Position bookkeeping follows the
/// stepper libraries: `moveTo`/`move` set the target, `runToNewPosition` moves the
/// current position there immediately, and `stop` freezes the axis in place.
class FakeStepperAxis final : public ports::IStepperAxis
{
  public:
    ports::AxisSnapshot snapshot() const override
    {
        ports::AxisSnapshot state;
        state.currentPosition = _current;
        state.targetPosition  = _target;
        state.distanceToGo    = _target - _current;
        state.speed           = _speed;
        state.isRunning       = _current != _target;
        return state;
    }

    long currentPosition() override
    {
        return _current;
    }

    void setCurrentPosition(long steps) override
    {
        _current = steps;
        _target  = steps;
        _setCurrentPositionCalls++;
    }

    long targetPosition() override
    {
        return _target;
    }

    long distanceToGo() override
    {
        return _target - _current;
    }

    void move(long relativeSteps) override
    {
        _target = _current + relativeSteps;
        _moveCalls++;
    }

    void moveTo(long absoluteSteps) override
    {
        _target = absoluteSteps;
        _moveToCalls++;
    }

    void runToNewPosition(long absoluteSteps) override
    {
        _target  = absoluteSteps;
        _current = absoluteSteps;
        _runToNewPositionCalls++;
    }

    void runToPosition() override
    {
        _current = _target;
        _runToPositionCalls++;
    }

    void run() override
    {
        _runCalls++;
    }

    void runSpeed() override
    {
        _runSpeedCalls++;
    }

    void stop() override
    {
        _target = _current;
        _stopCalls++;
    }

    bool isRunning() override
    {
        return _current != _target;
    }

    void setMaxSpeed(float stepsPerSecond) override
    {
        _maxSpeed = stepsPerSecond;
    }

    float maxSpeed() override
    {
        return _maxSpeed;
    }

    void setAcceleration(float stepsPerSecondSquared) override
    {
        _acceleration = stepsPerSecondSquared;
    }

    void setSpeed(float stepsPerSecond) override
    {
        _speed = stepsPerSecond;
        _setSpeedCalls++;
    }

    float speed() override
    {
        return _speed;
    }

    void setPinsInverted(bool directionInvert, bool stepInvert, bool enableInvert) override
    {
        (void) directionInvert;
        (void) stepInvert;
        (void) enableInvert;
    }

    // --- test controls / assertions ---

    void setPosition(long steps)
    {
        _current = steps;
        _target  = steps;
    }

    uint32_t setSpeedCalls() const
    {
        return _setSpeedCalls;
    }

    uint32_t stopCalls() const
    {
        return _stopCalls;
    }

    uint32_t runToNewPositionCalls() const
    {
        return _runToNewPositionCalls;
    }

    uint32_t setCurrentPositionCalls() const
    {
        return _setCurrentPositionCalls;
    }

  private:
    long _current                     = 0;
    long _target                      = 0;
    float _speed                      = 0;
    float _maxSpeed                   = 0;
    float _acceleration               = 0;
    uint32_t _moveCalls               = 0;
    uint32_t _moveToCalls             = 0;
    uint32_t _setSpeedCalls           = 0;
    uint32_t _stopCalls               = 0;
    uint32_t _runCalls                = 0;
    uint32_t _runSpeedCalls           = 0;
    uint32_t _runToPositionCalls      = 0;
    uint32_t _runToNewPositionCalls   = 0;
    uint32_t _setCurrentPositionCalls = 0;
};

}  // namespace fakes
