#pragma once

#include <stdint.h>

#include "ports/PortVocabulary.hpp"
#include "types/DayTime.hpp"
#include "types/Declination.hpp"

namespace ports
{
/// What the command layer may ask the mount to move.
///
/// A focused view of the mount's motion surface, not a facade of the whole class: the command
/// layer needs to request movement and nothing else. Everything else the mount answers is named by
/// its own port (`IMountReadModel`, `IMountConfiguration`, `IMountSiteTime`).
///
/// `direction` is the `int` bitmask the mount's motion calls take (`EAST`, `WEST`, `NORTH`,
/// `SOUTH`, `TRACKING`, `ALL_DIRECTIONS`). Those names live in `ports/PortVocabulary.hpp` and are
/// already the vocabulary of the command layer; inventing a parallel enum here would mean two
/// vocabularies in flight at once. The bit values are pinned to `core::SlewDirection` and
/// `core::CoordinateFormat` by `static_assert` in the executor.
///
/// Every request here is admitted or refused by the mount's own arbitration table
/// (`core::MountRequests`), which is why the methods return nothing: a refusal is not an error the
/// caller acts on — the Meade protocol acknowledges the command and the host polls for the
/// resulting state.
class IMountMotion
{
  public:
    virtual ~IMountMotion()
    {
    }

    /// Starts driving one axis (or `TRACKING`) in the given direction bitmask. Returns immediately:
    /// the axes ramp up in the background and the resulting state is read back later.
    virtual void startSlewing(int direction) = 0;

    /// Stops every role the direction bitmask names.
    virtual void stopSlewing(int direction) = 0;

    /// Stops one axis outright, whichever role was driving it.
    virtual void stopSlewing(StepperAxis axis) = 0;

    /// Slews both axes to the coordinates last written to the targets (`:MS#`). Refused while a
    /// park sequence owns the axes.
    virtual void startSlewingToTarget() = 0;

    /// Slews to the home position: DEC zero and the RA home point derived from the tracking
    /// position's current offset.
    virtual void startSlewingToHome() = 0;

    /// Stops tracking and the axes, then homes RA/DEC to the park position.
    virtual void park() = 0;

    /// Starts a guide pulse: a move in the given direction that ends by itself after `duration`
    /// milliseconds.
    virtual void guidePulse(uint8_t direction, int duration) = 0;

    /// Adds `steps` to one axis' position, as the handbox's move keys do. One axis only;
    /// `RA_AND_DEC_STEPS` is refused.
    virtual void moveStepperBy(StepperAxis which, long steps) = 0;

    /// Moves one axis to an absolute position without changing the tracking/guide axes.
    virtual void moveStepperTo(StepperAxis which, long position) = 0;

    /// True while the RA or DEC axis is in a slew, or a park sequence is running.
    virtual bool isSlewingRAorDEC() const = 0;

    /// True while the tracking role is driving the RA axis.
    virtual bool isSlewingTRK() const = 0;

    /// Re-anchors every coordinate to where the axes are now. Blocking: it stops the axes and
    /// waits for them, because the position it records would be wrong if taken while they moved.
    /// The protocol's `:SHP#` is one caller.
    virtual void setHome(bool clearZeroPos) = 0;

    /// Declares the current position to be the given coordinates, without moving. The mirror image
    /// of `setHome()`: one re-anchors the coordinates to where the axes are, the other the axes to
    /// where the coordinates are. Called by `:CM#` (sync to the target) and `:SY#` (sync to given
    /// coordinates).
    virtual void syncPosition(const types::DayTime &ra, const types::Declination &dec) = 0;

    /// Moves the tracking engine's idea of its own position, for the `:XT#` calibration.
    virtual void setTrackingStepperPos(long stepPos) = 0;

    /// Starts the drift-alignment state machine. It returns immediately; the controller is
    /// ticked from `app::MountExecutor::loop()`.
    ///
    /// Virtual with an empty default rather than pure: `Mount` declares it only under
    /// `SUPPORT_DRIFT_ALIGNMENT`, so a board without the feature would otherwise be an abstract
    /// class. The default keeps the `#if` out of the port header.
    virtual void startDriftAlignment(int)
    {
    }

    /// Runs the hall-sensor homing search on one axis and reports whether the sensor was found.
    ///
    /// It is the one request here that blocks: `core::HomingController` drives the state machine,
    /// but this entry point waits for the search to finish. A board without a homing sensor
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
