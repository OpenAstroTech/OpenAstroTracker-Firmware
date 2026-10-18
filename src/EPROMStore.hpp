#pragma once

#include "DayTime.hpp"
#include "Latitude.hpp"
#include "Longitude.hpp"
#include "hal/EepromLayout.hpp"

// Platform independant abstraction of the EEPROM storage capability of the boards.
// This is needed because the ESP boards require two things that the Arduino boards don't:
//  1) It wants to know how many bytes you want to use (at most)
//  2) It wants you to call a commit() function after a write() to actual persist the data.
class EEPROMStore
{
  public:
    static void initialize();
    static void clearConfiguration();

    static DayTime getHATime();
    static void storeHATime(DayTime const &ha);

    static int getUtcOffset();
    static void storeUtcOffset(int utcOffset);

    static byte getBrightness();
    static void storeBrightness(byte brightness);

    static float getRAStepsPerDegree();
    static void storeRAStepsPerDegree(float raStepsPerDegree);

    static float getDECStepsPerDegree();
    static void storeDECStepsPerDegree(float decStepsPerDegree);

    static float getAZStepsPerDegree();
    static void storeAZStepsPerDegree(float azStepsPerDegree);

    static float getALTStepsPerDegree();
    static void storeALTStepsPerDegree(float altStepsPerDegree);

    static float getSpeedFactor();
    static void storeSpeedFactor(float speed);

    static int16_t getBacklashCorrectionSteps();
    static void storeBacklashCorrectionSteps(int16_t backlashCorrectionSteps);

    static Latitude getLatitude();
    static void storeLatitude(Latitude const &latitude);

    static Longitude getLongitude();
    static void storeLongitude(Longitude const &longitude);

    static float getPitchCalibrationAngle();
    static void storePitchCalibrationAngle(float pitchCalibrationAngle);

    static float getRollCalibrationAngle();
    static void storeRollCalibrationAngle(float rollCalibrationAngle);

    static float getDECLowerLimit();
    static void storeDECLowerLimit(float decLowerLimit);

    static float getDECUpperLimit();
    static void storeDECUpperLimit(float decUpperLimit);

    static int32_t getRAHomingOffset();
    static void storeRAHomingOffset(int32_t raHomingOffset);

    static int32_t getDECHomingOffset();
    static void storeDECHomingOffset(int32_t decHomingOffset);

    static int16_t getLastFlashedVersion();
    static void storeLastFlashedVersion(int16_t lastVersion);

    static int32_t getAZPosition();
    static void storeAZPosition(int32_t azPosition);

    static int32_t getALTPosition();
    static void storeALTPosition(int32_t altPosition);

  private:
    // The storage format itself (magic markers, flags, addresses) lives in
    // hal::eeprom (src/hal/EepromLayout.hpp) so it can be tested without any
    // Arduino dependencies.
    using ItemFlag         = hal::eeprom::ItemFlag;
    using ExtendedItemFlag = hal::eeprom::ExtendedItemFlag;
    using ItemAddress      = hal::eeprom::ItemAddress;

    // Helper functions
    static void displayContents();

    static bool isPresent(ItemFlag item);
    static void updateFlags(ItemFlag item);

    static bool isPresentExtended(ExtendedItemFlag item);
    static void updateFlagsExtended(ExtendedItemFlag item);

    static void updateUint8(ItemAddress location, uint8_t value);
    static uint8_t readUint8(ItemAddress location);

    static void updateInt8(ItemAddress location, int8_t value);
    static int8_t readInt8(ItemAddress location);

    static void updateUint16(ItemAddress location, uint16_t value);
    static uint16_t readUint16(ItemAddress location);

    static void updateInt16(ItemAddress location, int16_t value);
    static int16_t readInt16(ItemAddress location);

    static void updateInt32(ItemAddress location, int32_t value);
    static int32_t readInt32(ItemAddress location);

    // A new store must implement these functions
    static uint8_t read(uint8_t location);
    static void update(uint8_t location, uint8_t value);
    static void commit();
};
