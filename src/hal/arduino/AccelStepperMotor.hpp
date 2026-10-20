#pragma once

#include <stdint.h>

#include "hal/WarningGuard.hpp"

PUSH_NO_WARNINGS
#include "AccelStepper.h"
POP_NO_WARNINGS

#include "hal/IStepperMotor.hpp"

namespace hal
{

/// `IStepperMotor` over an `AccelStepper` instance the caller owns.
///
/// Used on ESP32 and RP2040, where the main loop (ESP32: the stepping task on core 0)
/// drives `run()`/`runSpeed()`. Microstepping is configured on the motor driver, not
/// on the engine, so `setMicrosteps()` is a no-op here — exactly as it is for
/// `InterruptStepperMotor` on AVR.
class AccelStepperMotor final : public IStepperMotor
{
  public:
    explicit AccelStepperMotor(AccelStepper &motor);

    void moveTo(long absoluteSteps) override;

    void move(long relativeSteps) override;

    void setMaxSpeed(float stepsPerSecond) override;

    void setAcceleration(float stepsPerSecondSquared) override;

    void setSpeed(float stepsPerSecond) override;

    void setPinsInverted(bool directionInvert, bool stepInvert, bool enableInvert) override;

    void run() override;

    void runSpeed() override;

    void runToPosition() override;

    void runToNewPosition(long absoluteSteps) override;

    void stop() override;

    bool isRunning() override;

    long currentPosition() override;

    void setCurrentPosition(long steps) override;

    long targetPosition() override;

    long distanceToGo() override;

    float speed() override;

    float maxSpeed() override;

    void setMicrosteps(uint16_t microsteps) override;

    void enableOutputs() override;

    void disableOutputs() override;

  private:
    AccelStepper &_motor;
};

}  // namespace hal
