#include <gtest/gtest.h>

#include "../test_common/hal_fakes/FakeStepperMotor.hpp"
#include "adapters/StepperAxis.hpp"

TEST(StepperAxisTest, ForwardsMotionRequestsToTheEngine)
{
    fakes::FakeStepperMotor motor;
    adapters::StepperAxis axis(motor);

    axis.setMaxSpeed(1200.0f);
    axis.setAcceleration(800.0f);
    axis.moveTo(500);

    EXPECT_FLOAT_EQ(1200.0f, motor.maxSpeed());
    EXPECT_FLOAT_EQ(800.0f, motor.acceleration());
    EXPECT_EQ(500, motor.targetPosition());
    EXPECT_EQ(1u, motor.moveToCalls());
}

TEST(StepperAxisTest, RelativeMoveIsRelativeToTheEnginePosition)
{
    fakes::FakeStepperMotor motor;
    adapters::StepperAxis axis(motor);
    motor.setPosition(100);

    axis.move(-30);

    EXPECT_EQ(70, motor.targetPosition());
    EXPECT_EQ(1u, motor.moveCalls());
}

TEST(StepperAxisTest, SnapshotDescribesTheEngine)
{
    fakes::FakeStepperMotor motor;
    adapters::StepperAxis axis(motor);
    motor.setPosition(10);
    axis.setSpeed(3.5f);
    axis.moveTo(25);

    const ports::AxisSnapshot state = axis.snapshot();

    EXPECT_EQ(10, state.currentPosition);
    EXPECT_EQ(25, state.targetPosition);
    EXPECT_EQ(15, state.distanceToGo);
    EXPECT_FLOAT_EQ(3.5f, state.speed);
    EXPECT_TRUE(state.isRunning);
}

TEST(StepperAxisTest, StopFreezesTheAxisWhereItIs)
{
    fakes::FakeStepperMotor motor;
    adapters::StepperAxis axis(motor);
    axis.moveTo(400);

    axis.stop();

    EXPECT_FALSE(axis.isRunning());
    EXPECT_EQ(0, axis.distanceToGo());
    EXPECT_EQ(1u, motor.stopCalls());
}

TEST(StepperAxisTest, RunAndRunSpeedAreForwardedNotSimulated)
{
    fakes::FakeStepperMotor motor;
    adapters::StepperAxis axis(motor);

    axis.run();
    axis.runSpeed();

    // AVR stubs these out inside its engine; the adapter must not try to compensate.
    EXPECT_EQ(1u, motor.runCalls());
    EXPECT_EQ(1u, motor.runSpeedCalls());
}

TEST(StepperAxisTest, RunToPositionCompletesAtTheTarget)
{
    fakes::FakeStepperMotor motor;
    adapters::StepperAxis axis(motor);

    axis.runToNewPosition(-12);

    EXPECT_EQ(-12, axis.currentPosition());
    EXPECT_FALSE(axis.isRunning());
    EXPECT_EQ(1u, motor.runToNewPositionCalls());
}

TEST(StepperAxisTest, PinInversionIsPassedThrough)
{
    fakes::FakeStepperMotor motor;
    adapters::StepperAxis axis(motor);

    axis.setPinsInverted(true, false, false);

    EXPECT_TRUE(motor.inverted());
    EXPECT_EQ(1u, motor.invertCalls());
}

TEST(StepperAxisTest, RunToPositionBlocksInTheEngineUntilDone)
{
    fakes::FakeStepperMotor motor;
    adapters::StepperAxis axis(motor);
    axis.moveTo(90);

    axis.runToPosition();

    EXPECT_EQ(90, axis.currentPosition());
    EXPECT_EQ(1u, motor.runToPositionCalls());
}

TEST(StepperAxisTest, SpeedAndLimitReadBackFromTheEngine)
{
    fakes::FakeStepperMotor motor;
    adapters::StepperAxis axis(motor);

    axis.setMaxSpeed(1500.0f);
    axis.setSpeed(-42.0f);

    EXPECT_FLOAT_EQ(1500.0f, axis.maxSpeed());
    EXPECT_FLOAT_EQ(-42.0f, axis.speed());
}

TEST(StepperAxisTest, CurrentPositionCanBeSeeded)
{
    fakes::FakeStepperMotor motor;
    adapters::StepperAxis axis(motor);

    axis.setCurrentPosition(50000);

    EXPECT_EQ(50000, axis.currentPosition());
}
