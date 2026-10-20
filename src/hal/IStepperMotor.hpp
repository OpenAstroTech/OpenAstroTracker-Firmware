#pragma once

#include <stdint.h>

namespace hal
{

/// The motion engine of one stepper motor: position bookkeeping, speed/acceleration
/// limits and step generation. This is *not* a bare step/dir pulse interface and it
/// is *not* `IStepperAxis`: it mirrors what the stepper libraries (AccelStepper on
/// ESP32/RP2040, InterruptStepper on AVR) already offer, so a backend is a thin
/// translation and never has to re-implement motion ramps.
///
/// The units are the libraries': steps and steps per second. Which axis a motor
/// belongs to, and what a step means in degrees, is decided above this layer.
class IStepperMotor
{
  public:
    virtual ~IStepperMotor()
    {
    }

    // --- motion requests ---------------------------------------------------

    virtual void moveTo(long absoluteSteps) = 0;

    virtual void move(long relativeSteps) = 0;

    virtual void setMaxSpeed(float stepsPerSecond) = 0;

    virtual void setAcceleration(float stepsPerSecondSquared) = 0;

    /// Constant-speed request. Negative speeds reverse direction.
    virtual void setSpeed(float stepsPerSecond) = 0;

    virtual void setPinsInverted(bool directionInvert, bool stepInvert, bool enableInvert) = 0;

    // --- stepping ----------------------------------------------------------

    /// Advances one step when the acceleration profile is due (main-loop or task driven).
    virtual void run() = 0;

    /// Advances one step at the configured constant speed.
    virtual void runSpeed() = 0;

    /// Runs until the target is reached.
    virtual void runToPosition() = 0;

    /// Moves to `absoluteSteps`, then runs until the target is reached.
    virtual void runToNewPosition(long absoluteSteps) = 0;

    virtual void stop() = 0;

    // --- state -------------------------------------------------------------

    virtual bool isRunning() = 0;

    virtual long currentPosition() = 0;

    virtual void setCurrentPosition(long steps) = 0;

    virtual long targetPosition() = 0;

    virtual long distanceToGo() = 0;

    virtual float speed() = 0;

    virtual float maxSpeed() = 0;

    // --- electrical configuration -----------------------------------------

    /// Microstep resolution reported to the engine. Engines whose microstepping is
    /// fixed in the driver (AVR InterruptStepper) ignore this.
    virtual void setMicrosteps(uint16_t microsteps) = 0;

    virtual void enableOutputs() = 0;

    virtual void disableOutputs() = 0;
};

}  // namespace hal
