#pragma once

#include <stdint.h>

namespace core
{

/// What one tick of the AZ/ALT axes reported.
enum class AzAltUpdate : uint8_t
{
    None,
    JustStopped,  ///< neither axis is moving now, but one was on the previous tick
};

/// The bookkeeping for the optional AZ/ALT axes.
///
/// The axes themselves are plain pointers owned elsewhere; what this controller adds is
/// the one thing the mount had to remember: "a motor was moving last time I looked", so
/// the moment both stop can be detected and their outputs switched off. That edge is
/// tested here instead of being a bare `bool` with two copies of the same poll around it.
///
/// Threading: main-loop-only.
class AzAltController
{
  public:
    /// Feed the current "is any axis moving" answer once per tick.
    AzAltUpdate update(bool anyAxisRunning)
    {
        if (anyAxisRunning)
        {
            _wasRunning = true;
            return AzAltUpdate::None;
        }

        if (_wasRunning)
        {
            _wasRunning = false;
            return AzAltUpdate::JustStopped;
        }

        return AzAltUpdate::None;
    }

    bool wasRunning() const
    {
        return _wasRunning;
    }

    /// Forgetting the latch without reporting a stop, for a re-configured mount.
    void reset()
    {
        _wasRunning = false;
    }

  private:
    bool _wasRunning = false;
};

}  // namespace core
