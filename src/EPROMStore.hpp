#pragma once

#include "DayTime.hpp"
#include "Latitude.hpp"
#include "Longitude.hpp"

// Platform independant abstraction of the EEPROM storage capability of the boards.
// This is needed because the ESP boards require two things that the Arduino boards don't:
//  1) It wants to know how many bytes you want to use (at most)
//  2) It wants you to call a commit() function after a write() to actual persist the data.
class EEPROMStore {

public:

  static void initialize();
  static void clearConfiguration();

  static DayTime getHATime();
  static void storeHATime(DayTime const& ha);

  static byte getBrightness();
  static void storeBrightness(byte brightness);

  static float getRAStepsPerDegree();
  static void storeRAStepsPerDegree(float raStepsPerDegree);

  static float getDECStepsPerDegree();
  static void storeDECStepsPerDegree(float decStepsPerDegree);

  static float getSpeedFactor();
  static void storeSpeedFactor(float speed);

  static int16_t getBacklashCorrectionSteps();
  static void storeBacklashCorrectionSteps(int16_t backlashCorrectionSteps);

  static Latitude getLatitude();
  static void storeLatitude(Latitude const& latitude);

  static Longitude getLongitude();
  static void storeLongitude(Longitude const& longitude);

  static float getPitchCalibrationAngle();
  static void storePitchCalibrationAngle(float pitchCalibrationAngle);

  static float getRollCalibrationAngle();
  static void storeRollCalibrationAngle(float rollCalibrationAngle);

  static int32_t getRAParkingPos();
  static void storeRAParkingPos(int32_t raParkingPos);

  static int32_t getDECParkingPos();
  static void storeDECParkingPos(int32_t decParkingPos);

  static int32_t getDECLowerLimit();
  static void storeDECLowerLimit(int32_t decLowerLimit);

  static int32_t getDECUpperLimit();
  static void storeDECUpperLimit(int32_t decUpperLimit);

private:

  /////////////////////////////////
  //
  // EEPROM storage location 5 ("magic marker") must be 0xCE or 0xCF for the mount to read any data.
  // The lowest bit of location 5 indicates if extended flags are available in (21/22).
  //
  // Location 4 indicates what has been stored so far: 0000 0000
  //                                                   ^^^^ ^^^^
  //                                                   |||| ||||
  //                    Roll angle offset (19/20) -----+||| ||||
  //                   Pitch angle offset (17/18) ------+|| ||||
  //                            Longitude (14/15) -------+| ||||
  //                             Latitude (12/13) --------+ ||||
  //                       Backlash steps (10/11) ----------+|||
  //                           Speed factor (0/3) -----------+||
  //     DEC stepper motor steps per degree (8/9) ------------+|
  //      RA stepper motor steps per degree (6/7) -------------+
  //
  // Locations for HATime (1/2), Brightness (16)
  //
  // If Location 5 is 0xCF, then an extended 16-bit flag is stored in 21/22 and 
  // indicates the additional fields that have been stored: 0000 0000 0000 0000
  //                                                        ^^^^ ^^^^ ^^^^ ^^^^
  //                                                                         ||
  //     DEC lower (31-34) and upper (35-38) limits -------------------------+|                    
  //     RA (23-26) and DEC (27-30) Parking offsets --------------------------+
  //
  /////////////////////////////////

  enum ItemFlag {
    MAGIC_MARKER_VALUE = 0xCE00,  // Changed to 0xCxxx in V1.8.60 since we changed RA and DEC Steps to be 10x (previous settings ignored)
    MAGIC_MARKER_MASK = 0xFE00,   // If these bits are set to MAGIC_MARKER_VALUE, something has been written to the EEPROM

    // The marker bits for the first 8 values stored in EEPROM.
    RA_STEPS_FLAG = 0x0001,
    DEC_STEPS_FLAG = 0x0002,
    SPEED_FACTOR_FLAG = 0x0004,
    BACKLASH_STEPS_FLAG = 0x0008,
    LATITUDE_FLAG = 0x0010,
    LONGITUDE_FLAG = 0x0020,
    PITCH_OFFSET_FLAG = 0x0040,
    ROLL_OFFSET_FLAG = 0x0080,
    EXTENDED_FLAG = 0x0100,
    FLAGS_MASK = 0x01FF   // The bitwise OR of all preceding values
  };

  enum ExtendedItemFlag {
    // The marker bits for the extended values
    PARKING_POS_MARKER_FLAG = 0x0001,
    DEC_LIMIT_MARKER_FLAG = 0x0002
  };

  // These are the offsets to each item stored in the EEPROM
  enum ItemAddress { 
    SPEED_FACTOR_LOW_ADDR=0, SPEED_FACTOR_HIGH_ADDR=3,  // Split as two discontinuous Uint8
    HA_HOUR_ADDR=1, HA_MINUTE_ADDR=2, // Both Uint8
    FLAGS_ADDR=4,  // Uint8
    MAGIC_MARKER_ADDR=5, // Uint8
    MAGIC_MARKER_AND_FLAGS_ADDR=4,   // Alias for Uint16 access
    RA_STEPS_DEGREE_ADDR=6, _RA_STEPS_DEGREE_ADDR_1=7,    // Int16
    DEC_STEPS_DEGREE_ADDR=8, _DEC_STEPS_DEGREE_ADDR_1=9,  // Int16
    BACKLASH_STEPS_ADDR=10, _BACKLASH_STEPS_ADDR_1=11,    // Int16
    LATITUDE_ADDR=12, _LATITUDE_ADDR_1=13,    // Int16
    LONGITUDE_ADDR=14, _LONGITUDE_ADDR_1=13,  // Int16
    LCD_BRIGHTNESS_ADDR=16, // Uint8
    PITCH_OFFSET_ADDR=17, _PITCH_OFFSET_ADDR_1=18,  // Uint16
    ROLL_OFFSET_ADDR=19, _ROLL_OFFSET_ADDR_1=20,    // Uint16
    EXTENDED_FLAGS_ADDR=21, _EXTENDED_FLAGS_ADDR_1=22,  // Uint16
    RA_PARKING_POS_ADDR=23, _RA_PARKING_POS_ADDR_1, _RA_PARKING_POS_ADDR_2, _RA_PARKING_POS_ADDR_3,     // Int32
    DEC_PARKING_POS_ADDR=27, _DEC_PARKING_POS_ADDR_1, _DEC_PARKING_POS_ADDR_2, _DEC_PARKING_POS_ADDR_3, // Int32
    DEC_LOWER_LIMIT_ADDR=31, _DEC_LOWER_LIMIT_ADDR_1, _DEC_LOWER_LIMIT_ADDR_2, _DEC_LOWER_LIMIT_ADDR_3, // Int32
    DEC_UPPER_LIMIT_ADDR=35, _DEC_UPPER_LIMIT_ADDR_1, _DEC_UPPER_LIMIT_ADDR_2, _DEC_UPPER_LIMIT_ADDR_3, // Int32
    STORE_SIZE=64   
  };

  // Helper functions
  static void displayContents();

  static bool isPresent(ItemFlag item);
  static void updateFlags(ItemFlag item);

  static bool isPresentExtended(ExtendedItemFlag item);
  static void updateFlagsExtended(ExtendedItemFlag item);

  static void updateUint8(ItemAddress location, uint8_t value);
  static uint8_t readUint8(ItemAddress location);

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

