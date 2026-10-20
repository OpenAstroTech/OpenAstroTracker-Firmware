#pragma once

#include <stdint.h>

namespace ports
{

/// A consistent view of one axis, safe to read from an interrupt.
struct AxisSnapshot {
    long currentPosition;
    long targetPosition;
    long distanceToGo;
    float speed;
    bool isRunning;
};

/// One stepper axis as the domain manipulates it.
///
/// Threading contract (settled here, relied on by the Phase 3 controllers):
/// * `snapshot()` is the **only** method an ISR may call. It returns a copy, so the
///   ISR never observes a half-updated position/target pair.
/// * Every other method is main-loop-only. That holds for all three step-generator
///   models in this firmware: the AVR `InterruptStepper` ISR steps autonomously, the
///   ESP32 `stepperControlTask` drives stepping from core 0, and ESP32/RP2040 call
///   `run()`/`runSpeed()` from the main loop.
/// * Timers are shared, not per-axis: RA slew and tracking share one hardware timer,
///   DEC slew and guide share another, and ALT shares its timer with Focus. Axis
///   ownership is therefore granted by the coordinator, never assumed by an axis.
///
/// RA and DEC each hold two instances (slew + tracking, slew + guide), matching the
/// hardware, because the two roles run different microstep resolutions.
class IStepperAxis
{
  public:
    virtual ~IStepperAxis()
    {
    }

    // --- ISR-safe ---

    virtual AxisSnapshot snapshot() const = 0;

    // --- main-loop only ---

    virtual long currentPosition() = 0;

    virtual void setCurrentPosition(long steps) = 0;

    virtual long targetPosition() = 0;

    virtual long distanceToGo() = 0;

    virtual void move(long relativeSteps) = 0;

    virtual void moveTo(long absoluteSteps) = 0;

    virtual void runToNewPosition(long absoluteSteps) = 0;

    virtual void runToPosition() = 0;

    virtual void run() = 0;

    virtual void runSpeed() = 0;

    virtual void stop() = 0;

    virtual bool isRunning() = 0;

    virtual void setMaxSpeed(float stepsPerSecond) = 0;

    virtual float maxSpeed() = 0;

    virtual void setAcceleration(float stepsPerSecondSquared) = 0;

    virtual void setSpeed(float stepsPerSecond) = 0;

    virtual float speed() = 0;

    virtual void setPinsInverted(bool directionInvert, bool stepInvert, bool enableInvert) = 0;
};

}  // namespace ports
