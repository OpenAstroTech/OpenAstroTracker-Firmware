#pragma once

#include <stdint.h>

namespace core
{

/// Pure EEPROM layout constants (no Arduino deps).
/// Mirrors the enum definitions in EPROMStore.hpp for validation and testing.
struct EepromLayout {
    // Magic marker values
    static constexpr uint16_t MAGIC_MARKER_VALUE = 0xCE00;
    static constexpr uint16_t MAGIC_MARKER_MASK  = 0xFE00;

    // Storage size (bytes)
    static constexpr int STORE_SIZE = 512;  // Atmel: no explicit size needed; ESP32: 512 bytes

    // Item flags (bitfield indicating which fields have been stored)
    enum ItemFlag : uint16_t
    {
        RA_STEPS_FLAG       = 0x0001,
        DEC_STEPS_FLAG      = 0x0002,
        SPEED_FACTOR_FLAG   = 0x0004,
        BACKLASH_STEPS_FLAG = 0x0008,
        LATITUDE_FLAG       = 0x0010,
        LONGITUDE_FLAG      = 0x0020,
        PITCH_OFFSET_FLAG   = 0x0040,
        ROLL_OFFSET_FLAG    = 0x0080,
        EXTENDED_FLAG       = 0x0100,
        FLAGS_MASK          = 0x01FF
    };

    // Extended item flags
    enum ExtendedItemFlag : uint16_t
    {
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
    };

    // Normalization factor for steps-per-degree storage
    static constexpr float SteppingStorageNormalized = 25600.0;
};

}  // namespace core
