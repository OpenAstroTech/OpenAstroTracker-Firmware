#pragma once

#include <stdint.h>

namespace core
{

/// What the mount was asked to slew to. `Manual` is a direction-pad move, the others are
/// the three go-to shapes the firmware has.
enum class SlewIntent : uint8_t
{
    None,
    Target,
    Home,
    Park,
    Manual,
};

/// The part of the slew state that `setHome()` invalidates and the arrival sequence
/// needs back.
struct SlewSnapshot {
    SlewIntent intent;
};

/// The slew's bookkeeping.
///
/// This is the state the arrival sequence consults while it finishes a move: which slew
/// was requested, how far the axes had to travel, whether an axis was seen moving on the
/// previous tick, and whether a backlash correction is still owed.
///
/// It is deliberately **not** the arrival sequence itself. That sequence stops tracking,
/// writes driver microsteps, calls `setHome()`, refreshes the display and may immediately
/// start a second move — i.e. it arbitrates between axes and subsystems, which is the
/// coordinator's job (PLAN.md step 10). Keeping the bookkeeping here means those flags
/// stop being four loose members that only `Mount::loop()` understands, and lets the
/// transitions be tested.
///
/// Threading: main-loop-only.
class SlewController
{
  public:
    void begin(SlewIntent intent)
    {
        _intent = intent;
    }

    void end()
    {
        _intent = SlewIntent::None;
    }

    SlewIntent intent() const
    {
        return _intent;
    }

    bool isActive() const
    {
        return _intent != SlewIntent::None;
    }

    /// Distance the axes had to travel, kept so a display can show a percentage.
    void setTotalMove(float raSteps, float decSteps)
    {
        _totalRaSteps  = raSteps;
        _totalDecSteps = decSteps;
    }

    void clearTotalMove()
    {
        _totalRaSteps  = 0.0f;
        _totalDecSteps = 0.0f;
    }

    float totalRaSteps() const
    {
        return _totalRaSteps;
    }

    float totalDecSteps() const
    {
        return _totalDecSteps;
    }

    /// "An axis was moving when the main loop last looked", so the loop can tell a
    /// completed slew from a mount that was simply idle.
    void noteRunning(bool anyAxisRunning)
    {
        _wasRunning = anyAxisRunning;
    }

    bool wasRunning() const
    {
        return _wasRunning;
    }

    void requestBacklashCorrection()
    {
        _backlashOwed = true;
    }

    void clearBacklashCorrection()
    {
        _backlashOwed = false;
    }

    bool backlashOwed() const
    {
        return _backlashOwed;
    }

    /// `setHome()` resets the slew state, because a new home position invalidates any
    /// move that was in flight. The arrival sequence runs *during* a parking slew and
    /// needs its intent back afterwards, so it snapshots before and restores after.
    SlewSnapshot snapshot() const
    {
        SlewSnapshot saved;
        saved.intent = _intent;
        return saved;
    }

    void restore(const SlewSnapshot &saved)
    {
        _intent = saved.intent;
    }

  private:
    SlewIntent _intent   = SlewIntent::None;
    float _totalRaSteps  = 0.0f;
    float _totalDecSteps = 0.0f;
    bool _wasRunning     = false;
    bool _backlashOwed   = false;
};

}  // namespace core
