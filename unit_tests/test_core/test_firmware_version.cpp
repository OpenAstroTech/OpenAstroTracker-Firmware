#include <gtest/gtest.h>

#include "core/FirmwareVersion.hpp"

TEST(FirmwareVersionTest, ParsesTheReleasePleaseFormat)
{
    EXPECT_EQ(11322, core::firmwareVersionCode("v1.13.22"));
    EXPECT_EQ(11307, core::firmwareVersionCode("v1.13.7"));
}

TEST(FirmwareVersionTest, AcceptsAVersionWithoutThePrefix)
{
    EXPECT_EQ(11322, core::firmwareVersionCode("1.13.22"));
}

TEST(FirmwareVersionTest, EncodesEachComponentInItsOwnDigitField)
{
    EXPECT_EQ(20000, core::firmwareVersionCode("v2.0.0"));
    EXPECT_EQ(10100, core::firmwareVersionCode("v1.1.0"));
    EXPECT_EQ(10001, core::firmwareVersionCode("v1.0.1"));
}

TEST(FirmwareVersionTest, MissingComponentsCountAsZero)
{
    EXPECT_EQ(11300, core::firmwareVersionCode("v1.13"));
    EXPECT_EQ(10000, core::firmwareVersionCode("v1"));
}

TEST(FirmwareVersionTest, StopsAtSuffixesAndGarbage)
{
    EXPECT_EQ(11322, core::firmwareVersionCode("v1.13.22-beta"));
    EXPECT_EQ(0, core::firmwareVersionCode(""));
    EXPECT_EQ(0, core::firmwareVersionCode("v"));
    EXPECT_EQ(0, core::firmwareVersionCode(nullptr));
}

TEST(FirmwareVersionTest, TheMigrationThresholdIsWhereTheCodeSays)
{
    // readPersistentData() resets the AZ/ALT home positions for anything older than 1.13.7.
    EXPECT_LT(core::firmwareVersionCode("v1.13.6"), 11307);
    EXPECT_EQ(11307, core::firmwareVersionCode("v1.13.7"));
    EXPECT_GT(core::firmwareVersionCode("v1.14.0"), 11307);
}
