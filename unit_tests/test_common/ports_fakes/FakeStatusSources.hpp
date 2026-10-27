#pragma once

#include <string.h>

#include "ports/IMountConfiguration.hpp"
#include "ports/IMountReadModel.hpp"
#include "ports/IMountSiteTime.hpp"

namespace fakes
{
/// A `ports::IMountReadModel` with every answer under the test's control.
///
/// The coordinate readouts are driven by the `TARGET_STRING` / `CURRENT_STRING` flag the caller
/// passes, because that is the only distinction the screens make; the fake records the exact flag
/// word so a test can assert what was asked for.
class FakeMountReadModel : public ports::IMountReadModel
{
  public:
    long raStepperPosition  = 0;
    long decStepperPosition = 0;
    long trkStepperPosition = 0;

    const char *raTargetText   = "00:00:00";
    const char *raCurrentText  = "01:02:03";
    const char *decTargetText  = "+00@00:00";
    const char *decCurrentText = "-12@34:56";

    bool slewingIdle = true;

    /// The site's hemisphere, as the POI screen asks it. Northern is the default the firmware uses.
    bool northernHemisphere = true;

    /// A move counts as planned when this is non-zero, so the slew-status view has rows to draw.
    float plannedRaStepsValue  = 0.0F;
    float plannedDecStepsValue = 0.0F;

    int raPercentValue  = 100;
    int decPercentValue = 100;

    types::DayTime lstTime;
    types::DayTime haTime;

    /// What the screen last asked for, so a test can assert the flag words.
    mutable uint8_t lastRaType    = 0;
    mutable uint8_t lastDecType   = 0;
    mutable long lastPositionAxis = 0;

    types::DayTime currentRA() const override
    {
        return currentRaValue;
    }

    types::Declination currentDEC() const override
    {
        return types::Declination();
    }

    types::DayTime &targetRA() override
    {
        return targetRaValue;
    }

    types::Declination &targetDEC() override
    {
        return targetDecValue;
    }

    types::DayTime calculateHa() override
    {
        return haTime;
    }

    types::DayTime calculateLst() override
    {
        return lstTime;
    }

    void calculateStepperPositions(float, float, long &raPos, long &decPos) override
    {
        raPos  = 0;
        decPos = 0;
    }

    float checkRALimit() override
    {
        return 0.0F;
    }

    types::DayTime hourAngle() override
    {
        return haTime;
    }

    long getCurrentStepperPosition(StepperAxis) override
    {
        return 0;
    }

    /// The status screen asks by movement direction (`WEST`, `NORTH`, `TRACKING`), as the legacy
    /// screen did, so the fake records the direction and answers from the matching field.
    long getCurrentStepperPosition(int direction) override
    {
        lastPositionAxis = direction;
        if (direction == NORTH)
        {
            return decStepperPosition;
        }
        if (direction == TRACKING)
        {
            return trkStepperPosition;
        }
        return raStepperPosition;
    }

    bool isAxisRunning(StepperAxis) override
    {
        return false;
    }

    bool isSlewingIdle() const override
    {
        return slewingIdle;
    }

    bool getStepperProgress(int &raPercentage, int &decPercentage) override
    {
        raPercentage  = raPercentValue;
        decPercentage = decPercentValue;
        return (raPercentValue < 100) || (decPercentValue < 100);
    }

    float plannedRaSteps() const override
    {
        return plannedRaStepsValue;
    }

    float plannedDecSteps() const override
    {
        return plannedDecStepsValue;
    }

    bool isBootComplete() override
    {
        return true;
    }

    bool isGuiding() const override
    {
        return false;
    }

    bool isNorthernHemisphere() const override
    {
        return northernHemisphere;
    }

    const char *raText(uint8_t type, uint8_t) override
    {
        lastRaType = type;
        return ((type & CURRENT_STRING) != 0) ? raCurrentText : raTargetText;
    }

    const char *decText(uint8_t type, uint8_t) override
    {
        lastDecType = type;
        return ((type & CURRENT_STRING) != 0) ? decCurrentText : decTargetText;
    }

    const char *getStatusString() override
    {
        return "Idle";
    }

    const char *getStepperInfo() override
    {
        return "";
    }

    const char *getMountHardwareInfo() override
    {
        return "";
    }

    const char *getAutoHomingStates() const override
    {
        return "";
    }

    long getNumCommandsReceived() override
    {
        return 0;
    }

    void commandReceived() override
    {
    }

    /// The target the screen writes, and the current position it reads.
    types::DayTime targetRaValue;
    types::DayTime currentRaValue;
    types::Declination targetDecValue;
};

/// A `ports::IMountConfiguration` whose only interesting answers are the four the status screen
/// reads: the tracking speed, the two DEC soft limits and — via the inherited motion face — nothing.
class FakeMountConfiguration final : public ports::IMountConfiguration
{
  public:
    float trackingSpeed = 1.002738F;
    float decLowerLimit = -10.0F;
    float decUpperLimit = 80.0F;

    float speedCalibration      = 1.0F;
    float raStepsPerDegree      = 0.0F;
    float decStepsPerDegree     = 0.0F;
    int backlash                = 0;
    float rollCalibrationAngle  = 0.0F;
    float pitchCalibrationAngle = 0.0F;

    int liveSpeedCalls        = 0;
    int persistedSpeedCalls   = 0;
    int setStepsCalls         = 0;
    int setBacklashCalls      = 0;
    int setDecLimitCalls      = 0;
    int clearDecLimitCalls    = 0;
    bool lastDecLimitWasUpper = false;
    int setRollCalls          = 0;
    int setPitchCalls         = 0;

    void startSlewing(int) override
    {
    }
    void stopSlewing(int) override
    {
    }
    void stopSlewing(StepperAxis) override
    {
    }
    void startSlewingToTarget() override
    {
    }
    void startSlewingToHome() override
    {
    }
    void park() override
    {
    }
    void guidePulse(uint8_t, int) override
    {
    }
    void moveStepperBy(StepperAxis, long) override
    {
    }
    void moveStepperTo(StepperAxis, long) override
    {
    }
    bool isSlewingRAorDEC() const override
    {
        return false;
    }
    bool isSlewingTRK() const override
    {
        return false;
    }
    void setHome(bool) override
    {
    }
    void syncPosition(const types::DayTime &, const types::Declination &) override
    {
    }
    void setTrackingStepperPos(long) override
    {
    }
    void startDriftAlignment(int) override
    {
    }
    bool findHomeByHallSensor(StepperAxis, int, int) override
    {
        return false;
    }

    float getStepsPerDegree(StepperAxis which) override
    {
        return (which == RA_STEPS) ? raStepsPerDegree : decStepsPerDegree;
    }
    void setStepsPerDegree(StepperAxis which, float steps) override
    {
        setStepsCalls++;
        if (which == RA_STEPS)
        {
            raStepsPerDegree = steps;
        }
        else
        {
            decStepsPerDegree = steps;
        }
    }
    void setSpeed(StepperAxis, float) override
    {
    }
    float getSpeed(int direction) override
    {
        return (direction == TRACKING) ? trackingSpeed : 0.0F;
    }
    void setSlewRate(int) override
    {
    }
    int getSlewRate() override
    {
        return 0;
    }
    float getSpeedCalibration() override
    {
        return speedCalibration;
    }
    void setSpeedCalibration(float value, bool saveToStorage) override
    {
        speedCalibration = value;
        if (saveToStorage)
        {
            persistedSpeedCalls++;
        }
        else
        {
            liveSpeedCalls++;
        }
    }
    void setDecLimitPosition(bool upper, float) override
    {
        setDecLimitCalls++;
        lastDecLimitWasUpper = upper;
    }
    void clearDecLimitPosition(bool upper) override
    {
        clearDecLimitCalls++;
        lastDecLimitWasUpper = upper;
    }
    void getDecLimitPositions(float &lowerLimit, float &upperLimit) override
    {
        lowerLimit = decLowerLimit;
        upperLimit = decUpperLimit;
    }
    void setHomingOffset(StepperAxis, long) override
    {
    }
    long getHomingOffset(StepperAxis) override
    {
        return 0;
    }
    void setBacklashCorrection(int steps) override
    {
        setBacklashCalls++;
        backlash = steps;
    }
    int getBacklashCorrection() override
    {
        return backlash;
    }
    float getRollCalibrationAngle() override
    {
        return rollCalibrationAngle;
    }
    void setRollCalibrationAngle(float value) override
    {
        setRollCalls++;
        rollCalibrationAngle = value;
    }
    float getPitchCalibrationAngle() override
    {
        return pitchCalibrationAngle;
    }
    void setPitchCalibrationAngle(float value) override
    {
        setPitchCalls++;
        pitchCalibrationAngle = value;
    }
    void setManualSlewMode(bool) override
    {
    }
    void clearConfiguration() override
    {
    }
};

/// A `ports::IMountSiteTime` holding fixed site and clock values.
class FakeSiteTime final : public ports::IMountSiteTime
{
  public:
    types::Latitude siteLatitude   = types::Latitude(52.5F);
    types::Longitude siteLongitude = types::Longitude(-1.25F);
    types::DayTime localTime       = types::DayTime(21, 30, 15);
    types::DayTime utcTime         = types::DayTime(20, 30, 15);
    LocalDate localDate            = {2024, 7, 9};
    int utcOffset                  = 1;

    /// The last hour angle written, and how many times it was written.
    types::DayTime lastHa;
    int setHaCalls = 0;

    types::Latitude latitude() const override
    {
        return siteLatitude;
    }
    types::Longitude longitude() const override
    {
        return siteLongitude;
    }
    void setLatitude(const types::Latitude &value) override
    {
        siteLatitude = value;
    }
    void setLongitude(const types::Longitude &value) override
    {
        siteLongitude = value;
    }
    types::DayTime getLocalTime() override
    {
        return localTime;
    }
    types::DayTime getUtcTime() override
    {
        return utcTime;
    }
    LocalDate getLocalDate() override
    {
        return localDate;
    }
    int getLocalUtcOffset() const override
    {
        return utcOffset;
    }
    void setLocalUtcOffset(int offset) override
    {
        utcOffset = offset;
    }
    void setLocalStartTime(const types::DayTime &) override
    {
    }
    void setLocalStartDate(int, int, int) override
    {
    }
    void setLST(const types::DayTime &) override
    {
    }
    void setHA(const types::DayTime &value) override
    {
        lastHa = value;
        setHaCalls++;
    }
};

}  // namespace fakes
