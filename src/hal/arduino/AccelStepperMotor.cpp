#include "hal/arduino/AccelStepperMotor.hpp"

namespace hal
{

AccelStepperMotor::AccelStepperMotor(AccelStepper &motor) : _motor(motor)
{
}

void AccelStepperMotor::moveTo(long absoluteSteps)
{
    _motor.moveTo(absoluteSteps);
}

void AccelStepperMotor::move(long relativeSteps)
{
    _motor.move(relativeSteps);
}

void AccelStepperMotor::setMaxSpeed(float stepsPerSecond)
{
    _motor.setMaxSpeed(stepsPerSecond);
}

void AccelStepperMotor::setAcceleration(float stepsPerSecondSquared)
{
    _motor.setAcceleration(stepsPerSecondSquared);
}

void AccelStepperMotor::setSpeed(float stepsPerSecond)
{
    _motor.setSpeed(stepsPerSecond);
}

void AccelStepperMotor::setPinsInverted(bool directionInvert, bool stepInvert, bool enableInvert)
{
    _motor.setPinsInverted(directionInvert, stepInvert, enableInvert);
}

void AccelStepperMotor::run()
{
    _motor.run();
}

void AccelStepperMotor::runSpeed()
{
    _motor.runSpeed();
}

void AccelStepperMotor::runToPosition()
{
    _motor.runToPosition();
}

void AccelStepperMotor::runToNewPosition(long absoluteSteps)
{
    _motor.runToNewPosition(absoluteSteps);
}

void AccelStepperMotor::stop()
{
    _motor.stop();
}

bool AccelStepperMotor::isRunning()
{
    return _motor.isRunning();
}

long AccelStepperMotor::currentPosition()
{
    return _motor.currentPosition();
}

void AccelStepperMotor::setCurrentPosition(long steps)
{
    _motor.setCurrentPosition(steps);
}

long AccelStepperMotor::targetPosition()
{
    return _motor.targetPosition();
}

long AccelStepperMotor::distanceToGo()
{
    return _motor.distanceToGo();
}

float AccelStepperMotor::speed()
{
    return _motor.speed();
}

float AccelStepperMotor::maxSpeed()
{
    return _motor.maxSpeed();
}

void AccelStepperMotor::setMicrosteps(uint16_t microsteps)
{
    (void) microsteps;  // microstepping lives on the driver, not on the motion engine
}

void AccelStepperMotor::enableOutputs()
{
    _motor.enableOutputs();
}

void AccelStepperMotor::disableOutputs()
{
    _motor.disableOutputs();
}

}  // namespace hal
