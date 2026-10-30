#pragma once

#include "types/MathUtil.hpp"
#include "types/DayTime.hpp"
#include "types/Latitude.hpp"
#include "types/Longitude.hpp"
#include "hal/EepromLayout.hpp"
#include "ports/IMountSettings.hpp"
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
class EepromSettingsFormat final : public ports::IMountSettings
{
  public:
    /// The uncalibrated values a fresh store answers with, and the microstep settings the stored
    /// numbers are normalised against.
    ///
    /// They are board facts, so they arrive from the composition root instead of being read from
    /// the configuration here: that is what lets this class be tested on the host.
    struct AxisDefaults {
        float raStepsPerDegree;
        float decStepsPerDegree;
        float azStepsPerDegree;
        float altStepsPerDegree;
        int raSlewMicrosteps;
        int decSlewMicrosteps;
        int azMicrosteps;
        int altMicrosteps;
        int16_t backlashSteps;
    };

    EepromSettingsFormat(ports::IPersistentStore &persistent, ports::ILogger &logger, const AxisDefaults &defaults);

    void initialize();
    void clearConfiguration() override;

    types::DayTime getHATime() override;
    void storeHATime(const types::DayTime &ha) override;

    int getUtcOffset() override;
    void storeUtcOffset(int utcOffset) override;

    uint8_t getBrightness() override;
    void storeBrightness(uint8_t brightness) override;

    float getRAStepsPerDegree() override;
    void storeRAStepsPerDegree(float raStepsPerDegree) override;

    float getDECStepsPerDegree() override;
    void storeDECStepsPerDegree(float decStepsPerDegree) override;

    float getAZStepsPerDegree() override;
    void storeAZStepsPerDegree(float azStepsPerDegree) override;

    float getALTStepsPerDegree() override;
    void storeALTStepsPerDegree(float altStepsPerDegree) override;

    float getSpeedFactor() override;
    void storeSpeedFactor(float speed) override;

    int16_t getBacklashCorrectionSteps() override;
    void storeBacklashCorrectionSteps(int16_t backlashCorrectionSteps) override;

    types::Latitude getLatitude() override;
    void storeLatitude(const types::Latitude &latitude) override;

    types::Longitude getLongitude() override;
    void storeLongitude(const types::Longitude &longitude) override;

    float getPitchCalibrationAngle() override;
    void storePitchCalibrationAngle(float pitchCalibrationAngle) override;

    float getRollCalibrationAngle() override;
    void storeRollCalibrationAngle(float rollCalibrationAngle) override;

    float getDECLowerLimit() override;
    void storeDECLowerLimit(float decLowerLimit) override;

    float getDECUpperLimit() override;
    void storeDECUpperLimit(float decUpperLimit) override;

    int32_t getRAHomingOffset() override;
    void storeRAHomingOffset(int32_t raHomingOffset) override;

    int32_t getDECHomingOffset() override;
    void storeDECHomingOffset(int32_t decHomingOffset) override;

    int16_t getLastFlashedVersion() override;
    void storeLastFlashedVersion(int16_t lastVersion) override;

    int32_t getAZPosition() override;
    void storeAZPosition(int32_t azPosition) override;

    int32_t getALTPosition() override;
    void storeALTPosition(int32_t altPosition) override;

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

    /// Owned by the composition root for the firmware's lifetime: holding a reference keeps the
    /// twenty-six bytes of defaults out of every settings object on the 8 KB part.
    const AxisDefaults &_defaults;

    ports::IPersistentStore &_persistent;
    /// The log sink, held by reference because the store never outlives it. `OAT_LOG` takes a
    /// pointer, hence the `&_log` at the call sites below.
    ports::ILogger &_log;
};

}  // namespace adapters
