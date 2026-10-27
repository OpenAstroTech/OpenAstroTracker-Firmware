#pragma once

#include <stdint.h>

namespace ports
{

/// A consistent view of one axis, safe to read from an interrupt.
///
/// One instant, not five separate reads: the fields are copied together so an ISR cannot see a
/// position that belongs to a different target than the one it is paired with.
struct AxisSnapshot {
    /// Where the axis is, in the backend's microsteps.
    long currentPosition;
    /// Where the last move asked it to go.
    long targetPosition;
    /// Steps still to travel.
    long distanceToGo;
    /// Speed the backend last reported, in microsteps per second.
    float speed;
    /// True while the backend is generating steps.
    bool isRunning;
};

/// One stepper axis as the domain manipulates it.
///
/// Threading contract:
/// * `snapshot()` is the **only** method an ISR may call. It returns a copy, so the
///   ISR never observes a half-updated position/target pair.
/// * Every other method is main-loop-only.
///
/// *How* steps get generated is the backend's business and never a domain concern: an
/// MCU timer interrupt (AVR), a FreeRTOS task (ESP32), a polled library called
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

    /// The whole motion state in one read. The only method an interrupt may call.
    virtual AxisSnapshot snapshot() const = 0;

    // --- main-loop only ---

    /// The axis' position, in the backend's microsteps.
    virtual long currentPosition() = 0;

    /// Re-declares the current position without turning the motor: calibration and sync
    /// bookkeeping only.
    virtual void setCurrentPosition(long steps) = 0;

    /// Where the last `move()`/`moveTo()` asked the axis to go.
    virtual long targetPosition() = 0;

    /// Steps still to travel: `targetPosition() - currentPosition()`.
    virtual long distanceToGo() = 0;

    /// Requests a relative move. The steps happen later, as the backend is pumped.
    virtual void move(long relativeSteps) = 0;

    /// Requests an absolute move. The steps happen later, as the backend is pumped.
    virtual void moveTo(long absoluteSteps) = 0;

    /// Moves to `absoluteSteps` and waits until the axis arrives. For the places that need the
    /// move finished before they continue, e.g. tracking compensation.
    virtual void runToNewPosition(long absoluteSteps) = 0;

    /// Completes the pending move, waiting until the target is reached.
    virtual void runToPosition() = 0;

    /// Advances at most one step when the acceleration profile is due. A no-op where the backend
    /// steps itself (the AVR timer ISR).
    virtual void run() = 0;

    /// Advances at most one step at the constant speed set by `setSpeed()`. A no-op where the
    /// backend steps itself.
    virtual void runSpeed() = 0;

    /// Stops the axis as soon as the backend can; the position is wherever it stopped.
    virtual void stop() = 0;

    /// True while the backend is generating steps for this axis.
    virtual bool isRunning() = 0;

    /// The speed ceiling for accelerated moves, in microsteps per second.
    virtual void setMaxSpeed(float stepsPerSecond) = 0;

    /// The configured speed ceiling, in microsteps per second.
    virtual float maxSpeed() = 0;

    /// The acceleration used by `move()`/`moveTo()`, in microsteps per second squared.
    virtual void setAcceleration(float stepsPerSecondSquared) = 0;

    /// The constant speed used by `runSpeed()`. A negative speed reverses direction.
    virtual void setSpeed(float stepsPerSecond) = 0;

    /// The speed the backend last reported, in microsteps per second.
    virtual float speed() = 0;

    /// Flips the direction/step/enable pin polarity for wiring that needs it. Applied at setup.
    virtual void setPinsInverted(bool directionInvert, bool stepInvert, bool enableInvert) = 0;
};

}  // namespace ports
