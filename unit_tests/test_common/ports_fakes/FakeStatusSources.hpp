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

    /// A move counts as planned when this is non-zero, so the slew-status view has rows to draw.
    float plannedRaStepsValue  = 0.0F;
    float plannedDecStepsValue = 0.0F;

    int raPercentValue  = 100;
    int decPercentValue = 100;

    core::DayTime lstTime;
    core::DayTime haTime;

    /// What the screen last asked for, so a test can assert the flag words.
    mutable uint8_t lastRaType    = 0;
    mutable uint8_t lastDecType   = 0;
    mutable long lastPositionAxis = 0;

    core::DayTime currentRA() const override
    {
        return core::DayTime();
    }

    core::Declination currentDEC() const override
    {
        return core::Declination();
    }

    core::DayTime &targetRA() override
    {
        return targetRaValue;
    }

    core::Declination &targetDEC() override
    {
        return targetDecValue;
    }

    core::DayTime calculateHa() override
    {
        return haTime;
    }

    core::DayTime calculateLst() override
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

    core::DayTime hourAngle() override
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

    /// The target the screen writes; a test asserts on these directly.
    core::DayTime targetRaValue;
    core::Declination targetDecValue;
};

/// A `ports::IMountConfiguration` whose only interesting answers are the four the status screen
/// reads: the tracking speed, the two DEC soft limits and — via the inherited motion face — nothing.
class FakeMountConfiguration final : public ports::IMountConfiguration
{
  public:
    float trackingSpeed = 1.002738F;
    float decLowerLimit = -10.0F;
    float decUpperLimit = 80.0F;

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
    void syncPosition(const core::DayTime &, const core::Declination &) override
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

    float getStepsPerDegree(StepperAxis) override
    {
        return 0.0F;
    }
    void setStepsPerDegree(StepperAxis, float) override
    {
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
        return 1.0F;
    }
    void setSpeedCalibration(float, bool) override
    {
    }
    void setDecLimitPosition(bool, float) override
    {
    }
    void clearDecLimitPosition(bool) override
    {
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
    void setBacklashCorrection(int) override
    {
    }
    int getBacklashCorrection() override
    {
        return 0;
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
    core::Latitude siteLatitude   = core::Latitude(52.5F);
    core::Longitude siteLongitude = core::Longitude(-1.25F);
    core::DayTime localTime       = core::DayTime(21, 30, 15);
    core::DayTime utcTime         = core::DayTime(20, 30, 15);
    LocalDate localDate           = {2024, 7, 9};
    int utcOffset                 = 1;

    /// The last hour angle written, and how many times it was written.
    core::DayTime lastHa;
    int setHaCalls = 0;

    core::Latitude latitude() const override
    {
        return siteLatitude;
    }
    core::Longitude longitude() const override
    {
        return siteLongitude;
    }
    void setLatitude(const core::Latitude &value) override
    {
        siteLatitude = value;
    }
    void setLongitude(const core::Longitude &value) override
    {
        siteLongitude = value;
    }
    core::DayTime getLocalTime() override
    {
        return localTime;
    }
    core::DayTime getUtcTime() override
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
    void setLocalStartTime(const core::DayTime &) override
    {
    }
    void setLocalStartDate(int, int, int) override
    {
    }
    void setLST(const core::DayTime &) override
    {
    }
    void setHA(const core::DayTime &value) override
    {
        lastHa = value;
        setHaCalls++;
    }
};

}  // namespace fakes
