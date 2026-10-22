#pragma once

#include <stdint.h>

#include "hal/IStepperMotor.hpp"

namespace hal
{

/// `IStepperMotor` over one of the vendored AVR interrupt-stepper engines.
///
/// The engines are compile-time specialised per axis (`StepperAzSlew`, `StepperFocusSlew`,
/// …): each carries its own pins, timer and acceleration ramp as template parameters.
/// They already provide almost exactly this interface, so the adapter is a translation
/// plus three members the engines do not have:
///
/// * `setMicrosteps()` is a no-op — AVR boards set microstepping on the driver with pin
///   strapping, which is why only the UART driver path ever changes it.
/// * `enableOutputs()`/`disableOutputs()` are no-ops for the same reason; the enable pin
///   is driven through `hal::IGpioPin` where the board has one.
/// * `distanceToGo()` narrows the engine's `uint32_t` to the interface's `long`.
///
/// Why this exists: PLAN.md's Phase 2b deliberately left the AVR engines out of the HAL,
/// because a vtable per engine costs SRAM and the concrete engines inline better. Phase 4
/// needs the optional axes to be runtime-present — a board with no AZ motor must hold a
/// null pointer rather than compile the axis out — so those engines are wrapped. On AVR
/// the step generator is the engine's own ISR, not `Mount::interruptLoop()` (which is
/// compiled out for `NEW_STEPPER_LIB`), so this adds no virtual call to the stepping path:
/// it is main-loop only.
///
/// Measured cost on `ramps`, widest configuration: ~8 KB of flash for the three optional
/// axes, which is why only they are wrapped; RA, DEC, TRK and GUIDE are always present and
/// keep their concrete engines.
template <typename Engine> class InterruptStepperMotor final : public IStepperMotor
{
  public:
    explicit InterruptStepperMotor(Engine &engine) : _engine(engine)
    {
    }

    void moveTo(long absoluteSteps) override
    {
        _engine.moveTo(absoluteSteps);
    }

    void move(long relativeSteps) override
    {
        _engine.move(relativeSteps);
    }

    void setMaxSpeed(float stepsPerSecond) override
    {
        _engine.setMaxSpeed(stepsPerSecond);
    }

    void setAcceleration(float stepsPerSecondSquared) override
    {
        _engine.setAcceleration(stepsPerSecondSquared);
    }

    void setSpeed(float stepsPerSecond) override
    {
        _engine.setSpeed(stepsPerSecond);
    }

    void setPinsInverted(bool directionInvert, bool stepInvert, bool enableInvert) override
    {
        _engine.setPinsInverted(directionInvert, stepInvert, enableInvert);
    }

    void run() override
    {
        _engine.run();
    }

    void runSpeed() override
    {
        _engine.runSpeed();
    }

    void runToPosition() override
    {
        _engine.runToPosition();
    }

    void runToNewPosition(long absoluteSteps) override
    {
        _engine.runToNewPosition(absoluteSteps);
    }

    void stop() override
    {
        _engine.stop();
    }

    bool isRunning() override
    {
        return _engine.isRunning();
    }

    long currentPosition() override
    {
        return static_cast<long>(_engine.currentPosition());
    }

    void setCurrentPosition(long steps) override
    {
        _engine.setCurrentPosition(steps);
    }

    long targetPosition() override
    {
        return static_cast<long>(_engine.targetPosition());
    }

    long distanceToGo() override
    {
        return static_cast<long>(_engine.distanceToGo());
    }

    float speed() override
    {
        return _engine.speed();
    }

    float maxSpeed() override
    {
        return _engine.maxSpeed();
    }

    void setMicrosteps(uint16_t microsteps) override
    {
        (void) microsteps;  // fixed by pin strapping on these boards
    }

    void enableOutputs() override
    {
    }

    void disableOutputs() override
    {
    }

  private:
    Engine &_engine;
};

}  // namespace hal
