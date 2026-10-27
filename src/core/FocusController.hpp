#pragma once

#include <stdint.h>

#include "ports/IStepperAxis.hpp"
#include "types/FocuserDirection.hpp"

namespace core
{

/// What the focuser is doing. `Continuous` is kept because the step generator still has
/// a branch for it, but nothing selects it today: continuous motion runs through
/// `runAtSpeed()`, which targets a far position instead of a fixed speed.
enum class FocusMode : uint8_t
{
    Idle,
    ToTarget,
    Continuous,
};

/// Result of one `update()` tick.
enum class FocusUpdate : uint8_t
{
    None,
    Stopped,
};

/// The focus axis: which of the four speeds is selected, whether it is moving to a
/// target or running continuously, and the one transition the main loop cares about
/// (running -> stopped, so the driver can be powered down).
///
/// The controller does not know about the driver-enable pin, the microsteps or the
/// settings: the caller enables the motor before a move and disables it when `update()`
/// reports `Stopped`, exactly as `Mount` did inline.
///
/// The axis is held through `ports::IStepperAxis`, as in `TrackingController`, and the
/// template parameter it used to carry is gone for the same reason: step generation is a
/// `hal/` concern and no longer selects a type here.
///
/// Threading: main-loop-only for every method except `isRunning()`/`position()`, which
/// only read the axis.
class FocusController
{
  public:
    /// The axis is created by the mount's configure step, after the controller.
    void attach(ports::IStepperAxis *axis)
    {
        _axis = axis;
    }

    void detach()
    {
        _axis       = nullptr;
        _mode       = FocusMode::Idle;
        _wasRunning = false;
    }

    /// True when this build has a focus axis. A board without one keeps the controller
    /// detached, which is why every method below tolerates a null axis instead of treating
    /// the attachment as a precondition.
    bool attached() const
    {
        return _axis != nullptr;
    }

    // --- read model ---

    long position() const
    {
        return attached() ? _axis->currentPosition() : 0;
    }

    void setPosition(long steps)
    {
        if (!attached())
        {
            return;
        }
        _axis->setCurrentPosition(steps);
    }

    float speed() const
    {
        return attached() ? _axis->speed() : 0.0F;
    }

    bool isRunning() const
    {
        return attached() && _axis->isRunning();
    }

    FocusMode mode() const
    {
        return _mode;
    }

    int rate() const
    {
        return _rate;
    }

    /// The fraction of the axis' maximum speed that the selected rate asks for.
    float rateFactor() const
    {
        return factorFor(_rate);
    }

    float maxRateSpeed() const
    {
        return _maxRateSpeed;
    }

    // --- configuration ---

    /// Selects one of four speeds (1 = slowest, 4 = fastest). `maxSpeed` is the axis'
    /// configured maximum, which the caller keeps in its own configuration.
    void setRate(int rate, float maxSpeed)
    {
        _rate         = clampRate(rate);
        _maxRateSpeed = factorFor(_rate) * maxSpeed;
        if (attached())
        {
            _axis->setMaxSpeed(_maxRateSpeed);
        }
    }

    /// The configured maximum is the fastest rate, matching what the mount used to
    /// leave behind when it configured the axis.
    void initialize(float maxSpeed)
    {
        _rate         = kMaxRate;
        _maxRateSpeed = maxSpeed;
    }

    // --- motion ---

    /// Moves by a relative number of steps. The caller has enabled the driver.
    void moveBy(long steps)
    {
        if (!attached())
        {
            return;
        }
        _axis->moveTo(_axis->currentPosition() + steps);
        _mode = FocusMode::ToTarget;
    }

    /// Runs continuously at the selected rate. The caller has enabled the driver.
    void startContinuous(types::FocuserDirection direction)
    {
        runAtSpeed((float) direction * _maxRateSpeed);
    }

    /// A signed speed picks the direction; the axis then targets a far position so it
    /// keeps moving until something stops it. A speed of zero stops the axis, which is
    /// what the legacy `setSpeed(FOCUS_STEPS, 0)` did.
    void runAtSpeed(float signedSpeed)
    {
        if (!attached())
        {
            return;
        }
        if (signedSpeed != 0)
        {
            _axis->setMaxSpeed(signedSpeed);
            _axis->moveTo((signedSpeed > 0) ? kContinuousTarget : -kContinuousTarget);
            _mode = FocusMode::ToTarget;
        }
        else
        {
            _axis->stop();
        }
    }

    void stop()
    {
        if (attached())
        {
            _axis->stop();
        }
    }

    /// Called once per main-loop tick. Reports the single transition from "seen running"
    /// to "stopped", which is the only moment the old inline code powered the driver
    /// down; a move that was cancelled before it ever ran does not count, matching the
    /// previous `_focuserWasRunning` flag.
    FocusUpdate update()
    {
        if (!attached())
        {
            _wasRunning = false;
            _mode       = FocusMode::Idle;
            return FocusUpdate::None;
        }

        if (_axis->isRunning())
        {
            _wasRunning = true;
            return FocusUpdate::None;
        }

        if (!_wasRunning)
        {
            return FocusUpdate::None;
        }

        _mode       = FocusMode::Idle;
        _wasRunning = false;
        return FocusUpdate::Stopped;
    }

  private:
    static const int kMaxRate = 4;

    /// How far a continuous run aims: far enough that it keeps going until it is stopped.
    static constexpr long kContinuousTarget = 300000L;

    static int clampRate(int rate)
    {
        if (rate < 1)
        {
            return 1;
        }
        return (rate > kMaxRate) ? kMaxRate : rate;
    }

    /// Rate 1..4 as a fraction of the axis' maximum speed. The chain is deliberate: a
    /// table here would be a `static` in a template, i.e. a copy per instantiation in
    /// the ATmega2560's SRAM.
    static float factorFor(int rate)
    {
        switch (clampRate(rate))
        {
            case 1:
                return 0.05f;
            case 2:
                return 0.2f;
            case 3:
                return 0.5f;
            default:
                return 1.0f;
        }
    }

    ports::IStepperAxis *_axis = nullptr;
    FocusMode _mode            = FocusMode::Idle;
    float _maxRateSpeed        = 0.0f;
    int8_t _rate               = kMaxRate;
    bool _wasRunning           = false;
};

}  // namespace core
