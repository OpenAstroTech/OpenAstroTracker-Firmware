#include <gtest/gtest.h>

#include "core/RelativeMove.hpp"

TEST(RelativeMoveTest, TheCombinedAxisIsRejected)
{
    const core::RelativeMovePlan plan = core::planRelativeMove(RA_AND_DEC_STEPS, 10, true);

    EXPECT_FALSE(plan.accepted);
    EXPECT_FALSE(plan.stopTracking);
    EXPECT_FALSE(plan.entersSlewMode);
}

TEST(RelativeMoveTest, AnRaNudgeStopsTheTrackerFirst)
{
    const core::RelativeMovePlan plan = core::planRelativeMove(RA_STEPS, 10, true);

    EXPECT_TRUE(plan.accepted);
    EXPECT_TRUE(plan.stopTracking);
    EXPECT_TRUE(plan.entersSlewMode);
}

TEST(RelativeMoveTest, AnRaNudgeWhileNotTrackingDoesNotStopIt)
{
    const core::RelativeMovePlan plan = core::planRelativeMove(RA_STEPS, 10, false);

    EXPECT_TRUE(plan.accepted);
    EXPECT_FALSE(plan.stopTracking) << "a spurious stop publishes TrackingStopped";
    EXPECT_TRUE(plan.entersSlewMode);
}

TEST(RelativeMoveTest, AZeroStepRaNudgeStillEntersSlewMode)
{
    // Preserved quirk: the microstep switch and the status bits happen even with nothing to move.
    const core::RelativeMovePlan plan = core::planRelativeMove(RA_STEPS, 0, true);

    EXPECT_TRUE(plan.accepted);
    EXPECT_FALSE(plan.stopTracking);
    EXPECT_TRUE(plan.entersSlewMode);
}

TEST(RelativeMoveTest, ADecNudgeNeverTouchesTheTracker)
{
    const core::RelativeMovePlan plan = core::planRelativeMove(DEC_STEPS, -25, true);

    EXPECT_TRUE(plan.accepted);
    EXPECT_FALSE(plan.stopTracking);
    EXPECT_TRUE(plan.entersSlewMode);
}

TEST(RelativeMoveTest, TheOptionalAxesStayOutOfTheSlewStateMachine)
{
    for (StepperAxis axis : {AZIMUTH_STEPS, ALTITUDE_STEPS, FOCUS_STEPS})
    {
        const core::RelativeMovePlan plan = core::planRelativeMove(axis, 5, true);

        EXPECT_TRUE(plan.accepted);
        EXPECT_FALSE(plan.stopTracking);
        EXPECT_FALSE(plan.entersSlewMode) << "axis " << int(axis);
    }
}
