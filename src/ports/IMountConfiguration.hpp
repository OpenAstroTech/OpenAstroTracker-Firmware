#pragma once

#include <stdint.h>

#include "IMountMotion.hpp"
#include "ports/PortVocabulary.hpp"

namespace ports
{
/// The mount's *adjustable* configuration, as a host or a menu sets it.
///
/// Steps-per-degree, the slew speed, the two calibration angles, the DEC soft limits, the homing
/// offsets and the RA backlash figure are one group: each is a value the operator calibrates, each
/// is persisted, and each is read back by the same commands that write it. Keeping them together
/// means the command layer never needs the whole mount to answer ":GX# and set it again".
///
/// Not here: anything the mount computes per move (target coordinates, positions, status), which
/// is the read model, and the microstep rates, which come from `core::MountConfig` at boot.
/// Extends `IMountMotion` rather than standing beside it: the command layer needs both, and one
/// interface hierarchy means `Mount` carries a single vtable pointer instead of one per interface.
class IMountConfiguration : public IMountMotion
{
  public:
    virtual ~IMountConfiguration()
    {
    }

    /// Microsteps per degree of axis travel for the axis named: the calibration figure every
    /// conversion between steps and coordinates uses. AZ/ALT answer their fixed board constants.
    virtual float getStepsPerDegree(StepperAxis which) = 0;

    /// Persists a calibrated microsteps-per-degree figure for RA or DEC; the AZ/ALT figures are
    /// board constants and are not tunable.
    virtual void setStepsPerDegree(StepperAxis which, float steps) = 0;

    /// Sets the manual slew speed of one axis, in degrees per second. Only meaningful in manual
    /// slew mode, where the RA microstep rate is the slew rate rather than the tracking rate.
    virtual void setSpeed(StepperAxis which, float speedDegsPerSec) = 0;

    /// The current speed for a movement direction (`TRACKING`, `NORTH`/`SOUTH`, `EAST`/`WEST`), in
    /// degrees per second. Returns 0 for a direction no axis is moving in.
    virtual float getSpeed(int direction) = 0;

    /// The slew-rate index the handbox and the protocol select between (the preset speeds).
    virtual void setSlewRate(int rate) = 0;
    virtual int getSlewRate()          = 0;

    /// The sidereal tracking-speed trim, as a multiplier the operator calibrates so the mount
    /// tracks a star instead of drifting off it.
    virtual float getSpeedCalibration()                               = 0;
    virtual void setSpeedCalibration(float value, bool saveToStorage) = 0;

    /// The two level-calibration angles are the only entries a board can genuinely not have: the
    /// gyro is optional, and `Mount` declares these only when `USE_GYRO_LEVEL` is set. They are
    /// virtual with a no-op default rather than pure, so a board without a level sensor is a
    /// working implementation ("no angle") instead of an abstract class — the same null-object
    /// shape the optional axes use, without an `#if` in a port header.
    virtual float getPitchCalibrationAngle()
    {
        return 0.0F;
    }

    virtual void setPitchCalibrationAngle(float)
    {
    }

    virtual float getRollCalibrationAngle()
    {
        return 0.0F;
    }

    virtual void setRollCalibrationAngle(float)
    {
    }

    /// The DEC soft limits, in degrees, as the mount enforces them: a DEC move is clamped so the
    /// axis cannot travel past the calibrated range.
    ///
    /// `upper` selects which limit is set or cleared; setting one persists it, clearing it removes
    /// the stop. The default repeats `Mount`'s own so a caller that omits the angle compiles either
    /// way; a virtual override may not re-declare a default, so both have to carry it.
    virtual void setDecLimitPosition(bool upper, float limitAngle = 0.0F)   = 0;
    virtual void clearDecLimitPosition(bool upper)                          = 0;
    virtual void getDecLimitPositions(float &lowerLimit, float &upperLimit) = 0;

    /// The persisted microstep offset the homing search records for one axis: the distance from
    /// the sensor trigger to the axis' declared zero position.
    virtual void setHomingOffset(StepperAxis axis, long offset) = 0;
    virtual long getHomingOffset(StepperAxis axis)              = 0;

    /// The RA backlash correction, in microsteps, applied when the axis reverses direction.
    virtual void setBacklashCorrection(int steps) = 0;
    virtual int getBacklashCorrection()           = 0;

    /// Switches between sidereal tracking and a constant manual slew, stopping the axes first:
    /// the RA microstep rate changes underneath, which must not happen to a moving axis.
    virtual void setManualSlewMode(bool enable) = 0;

    /// Wipes every stored value and reloads the compile-time defaults (`:XFR#`).
    virtual void clearConfiguration() = 0;
};

}  // namespace ports
