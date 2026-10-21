#include <gtest/gtest.h>

#include "core/HomingController.hpp"

namespace
{
constexpr long kStepsPerDegree = 10;
constexpr int kInitialDir      = 1;
constexpr int kSearchDistance  = 20;

core::HomingController startedWith(bool sensorActive)
{
    core::HomingController homing;
    homing.start(kInitialDir, kSearchDistance, kStepsPerDegree, 0, 0, sensorActive);
    return homing;
}

/// Feeds `ticks` samples of the same sensor level while the axis keeps running.
void scan(core::HomingController &homing, bool sensorActive, long firstPosition, int ticks, long stepPerTick = 0)
{
    for (int i = 0; i < ticks; i++)
    {
        homing.update(sensorActive, true, firstPosition + i * stepPerTick, 0);
    }
}
}  // namespace

TEST(HomingControllerTest, StartsIdleAndReportsNeverRun)
{
    core::HomingController homing;

    EXPECT_FALSE(homing.isActive());
    EXPECT_EQ(core::HomingState::NotActive, homing.state());
    EXPECT_EQ(core::HomingResult::NeverRun, homing.result());
}

TEST(HomingControllerTest, SensorSignalledStartsByMovingOff)
{
    core::HomingController homing = startedWith(true);
    EXPECT_EQ(core::HomingState::MoveOff, homing.state());
    EXPECT_TRUE(homing.isActive());

    const core::HomingStep step = homing.update(true, false, 0, 0);

    EXPECT_EQ(core::HomingAction::MoveBy, step.action);
    EXPECT_EQ(-kInitialDir * kStepsPerDegree * 15, step.steps);
    EXPECT_EQ(core::HomingState::MovingOff, homing.state());
}

TEST(HomingControllerTest, SensorClearStartsBySearchingForIt)
{
    core::HomingController homing = startedWith(false);

    const core::HomingStep step = homing.update(false, false, 0, 0);

    EXPECT_EQ(core::HomingAction::MoveBy, step.action);
    EXPECT_EQ(kInitialDir * kStepsPerDegree * kSearchDistance, step.steps);
    EXPECT_EQ(core::HomingState::FindingStart, homing.state());
}

TEST(HomingControllerTest, LeavingTheSensorWaitsTwoSecondsThenStops)
{
    core::HomingController homing = startedWith(true);
    homing.update(true, false, 0, 0);  // MOVE_OFF

    EXPECT_EQ(core::HomingAction::None, homing.update(true, true, 10, 100).action);
    EXPECT_EQ(core::HomingState::MovingOff, homing.state());

    EXPECT_EQ(core::HomingAction::None, homing.update(false, true, 20, 200).action);
    EXPECT_EQ(core::HomingState::StopAtTime, homing.state());

    EXPECT_EQ(core::HomingAction::None, homing.update(false, true, 30, 2199).action);
    EXPECT_EQ(core::HomingState::StopAtTime, homing.state());

    const core::HomingStep step = homing.update(false, true, 30, 2201);
    EXPECT_EQ(core::HomingAction::StopAxis, step.action);
    EXPECT_EQ(core::HomingState::WaitForStop, homing.state());

    EXPECT_EQ(core::HomingAction::None, homing.update(false, true, 30, 2300).action);
    EXPECT_EQ(core::HomingState::WaitForStop, homing.state());
    EXPECT_EQ(core::HomingAction::None, homing.update(false, false, 30, 2400).action);
    EXPECT_EQ(core::HomingState::StartFindStart, homing.state());
}

TEST(HomingControllerTest, StoppingBeforeLeavingTheSensorFails)
{
    core::HomingController homing = startedWith(true);
    homing.update(true, false, 0, 0);  // MOVE_OFF

    homing.update(true, false, 0, 10);  // stopped while still on the sensor

    EXPECT_EQ(core::HomingState::Failed, homing.state());
    EXPECT_EQ(core::HomingResult::CantMoveOffSensor, homing.result());

    const core::HomingStep step = homing.update(true, false, 0, 20);
    EXPECT_EQ(core::HomingAction::Failed, step.action);
    EXPECT_FALSE(homing.isActive());
}

TEST(HomingControllerTest, FindingTheStartKeepsGoingSoTheFarEdgeIsReached)
{
    core::HomingController homing = startedWith(false);
    homing.update(false, false, 0, 0);  // START_FIND_START

    scan(homing, true, 40, 4);
    EXPECT_EQ(core::HomingState::FindingStart, homing.state());

    const core::HomingStep step = homing.update(true, true, 44, 0);

    EXPECT_EQ(core::HomingState::FindingEnd, homing.state());
    EXPECT_EQ(core::HomingAction::MoveBy, step.action);
    EXPECT_EQ(kInitialDir * kStepsPerDegree * kSearchDistance, step.steps);
    EXPECT_EQ(40, homing.sensorStartPosition());
}

TEST(HomingControllerTest, AFlappingSensorNeverCountsAsAnEdge)
{
    core::HomingController homing = startedWith(false);
    homing.update(false, false, 0, 0);

    for (int i = 0; i < 20; i++)
    {
        homing.update((i % 2) == 0, true, 100 + i, 0);
    }

    EXPECT_EQ(core::HomingState::FindingStart, homing.state());
    EXPECT_EQ(0, homing.sensorStartPosition());
}

TEST(HomingControllerTest, MissingTheStartOnTheForwardPassSearchesBackwards)
{
    core::HomingController homing = startedWith(false);
    homing.update(false, false, 0, 0);  // START_FIND_START

    const core::HomingStep step = homing.update(false, false, 200, 10);

    EXPECT_EQ(core::HomingAction::MoveBy, step.action);
    EXPECT_EQ(-kInitialDir * kStepsPerDegree * kSearchDistance * 2L, step.steps);
    EXPECT_EQ(core::HomingState::FindingStartReverse, homing.state());
}

TEST(HomingControllerTest, MissingTheStartInBothDirectionsFails)
{
    core::HomingController homing = startedWith(false);
    homing.update(false, false, 0, 0);
    homing.update(false, false, 200, 10);  // reverse pass

    homing.update(false, false, 0, 20);

    EXPECT_EQ(core::HomingState::Failed, homing.state());
    EXPECT_EQ(core::HomingResult::CantFindSensorOnReverse, homing.result());

    EXPECT_EQ(core::HomingAction::Failed, homing.update(false, false, 0, 30).action);
}

TEST(HomingControllerTest, MissingTheFarEdgeFails)
{
    core::HomingController homing = startedWith(false);
    homing.update(false, false, 0, 0);
    scan(homing, true, 40, 5);  // FINDING_END

    homing.update(true, false, 100, 10);

    EXPECT_EQ(core::HomingState::Failed, homing.state());
    EXPECT_EQ(core::HomingResult::CantFindSensorEnd, homing.result());
}

TEST(HomingControllerTest, ACompleteRunHomesToTheMiddleOfTheSensor)
{
    core::HomingController homing;
    homing.start(kInitialDir, kSearchDistance, kStepsPerDegree, 0, 25, false);

    homing.update(false, false, 0, 0);  // START_FIND_START

    scan(homing, true, 40, 5);  // start edge seen at 40, keep going
    ASSERT_EQ(core::HomingState::FindingEnd, homing.state());

    // The sensor's far edge is seen at 240; five consistent samples stop the axis.
    scan(homing, false, 240, 5);
    EXPECT_EQ(core::HomingState::WaitForStop, homing.state());
    EXPECT_EQ(240, homing.sensorEndPosition());

    homing.update(false, false, 245, 10);  // axis came to rest
    ASSERT_EQ(core::HomingState::RangeFound, homing.state());

    const core::HomingStep move = homing.update(false, false, 245, 20);
    EXPECT_EQ(core::HomingAction::MoveBy, move.action);
    // Midpoint (40 + 240) / 2 = 140, minus the position and the 25-step offset.
    EXPECT_EQ(140 - 245 - 25, move.steps);

    homing.update(false, true, 115, 30);   // moving to the centre
    homing.update(false, false, 115, 40);  // arrived
    ASSERT_EQ(core::HomingState::Successful, homing.state());

    const core::HomingStep done = homing.update(false, false, 115, 50);
    EXPECT_EQ(core::HomingAction::Succeeded, done.action);
    EXPECT_EQ(core::HomingResult::Succeeded, homing.result());
    EXPECT_FALSE(homing.isActive());
}

TEST(HomingControllerTest, TheTwoSecondStopSurvivesTheMillisecondCounterWrapping)
{
    core::HomingController homing = startedWith(true);
    homing.update(true, false, 0, 0);
    homing.update(true, true, 10, 0xFFFFF000u);

    // Leaving the sensor arms a deadline 2000 ms later, at 0xFFFFFFD0.
    homing.update(false, true, 20, 0xFFFFF800u);
    ASSERT_EQ(core::HomingState::StopAtTime, homing.state());

    EXPECT_EQ(core::HomingAction::None, homing.update(false, true, 30, 0xFFFFFFCFu).action);
    EXPECT_EQ(core::HomingAction::None, homing.update(false, true, 30, 0xFFFFFFD0u).action);
    // 0x40 ms after the deadline, but only after the millisecond counter wrapped.
    EXPECT_EQ(core::HomingAction::StopAxis, homing.update(false, true, 30, 0x00000010u).action);
}

TEST(HomingControllerTest, StartPositionIsReportedForTheFailurePath)
{
    core::HomingController homing;
    homing.start(kInitialDir, kSearchDistance, kStepsPerDegree, -1234, 0, false);

    EXPECT_EQ(-1234, homing.startPosition());
}
