#pragma once

#include "adapters/EepromSettingsFormat.hpp"
#include "ports/IMountSettings.hpp"

namespace adapters
{
/// `ports::IMountSettings` over `EepromSettingsFormat`.
///
/// The typed accessors and the on-device encoding stay in `EepromSettingsFormat`: it is the single writer
/// of that format. This adapter gives the domain an injectable handle, so `Mount` (and the
/// controllers) never name the store and a test can substitute the port.
///
/// Header-only and every method a single forward, so an optimising build collapses it to the same
/// calls a direct store use would make.
class MountSettingsStore final : public ports::IMountSettings
{
  public:
    explicit MountSettingsStore(EepromSettingsFormat &store) : _store(store)
    {
    }

    core::DayTime getHATime() override
    {
        return _store.getHATime();
    }

    void storeHATime(const core::DayTime &ha) override
    {
        _store.storeHATime(DayTime(ha));
    }

    int getUtcOffset() override
    {
        return _store.getUtcOffset();
    }

    void storeUtcOffset(int utcOffset) override
    {
        _store.storeUtcOffset(utcOffset);
    }

    uint8_t getBrightness() override
    {
        return _store.getBrightness();
    }

    void storeBrightness(uint8_t brightness) override
    {
        _store.storeBrightness(brightness);
    }

    float getRAStepsPerDegree() override
    {
        return _store.getRAStepsPerDegree();
    }

    void storeRAStepsPerDegree(float stepsPerDegree) override
    {
        _store.storeRAStepsPerDegree(stepsPerDegree);
    }

    float getDECStepsPerDegree() override
    {
        return _store.getDECStepsPerDegree();
    }

    void storeDECStepsPerDegree(float stepsPerDegree) override
    {
        _store.storeDECStepsPerDegree(stepsPerDegree);
    }

    float getAZStepsPerDegree() override
    {
        return _store.getAZStepsPerDegree();
    }

    void storeAZStepsPerDegree(float stepsPerDegree) override
    {
        _store.storeAZStepsPerDegree(stepsPerDegree);
    }

    float getALTStepsPerDegree() override
    {
        return _store.getALTStepsPerDegree();
    }

    void storeALTStepsPerDegree(float stepsPerDegree) override
    {
        _store.storeALTStepsPerDegree(stepsPerDegree);
    }

    float getSpeedFactor() override
    {
        return _store.getSpeedFactor();
    }

    void storeSpeedFactor(float speedFactor) override
    {
        _store.storeSpeedFactor(speedFactor);
    }

    int16_t getBacklashCorrectionSteps() override
    {
        return _store.getBacklashCorrectionSteps();
    }

    void storeBacklashCorrectionSteps(int16_t steps) override
    {
        _store.storeBacklashCorrectionSteps(steps);
    }

    core::Latitude getLatitude() override
    {
        return _store.getLatitude();
    }

    void storeLatitude(const core::Latitude &latitude) override
    {
        _store.storeLatitude(Latitude(latitude));
    }

    core::Longitude getLongitude() override
    {
        return _store.getLongitude();
    }

    void storeLongitude(const core::Longitude &longitude) override
    {
        _store.storeLongitude(Longitude(longitude));
    }

    float getPitchCalibrationAngle() override
    {
        return _store.getPitchCalibrationAngle();
    }

    void storePitchCalibrationAngle(float angle) override
    {
        _store.storePitchCalibrationAngle(angle);
    }

    float getRollCalibrationAngle() override
    {
        return _store.getRollCalibrationAngle();
    }

    void storeRollCalibrationAngle(float angle) override
    {
        _store.storeRollCalibrationAngle(angle);
    }

    float getDECLowerLimit() override
    {
        return _store.getDECLowerLimit();
    }

    void storeDECLowerLimit(float limit) override
    {
        _store.storeDECLowerLimit(limit);
    }

    float getDECUpperLimit() override
    {
        return _store.getDECUpperLimit();
    }

    void storeDECUpperLimit(float limit) override
    {
        _store.storeDECUpperLimit(limit);
    }

    int32_t getRAHomingOffset() override
    {
        return _store.getRAHomingOffset();
    }

    void storeRAHomingOffset(int32_t offset) override
    {
        _store.storeRAHomingOffset(offset);
    }

    int32_t getDECHomingOffset() override
    {
        return _store.getDECHomingOffset();
    }

    void storeDECHomingOffset(int32_t offset) override
    {
        _store.storeDECHomingOffset(offset);
    }

    int16_t getLastFlashedVersion() override
    {
        return _store.getLastFlashedVersion();
    }

    void storeLastFlashedVersion(int16_t version) override
    {
        _store.storeLastFlashedVersion(version);
    }

    int32_t getAZPosition() override
    {
        return _store.getAZPosition();
    }

    void storeAZPosition(int32_t position) override
    {
        _store.storeAZPosition(position);
    }

    int32_t getALTPosition() override
    {
        return _store.getALTPosition();
    }

    void storeALTPosition(int32_t position) override
    {
        _store.storeALTPosition(position);
    }

    void clearConfiguration() override
    {
        _store.clearConfiguration();
    }

  private:
    EepromSettingsFormat &_store;
};

}  // namespace adapters
