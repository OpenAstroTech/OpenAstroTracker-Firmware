#include "adapters/StepperAxis.hpp"

namespace adapters
{

StepperAxis::StepperAxis(hal::IStepperMotor &motor) : _motor(motor)
{
}

ports::AxisSnapshot StepperAxis::snapshot() const
{
    ports::AxisSnapshot state;
    state.currentPosition = _motor.currentPosition();
    state.targetPosition  = _motor.targetPosition();
    state.distanceToGo    = _motor.distanceToGo();
    state.speed           = _motor.speed();
    state.isRunning       = _motor.isRunning();
    return state;
}

long StepperAxis::currentPosition()
{
    return _motor.currentPosition();
}

void StepperAxis::setCurrentPosition(long steps)
{
    _motor.setCurrentPosition(steps);
}

long StepperAxis::targetPosition()
{
    return _motor.targetPosition();
}

long StepperAxis::distanceToGo()
{
    return _motor.distanceToGo();
}

void StepperAxis::move(long relativeSteps)
{
    _motor.move(relativeSteps);
}

void StepperAxis::moveTo(long absoluteSteps)
{
    _motor.moveTo(absoluteSteps);
}

void StepperAxis::runToNewPosition(long absoluteSteps)
{
    _motor.runToNewPosition(absoluteSteps);
}

void StepperAxis::runToPosition()
{
    _motor.runToPosition();
}

void StepperAxis::run()
{
    _motor.run();
}

void StepperAxis::runSpeed()
{
    _motor.runSpeed();
}

void StepperAxis::stop()
{
    _motor.stop();
}

bool StepperAxis::isRunning()
{
    return _motor.isRunning();
}

void StepperAxis::setMaxSpeed(float stepsPerSecond)
{
    _motor.setMaxSpeed(stepsPerSecond);
}

float StepperAxis::maxSpeed()
{
    return _motor.maxSpeed();
}

void StepperAxis::setAcceleration(float stepsPerSecondSquared)
{
    _motor.setAcceleration(stepsPerSecondSquared);
}

void StepperAxis::setSpeed(float stepsPerSecond)
{
    _motor.setSpeed(stepsPerSecond);
}

float StepperAxis::speed()
{
    return _motor.speed();
}

void StepperAxis::setPinsInverted(bool directionInvert, bool stepInvert, bool enableInvert)
{
    _motor.setPinsInverted(directionInvert, stepInvert, enableInvert);
}

}  // namespace adapters
