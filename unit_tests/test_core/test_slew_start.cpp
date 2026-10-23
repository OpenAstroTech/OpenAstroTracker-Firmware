#include <gtest/gtest.h>

#include "core/SlewStart.hpp"

namespace
{
core::SlewStartFacts facts(long current, long target, bool tracking)
{
    core::SlewStartFacts f;
    f.raCurrentSteps = current;
    f.raTargetSteps  = target;
    f.tracking       = tracking;
    return f;
}
}  // namespace

TEST(SlewStartTest, AGoToThatMovesStopsTrackingAndEntersSlewMicrosteps)
{
    const core::SlewStartPlan plan = core::planSlewStart(core::SlewStartKind::Target, facts(100, 5000, true));

    EXPECT_TRUE(plan.raMoves);
    EXPECT_TRUE(plan.stopTracking);
    EXPECT_TRUE(plan.enterRaSlewMicrosteps);
}

TEST(SlewStartTest, AGoToThatLandsOnTheCurrentPositionLeavesTrackingAlone)
{
    const core::SlewStartPlan plan = core::planSlewStart(core::SlewStartKind::Target, facts(100, 100, true));

    EXPECT_FALSE(plan.raMoves);
    EXPECT_FALSE(plan.stopTracking);
    EXPECT_FALSE(plan.enterRaSlewMicrosteps);
}

TEST(SlewStartTest, AGoToWhileAlreadyStoppedDoesNotStopTrackingAgain)
{
    const core::SlewStartPlan plan = core::planSlewStart(core::SlewStartKind::Target, facts(100, 5000, false));

    EXPECT_TRUE(plan.raMoves);
    EXPECT_FALSE(plan.stopTracking) << "a spurious stop publishes TrackingStopped";
    EXPECT_TRUE(plan.enterRaSlewMicrosteps);
}

TEST(SlewStartTest, HomingStopsTrackingEvenWhenItIsNotRunning)
{
    // The original home path never checked; the shared executor must keep that difference.
    const core::SlewStartPlan plan = core::planSlewStart(core::SlewStartKind::Home, facts(100, 0, false));

    EXPECT_TRUE(plan.raMoves);
    EXPECT_TRUE(plan.stopTracking);
    EXPECT_TRUE(plan.enterRaSlewMicrosteps);
}

TEST(SlewStartTest, HomingFromHomeStillEntersSlewMicrostepsIsFalseWhenNothingMoves)
{
    const core::SlewStartPlan plan = core::planSlewStart(core::SlewStartKind::Home, facts(0, 0, true));

    EXPECT_FALSE(plan.raMoves);
    EXPECT_FALSE(plan.stopTracking);
    EXPECT_FALSE(plan.enterRaSlewMicrosteps);
}

TEST(SlewStartTest, HomeTargetSubtractsTheTrackedDistanceInSlewMicrosteps)
{
    // Tracking ran 320 microsteps at the tracking rate; the slew rate is 8x coarser.
    EXPECT_EQ(-2560, core::homeRaTargetSteps(320, 256, 32));
}

TEST(SlewStartTest, HomeTargetTruncatesLikeTheOriginalIntegerDivision)
{
    // 5 * 8 / 6 is 6.67 steps; the firmware has always kept the 6.
    EXPECT_EQ(-6, core::homeRaTargetSteps(5, 8, 6));
    // A tracking position too small to amount to a slew microstep contributes nothing.
    EXPECT_EQ(0, core::homeRaTargetSteps(1, 1, 3));
}

TEST(SlewStartTest, HomeTargetWithNoTrackingIsHome)
{
    EXPECT_EQ(0, core::homeRaTargetSteps(0, 256, 32));
}

TEST(SlewStartTest, HomeTargetIsSignPreservingForNegativeTrackingPositions)
{
    EXPECT_EQ(2560, core::homeRaTargetSteps(-320, 256, 32));
}
