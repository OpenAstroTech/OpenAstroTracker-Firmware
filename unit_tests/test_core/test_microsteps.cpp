#include <gtest/gtest.h>

#include "core/Microsteps.hpp"

TEST(MicrostepsTest, OneXIsEncodedAsZero)
{
    // The TMC2209 MS register has no "1" code; 0 means 1x. Writing 1 would select a
    // microstep count the driver does not have.
    EXPECT_EQ(0, core::microstepRegisterValue(1));
}

TEST(MicrostepsTest, EveryOtherCountIsWrittenVerbatim)
{
    EXPECT_EQ(2, core::microstepRegisterValue(2));
    EXPECT_EQ(4, core::microstepRegisterValue(4));
    EXPECT_EQ(8, core::microstepRegisterValue(8));
    EXPECT_EQ(16, core::microstepRegisterValue(16));
    EXPECT_EQ(32, core::microstepRegisterValue(32));
    EXPECT_EQ(64, core::microstepRegisterValue(64));
    EXPECT_EQ(128, core::microstepRegisterValue(128));
    EXPECT_EQ(256, core::microstepRegisterValue(256));
}

TEST(MicrostepsTest, TheShippedSettingsMapToTheExpectedRegisterValues)
{
    // RA tracks at 256 and slews at 8 on the UART boards.
    EXPECT_EQ(256, core::microstepRegisterValue(256));
    EXPECT_EQ(8, core::microstepRegisterValue(8));
}

TEST(MicrostepsTest, IsConstexprSoItCanBeUsedInAConfigureCall)
{
    constexpr int value = core::microstepRegisterValue(1);
    static_assert(value == 0, "1x must be a compile-time zero");
    EXPECT_EQ(0, value);
}
