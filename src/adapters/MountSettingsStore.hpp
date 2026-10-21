#pragma once

#include "EPROMStore.hpp"
#include "ports/IMountSettings.hpp"

namespace adapters
{
/// `ports::IMountSettings` over the legacy `EEPROMStore` facade.
///
/// The typed accessors and the on-device encoding stay in `EPROMStore`: it is the
/// single writer of that format and the menu screens, the Meade adapter and the
/// composition root still read it directly. This adapter only gives the domain an
/// injectable handle, so `Mount` (and later the controllers) never name the static
/// facade and a test can substitute the port.
///
/// Header-only and every method a single forward, so an optimising build collapses it
/// to the same calls the static facade made before.
class MountSettingsStore final : public ports::IMountSettings
{
  public:
    core::DayTime getHATime() override
    {
        return EEPROMStore::getHATime();
    }

    void storeHATime(const core::DayTime &ha) override
    {
        EEPROMStore::storeHATime(DayTime(ha));
    }

    int getUtcOffset() override
    {
        return EEPROMStore::getUtcOffset();
    }

    void storeUtcOffset(int utcOffset) override
    {
        EEPROMStore::storeUtcOffset(utcOffset);
    }

    float getRAStepsPerDegree() override
    {
        return EEPROMStore::getRAStepsPerDegree();
    }

    void storeRAStepsPerDegree(float stepsPerDegree) override
    {
        EEPROMStore::storeRAStepsPerDegree(stepsPerDegree);
    }

    float getDECStepsPerDegree() override
    {
        return EEPROMStore::getDECStepsPerDegree();
    }

    void storeDECStepsPerDegree(float stepsPerDegree) override
    {
        EEPROMStore::storeDECStepsPerDegree(stepsPerDegree);
    }

    float getAZStepsPerDegree() override
    {
        return EEPROMStore::getAZStepsPerDegree();
    }

    void storeAZStepsPerDegree(float stepsPerDegree) override
    {
        EEPROMStore::storeAZStepsPerDegree(stepsPerDegree);
    }

    float getALTStepsPerDegree() override
    {
        return EEPROMStore::getALTStepsPerDegree();
    }

    void storeALTStepsPerDegree(float stepsPerDegree) override
    {
        EEPROMStore::storeALTStepsPerDegree(stepsPerDegree);
    }

    float getSpeedFactor() override
    {
        return EEPROMStore::getSpeedFactor();
    }

    void storeSpeedFactor(float speedFactor) override
    {
        EEPROMStore::storeSpeedFactor(speedFactor);
    }

    int16_t getBacklashCorrectionSteps() override
    {
        return EEPROMStore::getBacklashCorrectionSteps();
    }

    void storeBacklashCorrectionSteps(int16_t steps) override
    {
        EEPROMStore::storeBacklashCorrectionSteps(steps);
    }

    core::Latitude getLatitude() override
    {
        return EEPROMStore::getLatitude();
    }

    void storeLatitude(const core::Latitude &latitude) override
    {
        EEPROMStore::storeLatitude(Latitude(latitude));
    }

    core::Longitude getLongitude() override
    {
        return EEPROMStore::getLongitude();
    }

    void storeLongitude(const core::Longitude &longitude) override
    {
        EEPROMStore::storeLongitude(Longitude(longitude));
    }

    float getPitchCalibrationAngle() override
    {
        return EEPROMStore::getPitchCalibrationAngle();
    }

    void storePitchCalibrationAngle(float angle) override
    {
        EEPROMStore::storePitchCalibrationAngle(angle);
    }

    float getRollCalibrationAngle() override
    {
        return EEPROMStore::getRollCalibrationAngle();
    }

    void storeRollCalibrationAngle(float angle) override
    {
        EEPROMStore::storeRollCalibrationAngle(angle);
    }

    float getDECLowerLimit() override
    {
        return EEPROMStore::getDECLowerLimit();
    }

    void storeDECLowerLimit(float limit) override
    {
        EEPROMStore::storeDECLowerLimit(limit);
    }

    float getDECUpperLimit() override
    {
        return EEPROMStore::getDECUpperLimit();
    }

    void storeDECUpperLimit(float limit) override
    {
        EEPROMStore::storeDECUpperLimit(limit);
    }

    int32_t getRAHomingOffset() override
    {
        return EEPROMStore::getRAHomingOffset();
    }

    void storeRAHomingOffset(int32_t offset) override
    {
        EEPROMStore::storeRAHomingOffset(offset);
    }

    int32_t getDECHomingOffset() override
    {
        return EEPROMStore::getDECHomingOffset();
    }

    void storeDECHomingOffset(int32_t offset) override
    {
        EEPROMStore::storeDECHomingOffset(offset);
    }

    int16_t getLastFlashedVersion() override
    {
        return EEPROMStore::getLastFlashedVersion();
    }

    void storeLastFlashedVersion(int16_t version) override
    {
        EEPROMStore::storeLastFlashedVersion(version);
    }

    int32_t getAZPosition() override
    {
        return EEPROMStore::getAZPosition();
    }

    void storeAZPosition(int32_t position) override
    {
        EEPROMStore::storeAZPosition(position);
    }

    int32_t getALTPosition() override
    {
        return EEPROMStore::getALTPosition();
    }

    void storeALTPosition(int32_t position) override
    {
        EEPROMStore::storeALTPosition(position);
    }

    void clearConfiguration() override
    {
        EEPROMStore::clearConfiguration();
    }
};

}  // namespace adapters
