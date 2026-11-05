#pragma once

#include <stdint.h>

namespace hal
{

/// On-device EEPROM layout: magic markers, item flags and item addresses.
///
/// This header describes a storage format, not domain behaviour, and has no
/// Arduino or firmware dependencies so it can be unit-tested on the host.
/// EEPROMStore is the only reader/writer of this layout today; Phase 2 wraps
/// it in an IPersistentStore port + adapter.
///
/// EEPROM storage location 5 ("magic marker") must be 0xCE or 0xCF for the
/// mount to read any data. The lowest bit of location 5 indicates whether the
/// extended flags are available in (21/22).
///
/// Location 4 indicates what has been stored so far: 0000 0000
///                                                ^^^^ ^^^^
///                                                |||| ||||
///                 Roll angle offset (19/20) -----+||| ||||
///                Pitch angle offset (17/18) ------+|| ||||
///                         Longitude (14/15) -------+| ||||
///                          Latitude (12/13) --------+ ||||
///                    Backlash steps (10/11) ----------+|||
///                        Speed factor (0/3) -----------+||
///  DEC stepper motor steps per degree (8/9) ------------+|
///   RA stepper motor steps per degree (6/7) -------------+
///
/// Locations for HATime (1/2) and Brightness (16).
///
/// If location 5 is 0xCF, then an extended 16-bit flag is stored in 21/22 and
/// indicates the additional fields that have been stored:
/// 0000 0000 0000 0000
///  ^^^^ ^^^^ ^^^^ ^^^^
///       |||| |||| ||||
///  ALT Steps/deg, normalized to 256MS (70-73) -------------+||| |||| ||||
///   AZ Steps/deg, normalized to 256MS (66-69) --------------+|| |||| ||||
///                        ALT position (62-65) ---------------+| |||| ||||
///                         AZ Position (58-61) ----------------+ |||| ||||
///                Last flashed version (56-57) ------------------+||| ||||
///                    DEC Homing Offset (52-55) -------------------+|| ||||
///  DEC Steps/deg, normalized to 256MS (48-51) --------------------+| ||||
///   RA Steps/deg, normalized to 256MS (44-47) ---------------------+ ||||
///                     RA Homing Offset (40-43) -----------------------+|||
///                             UTC Offset (39) ------------------------+||
///  DEC lower (31-34) and upper (35-38) limits -------------------------+|
///  RA (23-26) and DEC (27-30) Parking offsets --------------------------+  (==== Obsolete V1.13.0 and beyond ====)
namespace eeprom
{
enum ItemFlag : uint16_t
{
    MAGIC_MARKER_VALUE = 0xCE00,  // Changed to 0xCxxx in V1.8.60 since we changed RA and DEC Steps to be 10x (previous settings ignored)
    MAGIC_MARKER_MASK  = 0xFE00,  // If these bits are set to MAGIC_MARKER_VALUE, something has been written to the EEPROM

    // The marker bits for the first 8 values stored in EEPROM.
    RA_STEPS_FLAG       = 0x0001,
    DEC_STEPS_FLAG      = 0x0002,
    SPEED_FACTOR_FLAG   = 0x0004,
    BACKLASH_STEPS_FLAG = 0x0008,
    LATITUDE_FLAG       = 0x0010,
    LONGITUDE_FLAG      = 0x0020,
    PITCH_OFFSET_FLAG   = 0x0040,
    ROLL_OFFSET_FLAG    = 0x0080,
    EXTENDED_FLAG       = 0x0100,
    FLAGS_MASK          = 0x01FF  // The bitwise OR of all preceding values
};

enum ExtendedItemFlag : uint16_t
{
    // The marker bits for the extended values
    PARKING_POS_MARKER_FLAG    = 0x0001,
    DEC_LIMIT_MARKER_FLAG      = 0x0002,
    UTC_OFFSET_MARKER_FLAG     = 0x0004,
    RA_HOMING_MARKER_FLAG      = 0x0008,
    RA_NORM_STEPS_MARKER_FLAG  = 0x0010,
    DEC_NORM_STEPS_MARKER_FLAG = 0x0020,
    DEC_HOMING_MARKER_FLAG     = 0x0040,
    LAST_FLASHED_MARKER_FLAG   = 0x0080,
    AZ_POSITION_MARKER_FLAG    = 0x0100,
    ALT_POSITION_MARKER_FLAG   = 0x0200,
    AZ_NORM_STEPS_MARKER_FLAG  = 0x0400,
    ALT_NORM_STEPS_MARKER_FLAG = 0x0800,
    EXTENDED_FLAGS_MASK        = 0x0FFF  // The bitwise OR of all preceding values
};

// These are the offsets to each item stored in the EEPROM
enum ItemAddress : uint8_t
{
    SPEED_FACTOR_LOW_ADDR         = 0,
    HA_HOUR_ADDR                  = 1,
    HA_MINUTE_ADDR                = 2,  // Both Uint8
    SPEED_FACTOR_HIGH_ADDR        = 3,  // Split as two discontinuous Uint8
    FLAGS_ADDR                    = 4,  // Uint8
    MAGIC_MARKER_ADDR             = 5,  // Uint8
    MAGIC_MARKER_AND_FLAGS_ADDR   = 4,  // Alias for Uint16 access
    RA_STEPS_DEGREE_ADDR          = 6,
    _RA_STEPS_DEGREE_ADDR_1       = 7,  // Int16
    DEC_STEPS_DEGREE_ADDR         = 8,
    _DEC_STEPS_DEGREE_ADDR_1      = 9,  // Int16
    BACKLASH_STEPS_ADDR           = 10,
    _BACKLASH_STEPS_ADDR_1        = 11,  // Int16
    LATITUDE_ADDR                 = 12,
    _LATITUDE_ADDR_1              = 13,  // Int16
    LONGITUDE_ADDR                = 14,
    _LONGITUDE_ADDR_1             = 15,  // Int16
    LCD_BRIGHTNESS_ADDR           = 16,  // Uint8
    PITCH_OFFSET_ADDR             = 17,
    _PITCH_OFFSET_ADDR_1          = 18,  // Uint16
    ROLL_OFFSET_ADDR              = 19,
    _ROLL_OFFSET_ADDR_1           = 20,  // Uint16
    EXTENDED_FLAGS_ADDR           = 21,
    _EXTENDED_FLAGS_ADDR_1        = 22,  // Uint16
    RA_PARKING_POS_ADDR           = 23,
    _RA_PARKING_POS_ADDR_1        = 24,
    _RA_PARKING_POS_ADDR_2        = 25,
    _RA_PARKING_POS_ADDR_3        = 26,  // Int32
    DEC_PARKING_POS_ADDR          = 27,
    _DEC_PARKING_POS_ADDR_1       = 28,
    _DEC_PARKING_POS_ADDR_2       = 29,
    _DEC_PARKING_POS_ADDR_3       = 30,  // Int32
    DEC_LOWER_LIMIT_ADDR          = 31,
    _DEC_LOWER_LIMIT_ADDR_1       = 32,
    _DEC_LOWER_LIMIT_ADDR_2       = 33,
    _DEC_LOWER_LIMIT_ADDR_3       = 34,  // Int32
    DEC_UPPER_LIMIT_ADDR          = 35,
    _DEC_UPPER_LIMIT_ADDR_1       = 36,
    _DEC_UPPER_LIMIT_ADDR_2       = 37,
    _DEC_UPPER_LIMIT_ADDR_3       = 38,  // Int32
    UTC_OFFSET_ADDR               = 39,  // Int8
    RA_HOMING_OFFSET_ADDR         = 40,
    _RA_HOMING_OFFSET_ADDR_1      = 41,
    _RA_HOMING_OFFSET_ADDR_2      = 42,
    _RA_HOMING_OFFSET_ADDR_3      = 43,  // Int32
    RA_NORM_STEPS_DEGREE_ADDR     = 44,
    _RA_NORM_STEPS_DEGREE_ADDR_1  = 45,
    _RA_NORM_STEPS_DEGREE_ADDR_2  = 46,
    _RA_NORM_STEPS_DEGREE_ADDR_3  = 47,  // Int32
    DEC_NORM_STEPS_DEGREE_ADDR    = 48,
    _DEC_NORM_STEPS_DEGREE_ADDR_1 = 49,
    _DEC_NORM_STEPS_DEGREE_ADDR_2 = 50,
    _DEC_NORM_STEPS_DEGREE_ADDR_3 = 51,  // Int32
    DEC_HOMING_OFFSET_ADDR        = 52,
    _DEC_HOMING_OFFSET_ADDR_1     = 53,
    _DEC_HOMING_OFFSET_ADDR_2     = 54,
    _DEC_HOMING_OFFSET_ADDR_3     = 55,  // Int32
    LAST_FLASHED_VERSION          = 56,
    _LAST_FLASHED_VERSION_1       = 57,
    AZ_POSITION_ADDR              = 58,
    _AZ_POSITION_ADDR_1           = 59,
    _AZ_POSITION_ADDR_2           = 60,
    _AZ_POSITION_ADDR_3           = 61,
    ALT_POSITION_ADDR             = 62,
    _ALT_POSITION_ADDR_1          = 63,
    _ALT_POSITION_ADDR_2          = 64,
    _ALT_POSITION_ADDR_3          = 65,
    AZ_NORM_STEPS_DEGREE_ADDR     = 66,
    _AZ_NORM_STEPS_DEGREE_ADDR_1  = 67,
    _AZ_NORM_STEPS_DEGREE_ADDR_2  = 68,
    _AZ_NORM_STEPS_DEGREE_ADDR_3  = 69,  // Int32
    ALT_NORM_STEPS_DEGREE_ADDR    = 70,
    _ALT_NORM_STEPS_DEGREE_ADDR_1 = 71,
    _ALT_NORM_STEPS_DEGREE_ADDR_2 = 72,
    _ALT_NORM_STEPS_DEGREE_ADDR_3 = 73,  // Int32
    STORE_SIZE                    = 74
};

// Steps/deg are normalized to this value and stored.
constexpr float SteppingStorageNormalized = 25600.0f;
}  // namespace eeprom
}  // namespace hal
