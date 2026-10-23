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
/// * Every other method is main-loop-only.
///
/// *How* steps get generated is the backend's business and never a domain concern: an
/// MCU timer interrupt (AVR today), a FreeRTOS task (ESP32), a polled library called
/// from the main loop (RP2040), or a driver that generates steps from its own clock and
/// needs no timer at all (a TMC2209 in `VACTUAL` mode does; the command is `x*` and the
/// axis then moves without the MCU pulsing anything). A backend that owns a shared
/// hardware timer serialises its own users inside `hal/`; nothing above `hal/` may
/// assume, require or work around a timer, and no controller is shaped by one.
///
/// RA and DEC each hold two instances (slew + tracking, slew + guide) because the two
/// roles drive **one physical actuator** at different microstep resolutions — that
/// mutual exclusion is a property of the machine, not of the timer, and it is what the
/// coordinator arbitrates.
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
