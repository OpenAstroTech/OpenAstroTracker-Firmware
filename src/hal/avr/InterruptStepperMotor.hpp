#pragma once

#include "hal/IStepperMotor.hpp"

#if defined(NEW_STEPPER_LIB) && defined(ARDUINO_AVR_ATmega2560)

    #include "../../InterruptAccelStepper.h"
    #include "../../StepperConfiguration.hpp"

namespace hal
{

/// `IStepperMotor` over the vendored `InterruptAccelStepper`, which steps from a
/// hardware timer ISR (Timer3 for RA, Timer4 for DEC, Timer1 for AZ, Timer5 for ALT
/// and Focus) without any main-loop involvement.
///
/// The template parameter is one of the `config::<Axis>::stepper_*` types, so the
/// pins, timer and acceleration ramp are all compile-time constants — the same
/// arrangement `Mount` uses today. `run()`/`runSpeed()` are stubs in that engine, and
/// `setMicrosteps()` is a no-op because microstepping is a driver setting.
///
/// The engine is instantiated by value: on AVR it holds no dynamic state, so the
/// wrapper can own it instead of pointing at a heap instance.
template <typename StepperType> class InterruptStepperMotor final : public IStepperMotor
{
  public:
    InterruptStepperMotor() : _stepper(0, 0, 0)
    {
    }

    void moveTo(long absoluteSteps) override
    {
        _stepper.moveTo(absoluteSteps);
    }

    void move(long relativeSteps) override
    {
        _stepper.move(relativeSteps);
    }

    void setMaxSpeed(float stepsPerSecond) override
    {
        _stepper.setMaxSpeed(stepsPerSecond);
    }

    void setAcceleration(float stepsPerSecondSquared) override
    {
        _stepper.setAcceleration(stepsPerSecondSquared);
    }

    void setSpeed(float stepsPerSecond) override
    {
        _stepper.setSpeed(stepsPerSecond);
    }

    void setPinsInverted(bool directionInvert, bool stepInvert, bool enableInvert) override
    {
        _stepper.setPinsInverted(directionInvert, stepInvert, enableInvert);
    }

    void run() override
    {
        _stepper.run();
    }

    void runSpeed() override
    {
        _stepper.runSpeed();
    }

    void runToPosition() override
    {
        _stepper.runToPosition();
    }

    void runToNewPosition(long absoluteSteps) override
    {
        _stepper.runToNewPosition(absoluteSteps);
    }

    void stop() override
    {
        _stepper.stop();
    }

    bool isRunning() override
    {
        return _stepper.isRunning();
    }

    long currentPosition() override
    {
        return _stepper.currentPosition();
    }

    void setCurrentPosition(long steps) override
    {
        _stepper.setCurrentPosition(steps);
    }

    long targetPosition() override
    {
        return _stepper.targetPosition();
    }

    long distanceToGo() override
    {
        // The AVR engine reports an unsigned distance; the interface keeps `long` so
        // the same code works against AccelStepper.
        return static_cast<long>(_stepper.distanceToGo());
    }

    float speed() override
    {
        return _stepper.speed();
    }

    float maxSpeed() override
    {
        return _stepper.maxSpeed();
    }

    void setMicrosteps(uint16_t microsteps) override
    {
        (void)microsteps;  // microstepping lives in the driver, not in the engine
    }

    void enableOutputs() override
    {
    }

    void disableOutputs() override
    {
    }

  private:
    StepperType _stepper;
};

}  // namespace hal

#endif  // NEW_STEPPER_LIB && ARDUINO_AVR_ATmega2560
