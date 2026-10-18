#include <gtest/gtest.h>

#include "hal/EepromLayout.hpp"

namespace L = hal::eeprom;

// The EEPROM layout is a persistent on-device format: these tests pin it so an
// accidental reorder or renumbering is caught on the host instead of on a mount
// that has already been calibrated.

static void expectFourByteField(int base, int b1, int b2, int b3)
{
    EXPECT_EQ(base + 1, b1);
    EXPECT_EQ(base + 2, b2);
    EXPECT_EQ(base + 3, b3);
}

static void expectTwoByteField(int base, int b1)
{
    EXPECT_EQ(base + 1, b1);
}

TEST(EepromLayoutTest, MagicMarkerIsEncodedInTheHighByte)
{
    EXPECT_EQ(0xCE00, L::MAGIC_MARKER_VALUE);
    EXPECT_EQ(0xFE00, L::MAGIC_MARKER_MASK);
    // The mask keeps the marker bits and drops the extended-flag bit.
    EXPECT_EQ(L::MAGIC_MARKER_VALUE, L::MAGIC_MARKER_VALUE & L::MAGIC_MARKER_MASK);
}

TEST(EepromLayoutTest, ExtendedMarkerStillMatchesTheMask)
{
    // 0xCF marks "extended flags present"; it must still read as present.
    EXPECT_EQ(L::MAGIC_MARKER_VALUE, 0xCF00 & L::MAGIC_MARKER_MASK);
    // Anything outside 0xCE/0xCF is "nothing stored".
    EXPECT_NE(L::MAGIC_MARKER_VALUE, 0x0000 & L::MAGIC_MARKER_MASK);
    EXPECT_NE(L::MAGIC_MARKER_VALUE, 0xCD00 & L::MAGIC_MARKER_MASK);
}

TEST(EepromLayoutTest, ItemFlagsFillTheFlagsMask)
{
    uint16_t all = L::RA_STEPS_FLAG | L::DEC_STEPS_FLAG | L::SPEED_FACTOR_FLAG | L::BACKLASH_STEPS_FLAG | L::LATITUDE_FLAG
                   | L::LONGITUDE_FLAG | L::PITCH_OFFSET_FLAG | L::ROLL_OFFSET_FLAG | L::EXTENDED_FLAG;

    EXPECT_EQ(static_cast<uint16_t>(L::FLAGS_MASK), all);
    // Every flag is a distinct single bit, so the OR and the sum agree.
    EXPECT_EQ(static_cast<int>(all),
              L::RA_STEPS_FLAG + L::DEC_STEPS_FLAG + L::SPEED_FACTOR_FLAG + L::BACKLASH_STEPS_FLAG + L::LATITUDE_FLAG + L::LONGITUDE_FLAG
                  + L::PITCH_OFFSET_FLAG + L::ROLL_OFFSET_FLAG + L::EXTENDED_FLAG);
}

TEST(EepromLayoutTest, ExtendedItemFlagsFillTheExtendedMask)
{
    uint16_t all = L::PARKING_POS_MARKER_FLAG | L::DEC_LIMIT_MARKER_FLAG | L::UTC_OFFSET_MARKER_FLAG | L::RA_HOMING_MARKER_FLAG
                   | L::RA_NORM_STEPS_MARKER_FLAG | L::DEC_NORM_STEPS_MARKER_FLAG | L::DEC_HOMING_MARKER_FLAG | L::LAST_FLASHED_MARKER_FLAG
                   | L::AZ_POSITION_MARKER_FLAG | L::ALT_POSITION_MARKER_FLAG | L::AZ_NORM_STEPS_MARKER_FLAG
                   | L::ALT_NORM_STEPS_MARKER_FLAG;

    EXPECT_EQ(static_cast<uint16_t>(L::EXTENDED_FLAGS_MASK), all);
    EXPECT_EQ(static_cast<int>(all),
              L::PARKING_POS_MARKER_FLAG + L::DEC_LIMIT_MARKER_FLAG + L::UTC_OFFSET_MARKER_FLAG + L::RA_HOMING_MARKER_FLAG
                  + L::RA_NORM_STEPS_MARKER_FLAG + L::DEC_NORM_STEPS_MARKER_FLAG + L::DEC_HOMING_MARKER_FLAG + L::LAST_FLASHED_MARKER_FLAG
                  + L::AZ_POSITION_MARKER_FLAG + L::ALT_POSITION_MARKER_FLAG + L::AZ_NORM_STEPS_MARKER_FLAG
                  + L::ALT_NORM_STEPS_MARKER_FLAG);
}

TEST(EepromLayoutTest, LegacyAddressesAreUnchanged)
{
    // The V1.x layout that every mount's stored calibration was written with.
    EXPECT_EQ(0, L::SPEED_FACTOR_LOW_ADDR);
    EXPECT_EQ(1, L::HA_HOUR_ADDR);
    EXPECT_EQ(2, L::HA_MINUTE_ADDR);
    EXPECT_EQ(3, L::SPEED_FACTOR_HIGH_ADDR);
    EXPECT_EQ(4, L::FLAGS_ADDR);
    EXPECT_EQ(5, L::MAGIC_MARKER_ADDR);
    EXPECT_EQ(4, L::MAGIC_MARKER_AND_FLAGS_ADDR);
    EXPECT_EQ(6, L::RA_STEPS_DEGREE_ADDR);
    EXPECT_EQ(8, L::DEC_STEPS_DEGREE_ADDR);
    EXPECT_EQ(10, L::BACKLASH_STEPS_ADDR);
    EXPECT_EQ(12, L::LATITUDE_ADDR);
    EXPECT_EQ(14, L::LONGITUDE_ADDR);
    EXPECT_EQ(16, L::LCD_BRIGHTNESS_ADDR);
    EXPECT_EQ(17, L::PITCH_OFFSET_ADDR);
    EXPECT_EQ(19, L::ROLL_OFFSET_ADDR);
    EXPECT_EQ(21, L::EXTENDED_FLAGS_ADDR);
}

TEST(EepromLayoutTest, ExtendedAddressesAreUnchanged)
{
    EXPECT_EQ(23, L::RA_PARKING_POS_ADDR);
    EXPECT_EQ(27, L::DEC_PARKING_POS_ADDR);
    EXPECT_EQ(31, L::DEC_LOWER_LIMIT_ADDR);
    EXPECT_EQ(35, L::DEC_UPPER_LIMIT_ADDR);
    EXPECT_EQ(39, L::UTC_OFFSET_ADDR);
    EXPECT_EQ(40, L::RA_HOMING_OFFSET_ADDR);
    EXPECT_EQ(44, L::RA_NORM_STEPS_DEGREE_ADDR);
    EXPECT_EQ(48, L::DEC_NORM_STEPS_DEGREE_ADDR);
    EXPECT_EQ(52, L::DEC_HOMING_OFFSET_ADDR);
    EXPECT_EQ(56, L::LAST_FLASHED_VERSION);
    EXPECT_EQ(58, L::AZ_POSITION_ADDR);
    EXPECT_EQ(62, L::ALT_POSITION_ADDR);
    EXPECT_EQ(66, L::AZ_NORM_STEPS_DEGREE_ADDR);
    EXPECT_EQ(70, L::ALT_NORM_STEPS_DEGREE_ADDR);
}

TEST(EepromLayoutTest, MultiByteFieldsAreContiguous)
{
    expectTwoByteField(L::RA_STEPS_DEGREE_ADDR, L::_RA_STEPS_DEGREE_ADDR_1);
    expectTwoByteField(L::DEC_STEPS_DEGREE_ADDR, L::_DEC_STEPS_DEGREE_ADDR_1);
    expectTwoByteField(L::BACKLASH_STEPS_ADDR, L::_BACKLASH_STEPS_ADDR_1);
    expectTwoByteField(L::LATITUDE_ADDR, L::_LATITUDE_ADDR_1);
    expectTwoByteField(L::LONGITUDE_ADDR, L::_LONGITUDE_ADDR_1);
    expectTwoByteField(L::PITCH_OFFSET_ADDR, L::_PITCH_OFFSET_ADDR_1);
    expectTwoByteField(L::ROLL_OFFSET_ADDR, L::_ROLL_OFFSET_ADDR_1);
    expectTwoByteField(L::EXTENDED_FLAGS_ADDR, L::_EXTENDED_FLAGS_ADDR_1);
    expectTwoByteField(L::LAST_FLASHED_VERSION, L::_LAST_FLASHED_VERSION_1);

    expectFourByteField(L::RA_PARKING_POS_ADDR, L::_RA_PARKING_POS_ADDR_1, L::_RA_PARKING_POS_ADDR_2, L::_RA_PARKING_POS_ADDR_3);
    expectFourByteField(L::DEC_PARKING_POS_ADDR, L::_DEC_PARKING_POS_ADDR_1, L::_DEC_PARKING_POS_ADDR_2, L::_DEC_PARKING_POS_ADDR_3);
    expectFourByteField(L::DEC_LOWER_LIMIT_ADDR, L::_DEC_LOWER_LIMIT_ADDR_1, L::_DEC_LOWER_LIMIT_ADDR_2, L::_DEC_LOWER_LIMIT_ADDR_3);
    expectFourByteField(L::DEC_UPPER_LIMIT_ADDR, L::_DEC_UPPER_LIMIT_ADDR_1, L::_DEC_UPPER_LIMIT_ADDR_2, L::_DEC_UPPER_LIMIT_ADDR_3);
    expectFourByteField(L::RA_HOMING_OFFSET_ADDR, L::_RA_HOMING_OFFSET_ADDR_1, L::_RA_HOMING_OFFSET_ADDR_2, L::_RA_HOMING_OFFSET_ADDR_3);
    expectFourByteField(
        L::RA_NORM_STEPS_DEGREE_ADDR, L::_RA_NORM_STEPS_DEGREE_ADDR_1, L::_RA_NORM_STEPS_DEGREE_ADDR_2, L::_RA_NORM_STEPS_DEGREE_ADDR_3);
    expectFourByteField(L::DEC_NORM_STEPS_DEGREE_ADDR,
                        L::_DEC_NORM_STEPS_DEGREE_ADDR_1,
                        L::_DEC_NORM_STEPS_DEGREE_ADDR_2,
                        L::_DEC_NORM_STEPS_DEGREE_ADDR_3);
    expectFourByteField(
        L::DEC_HOMING_OFFSET_ADDR, L::_DEC_HOMING_OFFSET_ADDR_1, L::_DEC_HOMING_OFFSET_ADDR_2, L::_DEC_HOMING_OFFSET_ADDR_3);
    expectFourByteField(L::AZ_POSITION_ADDR, L::_AZ_POSITION_ADDR_1, L::_AZ_POSITION_ADDR_2, L::_AZ_POSITION_ADDR_3);
    expectFourByteField(L::ALT_POSITION_ADDR, L::_ALT_POSITION_ADDR_1, L::_ALT_POSITION_ADDR_2, L::_ALT_POSITION_ADDR_3);
    expectFourByteField(
        L::AZ_NORM_STEPS_DEGREE_ADDR, L::_AZ_NORM_STEPS_DEGREE_ADDR_1, L::_AZ_NORM_STEPS_DEGREE_ADDR_2, L::_AZ_NORM_STEPS_DEGREE_ADDR_3);
    expectFourByteField(L::ALT_NORM_STEPS_DEGREE_ADDR,
                        L::_ALT_NORM_STEPS_DEGREE_ADDR_1,
                        L::_ALT_NORM_STEPS_DEGREE_ADDR_2,
                        L::_ALT_NORM_STEPS_DEGREE_ADDR_3);
}

TEST(EepromLayoutTest, StoreSizeCoversEveryField)
{
    EXPECT_EQ(74, L::STORE_SIZE);
    EXPECT_EQ(L::STORE_SIZE, L::_ALT_NORM_STEPS_DEGREE_ADDR_3 + 1);
    // Addresses must stay addressable with a single byte.
    EXPECT_LE(L::STORE_SIZE, 255);
}

TEST(EepromLayoutTest, SteppingStorageNormalizedIs256Microsteps)
{
    EXPECT_FLOAT_EQ(25600.0f, L::SteppingStorageNormalized);
}
