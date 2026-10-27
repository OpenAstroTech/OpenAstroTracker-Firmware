#pragma once

#include <stdint.h>

#include "types/DayTime.hpp"
#include "types/Latitude.hpp"
#include "types/Longitude.hpp"

namespace ports
{
/// The mount's durable settings: the values the firmware remembers across boots.
///
/// Typed on purpose. The domain asks for "the RA steps per degree", not for a
/// sixteen-bit value at address 0x12: where a value lives and how it is encoded is
/// the adapter's business, and an encoding change must not reach the domain.
///
/// Every value is a get/store pair: the getter returns the stored value, or the compile-time
/// default when nothing has been stored, and the store writes it through immediately. Reads and
/// writes are not ISR-safe and are main-loop-only, like the rest of the persistent store.
class IMountSettings
{
  public:
    virtual ~IMountSettings()
    {
    }

    /// The hour angle the mount was calibrated at, restored at boot so the alignment survives a
    /// power cycle.
    virtual types::DayTime getHATime()                 = 0;
    virtual void storeHATime(const types::DayTime &ha) = 0;

    /// Timezone offset from UTC, same sign as `IMountSiteTime::getLocalUtcOffset`
    /// (`local = UTC + offset`).
    virtual int getUtcOffset()                 = 0;
    virtual void storeUtcOffset(int utcOffset) = 0;

    /// Backlight level of the menu display, 0-255. Stored with the rest of the settings
    /// because it is a user preference that must survive a power cycle.
    virtual uint8_t getBrightness()                  = 0;
    virtual void storeBrightness(uint8_t brightness) = 0;

    /// Calibrated microsteps per degree for RA, DEC and the optional AZ/ALT axes.
    virtual float getRAStepsPerDegree()                      = 0;
    virtual void storeRAStepsPerDegree(float stepsPerDegree) = 0;

    virtual float getDECStepsPerDegree()                      = 0;
    virtual void storeDECStepsPerDegree(float stepsPerDegree) = 0;

    virtual float getAZStepsPerDegree()                      = 0;
    virtual void storeAZStepsPerDegree(float stepsPerDegree) = 0;

    virtual float getALTStepsPerDegree()                      = 0;
    virtual void storeALTStepsPerDegree(float stepsPerDegree) = 0;

    /// The tracking-speed trim multiplier (1.0 is nominal).
    virtual float getSpeedFactor()                   = 0;
    virtual void storeSpeedFactor(float speedFactor) = 0;

    /// RA backlash correction, in microsteps.
    virtual int16_t getBacklashCorrectionSteps()             = 0;
    virtual void storeBacklashCorrectionSteps(int16_t steps) = 0;

    /// The site, as the operator or the GPS set it.
    virtual types::Latitude getLatitude()                       = 0;
    virtual void storeLatitude(const types::Latitude &latitude) = 0;

    virtual types::Longitude getLongitude()                        = 0;
    virtual void storeLongitude(const types::Longitude &longitude) = 0;

    /// The level-sensor calibration angles, in degrees.
    virtual float getPitchCalibrationAngle()             = 0;
    virtual void storePitchCalibrationAngle(float angle) = 0;

    virtual float getRollCalibrationAngle()             = 0;
    virtual void storeRollCalibrationAngle(float angle) = 0;

    /// The DEC soft limits, in degrees.
    virtual float getDECLowerLimit()             = 0;
    virtual void storeDECLowerLimit(float limit) = 0;

    virtual float getDECUpperLimit()             = 0;
    virtual void storeDECUpperLimit(float limit) = 0;

    /// Microstep offsets from the homing sensor trigger to the declared zero position.
    virtual int32_t getRAHomingOffset()              = 0;
    virtual void storeRAHomingOffset(int32_t offset) = 0;

    virtual int32_t getDECHomingOffset()              = 0;
    virtual void storeDECHomingOffset(int32_t offset) = 0;

    /// The firmware version that last wrote this store, so an update can migrate values that an
    /// older layout left meaningless.
    virtual int16_t getLastFlashedVersion()               = 0;
    virtual void storeLastFlashedVersion(int16_t version) = 0;

    /// The persisted AZ/ALT home positions, in steps.
    virtual int32_t getAZPosition()                = 0;
    virtual void storeAZPosition(int32_t position) = 0;

    virtual int32_t getALTPosition()                = 0;
    virtual void storeALTPosition(int32_t position) = 0;

    /// Forgets every stored setting; the next read returns the compile-time default.
    virtual void clearConfiguration() = 0;
};

}  // namespace ports
