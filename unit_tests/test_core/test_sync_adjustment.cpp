#include <gtest/gtest.h>

#include "core/SyncAdjustment.hpp"

TEST(SyncAdjustmentTest, RaCorrectionIsTheDifferenceWhenItIsShort)
{
    const core::SyncAdjustment adjustment = core::syncAdjustment(5.0f, 7.5f, 10.0f, 10.0f, 10.0f);

    EXPECT_FLOAT_EQ(2.5f, adjustment.raHours);
    EXPECT_FLOAT_EQ(0.0f, adjustment.decDegrees);
}

TEST(SyncAdjustmentTest, RaCorrectionWrapsTheShortWayAcrossZeroHours)
{
    // 23h30m -> 0h30m is one hour forward, not twenty-three back.
    const core::SyncAdjustment adjustment = core::syncAdjustment(23.5f, 0.5f, 0.0f, 0.0f, 1.0f);

    EXPECT_FLOAT_EQ(1.0f, adjustment.raHours);
}

TEST(SyncAdjustmentTest, RaCorrectionWrapsBackwardsAcrossZeroHours)
{
    const core::SyncAdjustment adjustment = core::syncAdjustment(0.5f, 23.5f, 0.0f, 0.0f, 1.0f);

    EXPECT_FLOAT_EQ(-1.0f, adjustment.raHours);
}

TEST(SyncAdjustmentTest, DecCorrectionComparesMagnitudes)
{
    // Both readings are 30 degrees from the pole, so the mount is already right.
    const core::SyncAdjustment adjustment = core::syncAdjustment(0.0f, 0.0f, -30.0f, 30.0f, 5.0f);

    EXPECT_FLOAT_EQ(0.0f, adjustment.decDegrees);
}

TEST(SyncAdjustmentTest, DecCorrectionKeepsItsSignAboveHome)
{
    const core::SyncAdjustment adjustment = core::syncAdjustment(0.0f, 0.0f, 10.0f, 40.0f, 10.0f);

    EXPECT_FLOAT_EQ(30.0f, adjustment.decDegrees);
}

TEST(SyncAdjustmentTest, DecCorrectionFlipsBelowHome)
{
    // Below home the declination axis runs the other way, so the same 30 degree gap is -30.
    const core::SyncAdjustment adjustment = core::syncAdjustment(0.0f, 0.0f, 10.0f, 40.0f, -10.0f);

    EXPECT_FLOAT_EQ(-30.0f, adjustment.decDegrees);
}

TEST(SyncAdjustmentTest, HomePositionIsNotBelowHome)
{
    // Zero is the home position itself, which counts as "above": the correction keeps its sign.
    const core::SyncAdjustment adjustment = core::syncAdjustment(0.0f, 0.0f, 10.0f, 40.0f, 0.0f);

    EXPECT_FLOAT_EQ(30.0f, adjustment.decDegrees);
}
