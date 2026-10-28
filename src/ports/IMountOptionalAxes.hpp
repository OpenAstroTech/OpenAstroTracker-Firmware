#pragma once

#include "IMountConfiguration.hpp"
#include "types/FocuserDirection.hpp"

namespace ports
{
/// The axes a board may not have: the focuser and the AZ/ALT pair.
///
/// All three are runtime-present: a board without them holds a null axis, and every entry point
/// here is a no-op rather than absent code. That is why they belong in one port — the command
/// layer calls them without asking whether this board has the hardware.
///
/// Extends the interface chain rather than standing beside it, so `Mount` carries one vtable
/// pointer (see `IMountConfiguration`).
class IMountOptionalAxes : public IMountConfiguration
{
  public:
    virtual ~IMountOptionalAxes()
    {
    }

    // --- focuser ---

    /// Moves the focuser by `steps`, relative to where it is, at the selected rate. Enables the
    /// motor; the move completes in the background.
    virtual void focusMoveBy(long steps) = 0;

    /// Declares the current focuser position to be `steps`, without moving anything.
    virtual void focusSetStepperPosition(long steps) = 0;

    /// The focuser's current position, in its own steps. 0 on a board without a focuser.
    virtual long focusGetStepperPosition() = 0;

    /// Selects one of the four focus speeds (1 = slowest, 4 = fastest).
    virtual void focusSetSpeedByRate(int rate) = 0;

    /// The focuser's current speed in steps per second, as the axis reports it.
    virtual float getFocusSpeed() const = 0;

    /// Starts moving continuously in `direction` at the selected rate. `focusStop()` ends it.
    virtual void focusContinuousMove(types::FocuserDirection direction) = 0;

    /// Stops the focuser wherever it is.
    virtual void focusStop() = 0;

    /// True while the focuser is moving.
    virtual bool isRunningFocus() const = 0;

    // --- azimuth / altitude ---

    /// Nudges one AZ/ALT axis by `arcMinutes`. `direction` selects the axis
    /// (`AZIMUTH_STEPS` or `ALTITUDE_STEPS`).
    virtual void moveBy(int direction, float arcMinutes) = 0;

    /// Declares the current AZ/ALT position to be the home position and persists it. A no-op on a
    /// board without the axes, like every other entry point here.
    virtual void setAZALTHome() = 0;

    /// Drives both AZ/ALT axes back to the home position.
    virtual void moveAZALTToHome() = 0;

    /// The current AZ/ALT positions in steps, 0 for an axis this board does not have.
    virtual void getAZALTPositions(long &azimuthSteps, long &altitudeSteps) = 0;
};

}  // namespace ports
