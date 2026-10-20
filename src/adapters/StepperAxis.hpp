#pragma once

#include "hal/IStepperMotor.hpp"
#include "ports/IStepperAxis.hpp"

namespace adapters
{

/// `ports::IStepperAxis` over a `hal::IStepperMotor`.
///
/// Pure C++, so it is host-tested against a faked engine. The platform differences
/// live one layer down, not here: on AVR the interrupt engine steps by itself and
/// `run()`/`runSpeed()` are stubs, while on ESP32/RP2040 the main loop (or the
/// stepping task) has to call them. This adapter only forwards, so there is
/// deliberately no `AccelStepperAxis` and no `InterruptAccelStepperAxis`.
class StepperAxis final : public ports::IStepperAxis
{
  public:
    explicit StepperAxis(hal::IStepperMotor &motor);

    ports::AxisSnapshot snapshot() const override;

    long currentPosition() override;

    void setCurrentPosition(long steps) override;

    long targetPosition() override;

    long distanceToGo() override;

    void move(long relativeSteps) override;

    void moveTo(long absoluteSteps) override;

    void runToNewPosition(long absoluteSteps) override;

    void runToPosition() override;

    void run() override;

    void runSpeed() override;

    void stop() override;

    bool isRunning() override;

    void setMaxSpeed(float stepsPerSecond) override;

    float maxSpeed() override;

    void setAcceleration(float stepsPerSecondSquared) override;

    void setSpeed(float stepsPerSecond) override;

    float speed() override;

    void setPinsInverted(bool directionInvert, bool stepInvert, bool enableInvert) override;

  private:
    hal::IStepperMotor &_motor;
};

}  // namespace adapters
