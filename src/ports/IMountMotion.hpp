#pragma once

#include <stdint.h>

#include "Types.hpp"
#include "core/types/DayTime.hpp"
#include "core/types/Declination.hpp"

namespace ports
{
/// What the command layer may ask the mount to move.
///
/// The command layer is the only caller of these (the menus call `Mount` directly today), so this
/// is the narrow slice of `Mount` that Phase 5 step 3 needs, not a facade of the whole class: the
/// adapter's *other* dependencies are separately named and will be moved one group at a time.
///
/// `direction` stays the `int` bitmask the mount has always used (`EAST`, `WEST`, `NORTH`,
/// `SOUTH`, `TRACKING`, `ALL_DIRECTIONS`). Those names live in `Mount.hpp` and are already the
/// vocabulary of the command layer; inventing a parallel enum here would mean two vocabularies in
/// flight at once. Typing it is a separate cleanup, tracked in PLAN.md.
///
/// Every request here is admitted or refused by the mount's own arbitration table
/// (`core::MountRequests`), which is why the methods return nothing: a refusal is not an error the
/// caller acts on — the legacy protocol acknowledges and the host polls for the resulting state.
class IMountMotion
{
  public:
    virtual ~IMountMotion()
    {
    }

    virtual void startSlewing(int direction)                  = 0;
    virtual void stopSlewing(int direction)                   = 0;
    virtual void stopSlewing(StepperAxis axis)                = 0;
    virtual void startSlewingToTarget()                       = 0;
    virtual void startSlewingToHome()                         = 0;
    virtual void park()                                       = 0;
    virtual void guidePulse(uint8_t direction, int duration)  = 0;
    virtual void moveStepperBy(StepperAxis which, long steps) = 0;

    virtual bool isSlewingRAorDEC() const = 0;
    virtual bool isSlewingTRK() const     = 0;

    /// Re-anchors every coordinate to where the axes are now. Blocking: it stops the axes and
    /// waits for them, because the position it records would be wrong if taken while they moved.
    /// The protocol's `:CM#` sync is the caller.
    virtual void setHome(bool clearZeroPos) = 0;

    /// Declares the current position to be the given coordinates (`:CM#` / `:CS#`), without
    /// moving. The mirror image of `setHome()`: one re-anchors the coordinates to where the axes
    /// are, the other the axes to where the coordinates are.
    virtual void syncPosition(const core::DayTime &ra, const core::Declination &dec) = 0;

    /// Moves the tracking engine's idea of its own position, for the `:XT#` calibration.
    virtual void setTrackingStepperPos(long stepPos) = 0;

    /// Starts the drift-alignment phase machine. It returns immediately; the controller is
    /// ticked from `Mount::loop()`.
    ///
    /// Virtual with an empty default rather than pure: `Mount` declares it only under
    /// `SUPPORT_DRIFT_ALIGNMENT`, so a board without the feature would otherwise be an abstract
    /// class. The null-object shape again, with no `#if` in a port header.
    virtual void startDriftAlignment(int)
    {
    }

    /// Runs the hall-sensor homing search on one axis and reports whether the sensor was found.
    ///
    /// It is the one request here that blocks: `core::HomingController` drives the state machine,
    /// but this legacy entry point waits for the search to finish. A board without a homing sensor
    /// returns false rather than doing nothing silently.
    /// Virtual with a "not found" default rather than pure: `Mount` declares it only when the
    /// build has a hall sensor, so a board without one would otherwise be an abstract class. The
    /// same null-object shape the level calibration uses.
    virtual bool findHomeByHallSensor(StepperAxis axis, int initialDirection, int searchDistance)
    {
        (void) axis;
        (void) initialDirection;
        (void) searchDistance;
        return false;
    }
};

}  // namespace ports
