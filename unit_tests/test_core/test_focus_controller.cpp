#include <gtest/gtest.h>

#include "../test_common/hal_fakes/FakeStepperAxis.hpp"
#include "core/FocusController.hpp"

TEST(FocusControllerTest, DefaultsToTheFastestRateWithoutTouchingTheAxis)
{
    fakes::FakeStepperAxis axis;
    core::FocusController<> focus;
    focus.attach(&axis);
    axis.setMaxSpeed(111.0f);

    focus.initialize(1800.0f);

    EXPECT_EQ(4, focus.rate());
    EXPECT_FLOAT_EQ(1800.0f, focus.maxRateSpeed());
    // The mount has already configured the axis; initialization must not override it.
    EXPECT_FLOAT_EQ(111.0f, axis.maxSpeed());
}

TEST(FocusControllerTest, RateSelectsAFractionOfTheMaximumSpeed)
{
    fakes::FakeStepperAxis axis;
    core::FocusController<> focus;
    focus.attach(&axis);

    focus.setRate(1, 1000.0f);
    EXPECT_FLOAT_EQ(0.05f, focus.rateFactor());
    EXPECT_FLOAT_EQ(50.0f, focus.maxRateSpeed());

    focus.setRate(2, 1000.0f);
    EXPECT_FLOAT_EQ(200.0f, focus.maxRateSpeed());

    focus.setRate(3, 1000.0f);
    EXPECT_FLOAT_EQ(500.0f, focus.maxRateSpeed());

    focus.setRate(4, 1000.0f);
    EXPECT_FLOAT_EQ(1000.0f, focus.maxRateSpeed());
    // Every rate retunes the axis itself.
    EXPECT_FLOAT_EQ(1000.0f, axis.maxSpeed());
}

TEST(FocusControllerTest, RatesOutsideOneToFourAreClamped)
{
    fakes::FakeStepperAxis axis;
    core::FocusController<> focus;
    focus.attach(&axis);

    focus.setRate(-5, 1000.0f);
    EXPECT_EQ(1, focus.rate());
    EXPECT_FLOAT_EQ(50.0f, focus.maxRateSpeed());

    focus.setRate(99, 1000.0f);
    EXPECT_EQ(4, focus.rate());
    EXPECT_FLOAT_EQ(1000.0f, focus.maxRateSpeed());
}

TEST(FocusControllerTest, MovingByStepsTargetsTheRelativePosition)
{
    fakes::FakeStepperAxis axis;
    core::FocusController<> focus;
    focus.attach(&axis);
    axis.setPosition(50000);

    focus.moveBy(-250);

    EXPECT_EQ(49750, axis.targetPosition());
    EXPECT_EQ(core::FocusMode::ToTarget, focus.mode());
}

TEST(FocusControllerTest, ContinuousMotionRunsToAFarTargetInTheGivenDirection)
{
    fakes::FakeStepperAxis axis;
    core::FocusController<> focus;
    focus.attach(&axis);
    focus.initialize(1000.0f);

    focus.startContinuous(core::FOCUS_FORWARD);
    EXPECT_EQ(300000, axis.targetPosition());
    EXPECT_FLOAT_EQ(1000.0f, axis.maxSpeed());
    EXPECT_EQ(core::FocusMode::ToTarget, focus.mode());

    focus.startContinuous(core::FOCUS_BACKWARD);
    EXPECT_EQ(-300000, axis.targetPosition());
}

TEST(FocusControllerTest, RunAtSpeedUsesTheSignAsDirection)
{
    fakes::FakeStepperAxis axis;
    core::FocusController<> focus;
    focus.attach(&axis);

    focus.runAtSpeed(-400.0f);

    EXPECT_EQ(-300000, axis.targetPosition());
    EXPECT_EQ(core::FocusMode::ToTarget, focus.mode());
}

TEST(FocusControllerTest, RunAtZeroSpeedStopsTheAxisAndLeavesTheModeAlone)
{
    fakes::FakeStepperAxis axis;
    core::FocusController<> focus;
    focus.attach(&axis);
    focus.initialize(1000.0f);
    focus.startContinuous(core::FOCUS_FORWARD);

    focus.runAtSpeed(0.0f);

    EXPECT_FALSE(axis.isRunning());
    EXPECT_EQ(1u, axis.stopCalls());
    EXPECT_EQ(core::FocusMode::ToTarget, focus.mode());
}

TEST(FocusControllerTest, UpdateReportsTheStopExactlyOnce)
{
    fakes::FakeStepperAxis axis;
    core::FocusController<> focus;
    focus.attach(&axis);
    axis.setPosition(10);
    focus.moveBy(100);

    EXPECT_EQ(core::FocusUpdate::None, focus.update());  // still running
    EXPECT_EQ(core::FocusMode::ToTarget, focus.mode());

    axis.setPosition(110);  // arrived
    EXPECT_EQ(core::FocusUpdate::Stopped, focus.update());
    EXPECT_EQ(core::FocusMode::Idle, focus.mode());

    EXPECT_EQ(core::FocusUpdate::None, focus.update());
}

TEST(FocusControllerTest, AMoveCancelledBeforeItRanDoesNotReportAStop)
{
    fakes::FakeStepperAxis axis;
    core::FocusController<> focus;
    focus.attach(&axis);
    axis.setPosition(10);
    focus.moveBy(100);
    focus.stop();  // never observed running

    EXPECT_EQ(core::FocusUpdate::None, focus.update());
}

TEST(FocusControllerTest, StopReachesTheAxis)
{
    fakes::FakeStepperAxis axis;
    core::FocusController<> focus;
    focus.attach(&axis);
    axis.setPosition(10);
    focus.moveBy(100);

    focus.stop();

    EXPECT_EQ(1u, axis.stopCalls());
    EXPECT_FALSE(focus.isRunning());
}

TEST(FocusControllerTest, PositionAndSpeedComeFromTheAxis)
{
    fakes::FakeStepperAxis axis;
    core::FocusController<> focus;
    focus.attach(&axis);

    focus.setPosition(1234);
    axis.setSpeed(7.5f);

    EXPECT_EQ(1234, focus.position());
    EXPECT_FLOAT_EQ(7.5f, focus.speed());
}

TEST(FocusControllerTest, DetachForgetsTheAxisAndTheMode)
{
    fakes::FakeStepperAxis axis;
    core::FocusController<> focus;
    focus.attach(&axis);
    focus.moveBy(10);

    focus.detach();

    EXPECT_EQ(core::FocusMode::Idle, focus.mode());
}

// A board without a focuser keeps the controller detached and still ticks it every pass, so
// a detached controller has to read as idle rather than dereference a null axis. This is the
// shape `MountCoordinator::tick()` relies on for every board.
TEST(FocusControllerTest, DetachedControllerReadsAsIdle)
{
    core::FocusController<> focus;

    EXPECT_FALSE(focus.attached());
    EXPECT_FALSE(focus.isRunning());
    EXPECT_EQ(0, focus.position());
    EXPECT_FLOAT_EQ(0.0f, focus.speed());
    EXPECT_EQ(core::FocusUpdate::None, focus.update());

    // Commands are accepted and dropped, so a caller needs no presence check of its own.
    focus.setPosition(100);
    focus.moveBy(10);
    focus.runAtSpeed(5.0f);
    focus.stop();
    focus.setRate(2, 10.0f);

    EXPECT_FALSE(focus.attached());
    EXPECT_EQ(0, focus.position());
    EXPECT_EQ(core::FocusUpdate::None, focus.update());
}

// Attaching after a detach must bring the controller back, including the mode the stopped
// transition resets.
TEST(FocusControllerTest, ReattachResumesMotion)
{
    fakes::FakeStepperAxis axis;
    core::FocusController<> focus;
    focus.attach(&axis);
    focus.detach();

    focus.attach(&axis);
    focus.moveBy(25);

    EXPECT_TRUE(focus.attached());
    EXPECT_EQ(core::FocusMode::ToTarget, focus.mode());
    EXPECT_EQ(25, axis.targetPosition());
}
