#pragma once

#include <stdint.h>

namespace core
{

/// Where the drift-alignment sequence is.
enum class DriftPhase : uint8_t
{
    Idle,
    Stopping,  ///< waiting for the axes to come to rest before the first pass
    PauseEast,
    PassEast,  ///< the long eastward move; the axis is being stepped
    GapEast,   ///< the 20-step take-up that removes the gearing gap after the east pass
    PauseWest,
    PassWest,
    PauseReset,
    GapReset,  ///< the 20-step move back, so the gearing ends where it started
    ResumeTracking,
};

/// What the controller wants the mount to do next. The mount owns every side effect.
enum class DriftAction : uint8_t
{
    None,
    StopAxes,      ///< stop everything, then wait for `axesStopped`
    MoveEastPass,  ///< move +passSteps at `passSpeed`
    MoveEastGap,   ///< take up the gearing gap: -20 steps at the slow speed
    MoveWestPass,  ///< move -passSteps at `passSpeed`
    ResetGap,      ///< +20 steps at the slow speed
    ResumeTracking,
};

/// The drift-alignment sequence as a phase machine.
///
/// The command handler used to run this inline and block: stop, `delay(1500)`, a synchronous east
/// pass, `delay(1500)`, a synchronous west pass, `delay(1500)`, the reset move — with the pass
/// `duration` supplied by the host. At the default rate that is **90 seconds during which the
/// mount answers nothing**, because the main loop is inside the handler. Here the sequence is a
/// state, `Mount::loop()` ticks it, and every wait is either a deadline on the clock or "the axis
/// stopped", which the loop already knows.
///
/// Faithful to the original geometry: a 5.3 arcminute pass covered in `durationSeconds` (so the
/// speed is derived, not configured), a −20 step take-up after the east pass only, and a +20 step
/// reset at the end so the gearing ends where it started.
///
/// Threading: main-loop-only.
class DriftAlignmentController
{
  public:
    /// How far the axis moves per pass, and how much of the gearing backlash it takes up.
    static constexpr float kPassArcMinutes   = 5.3F;
    static constexpr long kGearingGapSteps   = 20;
    static constexpr uint32_t kPauseMillis   = 1500;
    static constexpr float kGapSpeedStepsSec = 300.0F;

    void begin(int durationSeconds, float stepsPerDegree)
    {
        _durationSeconds = (durationSeconds > 0) ? static_cast<uint32_t>(durationSeconds) : 1;
        _passSteps       = static_cast<long>(stepsPerDegree * (kPassArcMinutes / 60.0F));
        _passSpeed       = static_cast<float>(_passSteps) / static_cast<float>(_durationSeconds);
        _phase           = DriftPhase::Stopping;
        _deadline        = 0;
        _seenMoving      = false;
    }

    void abort()
    {
        _phase = DriftPhase::Idle;
    }

    bool isActive() const
    {
        return _phase != DriftPhase::Idle;
    }

    DriftPhase phase() const
    {
        return _phase;
    }

    long passSteps() const
    {
        return _passSteps;
    }

    float passSpeed() const
    {
        return _passSpeed;
    }

    /// Advances the sequence and returns the one action to perform now.
    ///
    /// `axesStopped` is the caller's "nothing is moving" answer for this tick. The pass phases
    /// only accept a stop **after** they have seen the axis move, so issuing a move and asking in
    /// the same tick cannot skip the pass.
    DriftAction update(bool axesStopped, uint32_t nowMs)
    {
        switch (_phase)
        {
            case DriftPhase::Idle:
                return DriftAction::None;

            case DriftPhase::Stopping:
                if (!axesStopped)
                {
                    return DriftAction::None;
                }
                _phase    = DriftPhase::PauseEast;
                _deadline = nowMs + kPauseMillis;
                return DriftAction::None;

            case DriftPhase::PauseEast:
                if (elapsed(nowMs, _deadline))
                {
                    _phase      = DriftPhase::PassEast;
                    _seenMoving = false;
                    return DriftAction::MoveEastPass;
                }
                return DriftAction::None;

            case DriftPhase::PassEast:
                if (advanceWhenStopped(axesStopped, DriftPhase::GapEast))
                {
                    return DriftAction::MoveEastGap;
                }
                return DriftAction::None;

            case DriftPhase::GapEast:
                if (advanceWhenStopped(axesStopped, DriftPhase::PauseWest))
                {
                    _deadline = nowMs + kPauseMillis;
                }
                return DriftAction::None;

            case DriftPhase::PauseWest:
                if (elapsed(nowMs, _deadline))
                {
                    _phase      = DriftPhase::PassWest;
                    _seenMoving = false;
                    return DriftAction::MoveWestPass;
                }
                return DriftAction::None;

            case DriftPhase::PassWest:
                if (advanceWhenStopped(axesStopped, DriftPhase::PauseReset))
                {
                    _deadline = nowMs + kPauseMillis;
                }
                return DriftAction::None;

            case DriftPhase::PauseReset:
                if (elapsed(nowMs, _deadline))
                {
                    _phase      = DriftPhase::GapReset;
                    _seenMoving = false;
                    return DriftAction::ResetGap;
                }
                return DriftAction::None;

            case DriftPhase::GapReset:
                advanceWhenStopped(axesStopped, DriftPhase::ResumeTracking);
                return DriftAction::None;

            case DriftPhase::ResumeTracking:
                _phase = DriftPhase::Idle;
                return DriftAction::ResumeTracking;
        }

        return DriftAction::None;
    }

  private:
    /// True once the phase has seen motion and then rest.
    bool advanceWhenStopped(bool axesStopped, DriftPhase next)
    {
        if (!axesStopped)
        {
            _seenMoving = true;
            return false;
        }

        if (!_seenMoving)
        {
            return false;  // the move has not been observed running yet
        }

        _phase = next;
        return true;
    }

    static bool elapsed(uint32_t nowMs, uint32_t deadline)
    {
        return static_cast<int32_t>(nowMs - deadline) >= 0;
    }

    DriftPhase _phase         = DriftPhase::Idle;
    uint32_t _durationSeconds = 1;
    uint32_t _deadline        = 0;
    long _passSteps           = 0;
    float _passSpeed          = 0.0F;
    bool _seenMoving          = false;
};

}  // namespace core
