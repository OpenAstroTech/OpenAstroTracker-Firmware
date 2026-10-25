#pragma once

#include "core/types/DayTime.hpp"
#include "core/types/Latitude.hpp"
#include "core/types/Longitude.hpp"
#include "hal/EepromLayout.hpp"
#include "ports/ILogger.hpp"
#include "ports/IPersistentStore.hpp"

namespace adapters
{

/// The on-device settings format: what each persisted value means and where it lives.
///
/// Not a storage backend — the uint8_t medium arrives as a `ports::IPersistentStore`, because the ESP
/// boards want a size up front and an explicit commit while the AVR boards do not. The log sink is
/// injected too, so the class holds no global state: one instance per firmware, built by the
/// composition root.
///
/// The layout itself (magic markers, flags, addresses) lives in `hal/EepromLayout.hpp` so it can be
/// pinned by a test without touching a single uint8_t of storage.
class EepromSettingsFormat
{
  public:
    EepromSettingsFormat(ports::IPersistentStore &persistent, ports::ILogger &logger);

    void initialize();
    void clearConfiguration();

    core::DayTime getHATime();
    void storeHATime(const core::DayTime &ha);

    int getUtcOffset();
    void storeUtcOffset(int utcOffset);

    uint8_t getBrightness();
    void storeBrightness(uint8_t brightness);

    float getRAStepsPerDegree();
    void storeRAStepsPerDegree(float raStepsPerDegree);

    float getDECStepsPerDegree();
    void storeDECStepsPerDegree(float decStepsPerDegree);

    float getAZStepsPerDegree();
    void storeAZStepsPerDegree(float azStepsPerDegree);

    float getALTStepsPerDegree();
    void storeALTStepsPerDegree(float altStepsPerDegree);

    float getSpeedFactor();
    void storeSpeedFactor(float speed);

    int16_t getBacklashCorrectionSteps();
    void storeBacklashCorrectionSteps(int16_t backlashCorrectionSteps);

    core::Latitude getLatitude();
    void storeLatitude(const core::Latitude &latitude);

    core::Longitude getLongitude();
    void storeLongitude(const core::Longitude &longitude);

    float getPitchCalibrationAngle();
    void storePitchCalibrationAngle(float pitchCalibrationAngle);

    float getRollCalibrationAngle();
    void storeRollCalibrationAngle(float rollCalibrationAngle);

    float getDECLowerLimit();
    void storeDECLowerLimit(float decLowerLimit);

    float getDECUpperLimit();
    void storeDECUpperLimit(float decUpperLimit);

    int32_t getRAHomingOffset();
    void storeRAHomingOffset(int32_t raHomingOffset);

    int32_t getDECHomingOffset();
    void storeDECHomingOffset(int32_t decHomingOffset);

    int16_t getLastFlashedVersion();
    void storeLastFlashedVersion(int16_t lastVersion);

    int32_t getAZPosition();
    void storeAZPosition(int32_t azPosition);

    int32_t getALTPosition();
    void storeALTPosition(int32_t altPosition);

  private:
    // The storage format itself (magic markers, flags, addresses) lives in
    // hal::eeprom (src/hal/EepromLayout.hpp) so it can be tested without any
    // Arduino dependencies.
    using ItemFlag         = hal::eeprom::ItemFlag;
    using ExtendedItemFlag = hal::eeprom::ExtendedItemFlag;
    using ItemAddress      = hal::eeprom::ItemAddress;

    // Helper functions
    void displayContents();

    bool isPresent(ItemFlag item);
    void updateFlags(ItemFlag item);

    bool isPresentExtended(ExtendedItemFlag item);
    void updateFlagsExtended(ExtendedItemFlag item);

    void updateUint8(ItemAddress location, uint8_t value);
    uint8_t readUint8(ItemAddress location);

    void updateInt8(ItemAddress location, int8_t value);
    int8_t readInt8(ItemAddress location);

    void updateUint16(ItemAddress location, uint16_t value);
    uint16_t readUint16(ItemAddress location);

    void updateInt16(ItemAddress location, int16_t value);
    int16_t readInt16(ItemAddress location);

    void updateInt32(ItemAddress location, int32_t value);
    int32_t readInt32(ItemAddress location);

    // A new store must implement these functions
    uint8_t read(uint8_t location);
    void update(uint8_t location, uint8_t value);
    void commit();

    ports::IPersistentStore &_persistent;
    /// The log sink, held by reference because the store never outlives it. `OAT_LOG` takes a
    /// pointer, hence the `&_log` at the call sites below.
    ports::ILogger &_log;
};

}  // namespace adapters
