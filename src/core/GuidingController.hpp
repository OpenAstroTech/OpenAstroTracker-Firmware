#pragma once

#include <stdint.h>

#include "ports/IClock.hpp"

namespace core
{

/// One axis as a guide pulse sees it: the slew-mode steps-per-degree, the two
/// microstep modes involved, and the pulse-speed multiplier from the configuration.
struct GuideAxisGeometry {
    float stepsPerDegree;
    int guideMicrosteps;
    int slewMicrosteps;
    float pulseMultiplier;
};

/// What an RA pulse needs: the sidereal rate, the rate corrected for the mount's
/// calibration factor, and the rate this direction actually asks the axis for.
struct RaPulse {
    float baseSpeed;
    float adjustedSpeed;
    float targetSpeed;
};

/// What a DEC pulse needs. `allowed` is false when the axis already sits on the limit
/// the pulse would push it past, in which case no speed is applied.
struct DecPulse {
    bool allowed;
    float baseSpeed;
    float targetSpeed;
};

/// Guide-pulse policy for the two axes that can guide: how fast each moves for a
/// pulse, whether DEC may move at all, and when the pulse has run its course.
///
/// The pulse rates are derived from the mount's geometry and the sidereal rate, so
/// this class holds no hardware and no configuration: the caller supplies both. It
/// owns only the two deadlines, because a pulse ends when its time is up rather than
/// when a motor stops.
///
/// Threading: main-loop-only.
class GuidingController
{
  public:
    explicit GuidingController(ports::IClock *clock) : _clock(clock)
    {
    }

    /// Speed at the sidereal rate, before calibration and before the pulse multiplier.
    /// The microstep ratio is a floating-point division here, unlike the tracking rate's
    /// integer one, because that is what the firmware has always done.
    ///
    /// RA moves at 2.5x sidereal westward and 0.5x eastward (by default), and the whole
    /// rate is scaled by the tracking calibration so guiding shares the mount's measured
    /// tracking error.
    static RaPulse raPulse(bool west, const GuideAxisGeometry &geometry, float siderealDegreesPerHour, float calibration)
    {
        RaPulse pulse;
        pulse.baseSpeed     = baseSpeed(geometry, siderealDegreesPerHour);
        pulse.adjustedSpeed = pulse.baseSpeed * calibration;
        pulse.targetSpeed = west ? geometry.pulseMultiplier * pulse.adjustedSpeed : pulse.adjustedSpeed * (2.0f - geometry.pulseMultiplier);
        return pulse;
    }

    /// DEC moves at the sidereal rate in both directions, and refuses to move further
    /// into a configured limit. A limit of 0 means "no limit".
    static DecPulse decPulse(
        bool north, const GuideAxisGeometry &geometry, float siderealDegreesPerHour, long decPosition, long lowerLimit, long upperLimit)
    {
        DecPulse pulse;
        pulse.baseSpeed = baseSpeed(geometry, siderealDegreesPerHour);
        pulse.allowed   = north ? !((upperLimit != 0) && (decPosition >= upperLimit)) : !((lowerLimit != 0) && (decPosition <= lowerLimit));
        pulse.targetSpeed = north ? geometry.pulseMultiplier * pulse.baseSpeed : -geometry.pulseMultiplier * pulse.baseSpeed;
        return pulse;
    }

    // --- pulse timing ---

    void startRa(int durationMilliseconds)
    {
        _raDeadline = _clock->now() + (uint32_t) durationMilliseconds;
    }

    void startDec(int durationMilliseconds)
    {
        _decDeadline = _clock->now() + (uint32_t) durationMilliseconds;
    }

    void clearRa()
    {
        _raDeadline = 0;
    }

    void clearDec()
    {
        _decDeadline = 0;
    }

    /// True once an armed pulse has run out. An unarmed pulse never expires, which is
    /// what keeps a stale deadline from stopping a pulse that has not started.
    bool raExpired() const
    {
        return expired(_raDeadline);
    }

    bool decExpired() const
    {
        return expired(_decDeadline);
    }

    uint32_t raDeadline() const
    {
        return _raDeadline;
    }

    uint32_t decDeadline() const
    {
        return _decDeadline;
    }

  private:
    static float baseSpeed(const GuideAxisGeometry &geometry, float siderealDegreesPerHour)
    {
        // u-steps/deg * deg/hr / sec/hr = u-steps/sec
        return geometry.stepsPerDegree * (1.0 * geometry.guideMicrosteps / geometry.slewMicrosteps) * siderealDegreesPerHour / 3600.0f;
    }

    bool expired(uint32_t deadline) const
    {
        // Wrap-safe: a deadline is at most one pulse length away.
        return (deadline != 0) && ((int32_t) (_clock->now() - deadline) > 0);
    }

    ports::IClock *_clock;
    uint32_t _raDeadline  = 0;
    uint32_t _decDeadline = 0;
};

}  // namespace core
