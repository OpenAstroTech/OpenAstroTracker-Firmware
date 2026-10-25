#pragma once

#include "ports/IMountSettings.hpp"

namespace fakes
{

/// An in-memory `ports::IMountSettings`.
///
/// Every value is a member, so a test can arrange the store, run the code under test, and then
/// assert both what was read and what was written. Defaults are zero-ish on purpose: a test that
/// forgets to set a value gets an obviously empty store rather than a plausible one.
class FakeMountSettings final : public ports::IMountSettings
{
  public:
    /// How many times the hour angle was persisted.
    int storeHaCalls = 0;

    core::DayTime getHATime() override
    {
        return haTime;
    }

    void storeHATime(const core::DayTime &value) override
    {
        haTime = value;
        storeHaCalls++;
    }

    int getUtcOffset() override
    {
        return utcOffset;
    }

    void storeUtcOffset(int value) override
    {
        utcOffset = value;
    }

    uint8_t getBrightness() override
    {
        return brightness;
    }

    void storeBrightness(uint8_t value) override
    {
        brightness = value;
    }

    float getRAStepsPerDegree() override
    {
        return raStepsPerDegree;
    }

    void storeRAStepsPerDegree(float value) override
    {
        raStepsPerDegree = value;
    }

    float getDECStepsPerDegree() override
    {
        return decStepsPerDegree;
    }

    void storeDECStepsPerDegree(float value) override
    {
        decStepsPerDegree = value;
    }

    float getAZStepsPerDegree() override
    {
        return azStepsPerDegree;
    }

    void storeAZStepsPerDegree(float value) override
    {
        azStepsPerDegree = value;
    }

    float getALTStepsPerDegree() override
    {
        return altStepsPerDegree;
    }

    void storeALTStepsPerDegree(float value) override
    {
        altStepsPerDegree = value;
    }

    float getSpeedFactor() override
    {
        return speedFactor;
    }

    void storeSpeedFactor(float value) override
    {
        speedFactor = value;
    }

    int16_t getBacklashCorrectionSteps() override
    {
        return backlashCorrectionSteps;
    }

    void storeBacklashCorrectionSteps(int16_t value) override
    {
        backlashCorrectionSteps = value;
    }

    core::Latitude getLatitude() override
    {
        return latitude;
    }

    void storeLatitude(const core::Latitude &value) override
    {
        latitude = value;
    }

    core::Longitude getLongitude() override
    {
        return longitude;
    }

    void storeLongitude(const core::Longitude &value) override
    {
        longitude = value;
    }

    float getPitchCalibrationAngle() override
    {
        return pitchCalibrationAngle;
    }

    void storePitchCalibrationAngle(float value) override
    {
        pitchCalibrationAngle = value;
    }

    float getRollCalibrationAngle() override
    {
        return rollCalibrationAngle;
    }

    void storeRollCalibrationAngle(float value) override
    {
        rollCalibrationAngle = value;
    }

    float getDECLowerLimit() override
    {
        return decLowerLimit;
    }

    void storeDECLowerLimit(float value) override
    {
        decLowerLimit = value;
    }

    float getDECUpperLimit() override
    {
        return decUpperLimit;
    }

    void storeDECUpperLimit(float value) override
    {
        decUpperLimit = value;
    }

    int32_t getRAHomingOffset() override
    {
        return raHomingOffset;
    }

    void storeRAHomingOffset(int32_t value) override
    {
        raHomingOffset = value;
    }

    int32_t getDECHomingOffset() override
    {
        return decHomingOffset;
    }

    void storeDECHomingOffset(int32_t value) override
    {
        decHomingOffset = value;
    }

    int16_t getLastFlashedVersion() override
    {
        return lastFlashedVersion;
    }

    void storeLastFlashedVersion(int16_t value) override
    {
        lastFlashedVersion = value;
    }

    int32_t getAZPosition() override
    {
        return azPosition;
    }

    void storeAZPosition(int32_t value) override
    {
        azPosition = value;
    }

    int32_t getALTPosition() override
    {
        return altPosition;
    }

    void storeALTPosition(int32_t value) override
    {
        altPosition = value;
    }

    void clearConfiguration() override
    {
        *this = FakeMountSettings();
        clearCount++;
    }

    core::DayTime haTime;
    int utcOffset                   = 0;
    uint8_t brightness              = 0;
    float raStepsPerDegree          = 0.0f;
    float decStepsPerDegree         = 0.0f;
    float azStepsPerDegree          = 0.0f;
    float altStepsPerDegree         = 0.0f;
    float speedFactor               = 0.0f;
    int16_t backlashCorrectionSteps = 0;
    core::Latitude latitude;
    core::Longitude longitude;
    float pitchCalibrationAngle = 0.0f;
    float rollCalibrationAngle  = 0.0f;
    float decLowerLimit         = 0.0f;
    float decUpperLimit         = 0.0f;
    int32_t raHomingOffset      = 0;
    int32_t decHomingOffset     = 0;
    int16_t lastFlashedVersion  = 0;
    int32_t azPosition          = 0;
    int32_t altPosition         = 0;
    int clearCount              = 0;
};

}  // namespace fakes
