#pragma once

#include <stdint.h>

#include "IMountMotion.hpp"
#include "Types.hpp"

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
/// interface hierarchy means `Mount` carries one vtable pointer instead of one per interface — 2 B
/// of AVR SRAM each in an object that is a single global, which is worth the slightly odd reading
/// of "configuration is a kind of motion interface".
class IMountConfiguration : public IMountMotion
{
  public:
    virtual ~IMountConfiguration()
    {
    }

    virtual float getStepsPerDegree(StepperAxis which)             = 0;
    virtual void setStepsPerDegree(StepperAxis which, float steps) = 0;

    virtual void setSpeed(StepperAxis which, float speedDegsPerSec) = 0;
    virtual float getSpeed(int direction)                           = 0;
    virtual void setSlewRate(int rate)                              = 0;

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

    /// The default repeats `Mount`'s own so a caller that omits the angle compiles either way;
    /// a virtual override may not re-declare a default, so both have to carry it.
    virtual void setDecLimitPosition(bool upper, float limitAngle = 0.0F)   = 0;
    virtual void clearDecLimitPosition(bool upper)                          = 0;
    virtual void getDecLimitPositions(float &lowerLimit, float &upperLimit) = 0;

    virtual void setHomingOffset(StepperAxis axis, long offset) = 0;
    virtual long getHomingOffset(StepperAxis axis)              = 0;

    virtual void setBacklashCorrection(int steps) = 0;
    virtual int getBacklashCorrection()           = 0;

    /// Switches between sidereal tracking and a constant manual slew, stopping the axes first:
    /// the RA microstep rate changes underneath, which must not happen to a moving axis.
    virtual void setManualSlewMode(bool enable) = 0;

    /// Wipes every stored value and reloads the compile-time defaults (`:XFR#`).
    virtual void clearConfiguration() = 0;

};

}  // namespace ports
