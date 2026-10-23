#include <gtest/gtest.h>

#include "core/DecLimitClamp.hpp"

TEST(DecLimitClampTest, NoLimitsLeavesTheTargetAlone)
{
    const core::DecLimitResult result = core::clampDecStepsToLimits(-5000.0f, 0, 0);

    EXPECT_FLOAT_EQ(-5000.0f, result.steps);
    EXPECT_FALSE(result.clampedToUpper);
    EXPECT_FALSE(result.clampedToLower);
}

TEST(DecLimitClampTest, TargetBetweenTheLimitsIsUntouched)
{
    const core::DecLimitResult result = core::clampDecStepsToLimits(250.0f, -1000, 1000);

    EXPECT_FLOAT_EQ(250.0f, result.steps);
    EXPECT_FALSE(result.clampedToUpper);
    EXPECT_FALSE(result.clampedToLower);
}

TEST(DecLimitClampTest, TargetPastTheUpperLimitIsPulledBack)
{
    const core::DecLimitResult result = core::clampDecStepsToLimits(1500.0f, -1000, 1000);

    EXPECT_FLOAT_EQ(1000.0f, result.steps);
    EXPECT_TRUE(result.clampedToUpper);
    EXPECT_FALSE(result.clampedToLower);
}

TEST(DecLimitClampTest, TargetPastTheLowerLimitIsPushedUp)
{
    const core::DecLimitResult result = core::clampDecStepsToLimits(-1500.0f, -1000, 1000);

    EXPECT_FLOAT_EQ(-1000.0f, result.steps);
    EXPECT_FALSE(result.clampedToUpper);
    EXPECT_TRUE(result.clampedToLower);
}

TEST(DecLimitClampTest, OnlyOneLimitConfiguredStillClamps)
{
    EXPECT_FLOAT_EQ(1000.0f, core::clampDecStepsToLimits(1500.0f, 0, 1000).steps);
    EXPECT_FLOAT_EQ(-1500.0f, core::clampDecStepsToLimits(-1500.0f, 0, 1000).steps);
    EXPECT_FLOAT_EQ(-1000.0f, core::clampDecStepsToLimits(-1500.0f, -1000, 0).steps);
    EXPECT_FLOAT_EQ(1500.0f, core::clampDecStepsToLimits(1500.0f, -1000, 0).steps);
}

TEST(DecLimitClampTest, ALimitOfZeroMeansNoLimit)
{
    // An uncalibrated mount must still be movable past the pole.
    const core::DecLimitResult result = core::clampDecStepsToLimits(-99999.0f, 0, 0);

    EXPECT_FLOAT_EQ(-99999.0f, result.steps);
}
