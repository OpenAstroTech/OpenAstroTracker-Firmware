#pragma once

#include <stdint.h>

#include "ports/IStepperAxis.hpp"

namespace core
{

/// Sidereal tracking for the RA axis.
///
/// Owns the tracking stepper and the two numbers behind it: the dimensionless
/// calibration factor (persisted by the mount's settings) and the resulting step rate.
/// It also remembers when the tracker was stopped, so the mount can advance it by the
/// distance it would have covered while a slew held it still.
///
/// The axis is held through `ports::IStepperAxis` on every platform. It used to be a
/// template parameter so the AVR build could keep its concrete interrupt-stepper engine;
/// once every platform started building engines behind `hal::IStepperMotor`, the
/// parameter had exactly one instantiation and was removed. How steps are generated is
/// `hal/`'s business (timer ISR, task, polled engine, or a driver that generates them
/// itself) and must not reach this class.
///
/// Threading: every method is main-loop-only. The step generator reads the axis, not
/// this controller.
class TrackingController
{
  public:
    /// `microstepRatio` is tracking microsteps per slew microstep, computed with integer
    /// division exactly as the original inline expression did (256/8 = 32, or 1 when the
    /// two modes are the same).
    TrackingController(float siderealSecondsPerDay, unsigned microstepRatio)
        : _siderealSecondsPerDay(siderealSecondsPerDay), _microstepRatio(microstepRatio)
    {
    }

    /// The axis is created by the mount's `configure*` step, after the controller.
    void attach(ports::IStepperAxis *axis)
    {
        _axis = axis;
    }

    /// Forgets the axis and any pending compensation. Used when the mount drops its
    /// configuration and rebuilds the axes.
    void detach()
    {
        _axis                = nullptr;
        _stoppedAt           = 0;
        _compensationPending = false;
    }

    float calibration() const
    {
        return _calibration;
    }

    /// The dimensionless factor, very close to 1.0. Call `setStepsPerDegree()` afterwards
    /// to turn it into a step rate.
    void setCalibration(float calibration)
    {
        _calibration = calibration;
    }

    /// RA microsteps per second while tracking.
    float speed() const
    {
        return _speed;
    }

    /// Recomputes the tracking rate. The earth rotates 360 degrees per sidereal day, so
    /// the calibration factor is (sidereal day / 24 h) times the mechanical factor.
    /// `stepsPerDegree` is the slew-mode value, hence the microstep ratio.
    void setStepsPerDegree(float stepsPerDegree)
    {
        _stepsPerDegree = stepsPerDegree;

        // (fraction of day) * u-steps/deg * (u-steps/u-steps) * deg / (sec/day) = u-steps / sec
        _speed = _calibration * _stepsPerDegree * _microstepRatio * 360.0f / _siderealSecondsPerDay;
    }

    /// Hands the current tracking rate to the axis. Used both to start tracking and to
    /// retune it while it is running.
    void applySpeed()
    {
        _axis->setSpeed(_speed);
    }

    void stop()
    {
        _axis->stop();
    }

    // --- tracker-stopped compensation ---

    /// Records that the tracker was stopped at `now`, so a later `compensationSteps()`
    /// can work out how far it fell behind.
    void noteStopped(uint32_t now)
    {
        _stoppedAt           = now;
        _compensationPending = true;
    }

    bool compensationPending() const
    {
        return _compensationPending;
    }

    uint32_t stoppedAt() const
    {
        return _stoppedAt;
    }

    /// Steps the tracker must advance to make up for being stopped since `stoppedAt()`.
    /// Pure: `applyCompensation()` performs the move.
    long compensationSteps(uint32_t now) const
    {
        const uint32_t elapsed = now - _stoppedAt;
        return (long) (_speed * (float) elapsed / 1000.0f);
    }

    /// Moves the tracking axis by `steps`, using goto mode (`runToNewPosition`) because
    /// compensation happens while the axis is not tracking at a fixed speed.
    void applyCompensation(long steps)
    {
        _axis->runToNewPosition(_axis->currentPosition() + steps);
        _compensationPending = false;
    }

    // --- read model ---

    long currentPosition() const
    {
        return _axis->currentPosition();
    }

    /// Hours of tracking the axis has accumulated. Zero while the rate is unknown.
    float trackedHours() const
    {
        return (_speed > 0) ? ((_axis->currentPosition() / _speed) / 3600.0F) : 0.0F;
    }

  private:
    const float _siderealSecondsPerDay;
    const unsigned _microstepRatio;
    ports::IStepperAxis *_axis = nullptr;
    float _stepsPerDegree      = 0.0f;
    float _speed               = 0.0f;
    float _calibration         = 0.0f;
    uint32_t _stoppedAt        = 0;
    bool _compensationPending  = false;
};

}  // namespace core
