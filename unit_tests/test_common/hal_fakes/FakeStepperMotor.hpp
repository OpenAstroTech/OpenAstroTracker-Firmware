#pragma once

#include <stdint.h>

#include "hal/IStepperMotor.hpp"

namespace fakes
{

/// A motion engine that records what was asked of it. Position bookkeeping follows
/// the stepper libraries: `moveTo`/`move` set the target, `runToPosition` (or a test
/// call to `advanceToTarget`) moves the current position to it.
class FakeStepperMotor final : public hal::IStepperMotor
{
  public:
    void moveTo(long absoluteSteps) override
    {
        _target = absoluteSteps;
        _moveToCalls++;
    }

    void move(long relativeSteps) override
    {
        _target = _current + relativeSteps;
        _moveCalls++;
    }

    void setMaxSpeed(float stepsPerSecond) override
    {
        _maxSpeed = stepsPerSecond;
    }

    void setAcceleration(float stepsPerSecondSquared) override
    {
        _acceleration = stepsPerSecondSquared;
    }

    void setSpeed(float stepsPerSecond) override
    {
        _speed = stepsPerSecond;
    }

    void setPinsInverted(bool directionInvert, bool stepInvert, bool enableInvert) override
    {
        _inverted = directionInvert;
        (void) stepInvert;
        (void) enableInvert;
        _invertCalls++;
    }

    void run() override
    {
        _runCalls++;
    }

    void runSpeed() override
    {
        _runSpeedCalls++;
    }

    void runToPosition() override
    {
        _current = _target;
        _runToPositionCalls++;
    }

    void runToNewPosition(long absoluteSteps) override
    {
        _target  = absoluteSteps;
        _current = absoluteSteps;
        _runToNewPositionCalls++;
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

    long currentPosition() override
    {
        return _current;
    }

    void setCurrentPosition(long steps) override
    {
        _current = steps;
    }

    long targetPosition() override
    {
        return _target;
    }

    long distanceToGo() override
    {
        return _target - _current;
    }

    float speed() override
    {
        return _speed;
    }

    float maxSpeed() override
    {
        return _maxSpeed;
    }

    void setMicrosteps(uint16_t microsteps) override
    {
        _microsteps = microsteps;
    }

    void enableOutputs() override
    {
        _outputsEnabled = true;
    }

    void disableOutputs() override
    {
        _outputsEnabled = false;
    }

    // --- test controls / assertions ---

    void setPosition(long steps)
    {
        _current = steps;
        _target  = steps;
    }

    uint32_t moveToCalls() const
    {
        return _moveToCalls;
    }

    uint32_t moveCalls() const
    {
        return _moveCalls;
    }

    uint32_t runCalls() const
    {
        return _runCalls;
    }

    uint32_t runSpeedCalls() const
    {
        return _runSpeedCalls;
    }

    uint32_t runToPositionCalls() const
    {
        return _runToPositionCalls;
    }

    uint32_t runToNewPositionCalls() const
    {
        return _runToNewPositionCalls;
    }

    uint32_t stopCalls() const
    {
        return _stopCalls;
    }

    uint32_t invertCalls() const
    {
        return _invertCalls;
    }

    bool inverted() const
    {
        return _inverted;
    }

    float acceleration() const
    {
        return _acceleration;
    }

    uint16_t microsteps() const
    {
        return _microsteps;
    }

    bool outputsEnabled() const
    {
        return _outputsEnabled;
    }

  private:
    long _current                   = 0;
    long _target                    = 0;
    float _speed                    = 0;
    float _maxSpeed                 = 0;
    float _acceleration             = 0;
    uint16_t _microsteps            = 0;
    bool _inverted                  = false;
    bool _outputsEnabled            = false;
    uint32_t _moveToCalls           = 0;
    uint32_t _moveCalls             = 0;
    uint32_t _runCalls              = 0;
    uint32_t _runSpeedCalls         = 0;
    uint32_t _runToPositionCalls    = 0;
    uint32_t _runToNewPositionCalls = 0;
    uint32_t _stopCalls             = 0;
    uint32_t _invertCalls           = 0;
};

}  // namespace fakes
