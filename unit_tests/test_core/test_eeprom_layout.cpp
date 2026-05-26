#include <gtest/gtest.h>
#include "core/EepromLayout.hpp"

using core::EepromLayout;

TEST(EepromLayoutTest, MagicMarkerValue)
{
    EXPECT_EQ(0xCE00, EepromLayout::MAGIC_MARKER_VALUE);
}

TEST(EepromLayoutTest, MagicMarkerMask)
{
    EXPECT_EQ(0xFE00, EepromLayout::MAGIC_MARKER_MASK);
}

TEST(EepromLayoutTest, FlagsMaskCoversAll)
{
    uint16_t allFlags = EepromLayout::RA_STEPS_FLAG | EepromLayout::DEC_STEPS_FLAG | EepromLayout::SPEED_FACTOR_FLAG
                        | EepromLayout::BACKLASH_STEPS_FLAG | EepromLayout::LATITUDE_FLAG | EepromLayout::LONGITUDE_FLAG
                        | EepromLayout::PITCH_OFFSET_FLAG | EepromLayout::ROLL_OFFSET_FLAG | EepromLayout::EXTENDED_FLAG;
    EXPECT_EQ(allFlags, allFlags & EepromLayout::FLAGS_MASK);
}

TEST(EepromLayoutTest, ItemFlagValues)
{
    EXPECT_EQ(0x0001, EepromLayout::RA_STEPS_FLAG);
    EXPECT_EQ(0x0002, EepromLayout::DEC_STEPS_FLAG);
    EXPECT_EQ(0x0004, EepromLayout::SPEED_FACTOR_FLAG);
    EXPECT_EQ(0x0008, EepromLayout::BACKLASH_STEPS_FLAG);
    EXPECT_EQ(0x0010, EepromLayout::LATITUDE_FLAG);
    EXPECT_EQ(0x0020, EepromLayout::LONGITUDE_FLAG);
    EXPECT_EQ(0x0040, EepromLayout::PITCH_OFFSET_FLAG);
    EXPECT_EQ(0x0080, EepromLayout::ROLL_OFFSET_FLAG);
    EXPECT_EQ(0x0100, EepromLayout::EXTENDED_FLAG);
}

TEST(EepromLayoutTest, ExtendedFlagValues)
{
    EXPECT_EQ(0x0001, EepromLayout::PARKING_POS_MARKER_FLAG);
    EXPECT_EQ(0x0002, EepromLayout::DEC_LIMIT_MARKER_FLAG);
    EXPECT_EQ(0x0004, EepromLayout::UTC_OFFSET_MARKER_FLAG);
    EXPECT_EQ(0x0008, EepromLayout::RA_HOMING_MARKER_FLAG);
}

TEST(EepromLayoutTest, NormalizedStepValue)
{
    EXPECT_FLOAT_EQ(25600.0f, EepromLayout::SteppingStorageNormalized);
}
