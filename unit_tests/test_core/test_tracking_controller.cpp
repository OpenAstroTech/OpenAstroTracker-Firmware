#include <gtest/gtest.h>

#include "../test_common/hal_fakes/FakeStepperAxis.hpp"
#include "core/TrackingController.hpp"

namespace
{
/// The shipped geometry: TMC2209 UART with 256x tracking microsteps and 8x for slewing.
constexpr float kSiderealSecondsPerDay = 86164.0905f;
constexpr unsigned kMicrostepRatio     = 256 / 8;

core::TrackingController<> makeController()
{
    return core::TrackingController<>(kSiderealSecondsPerDay, kMicrostepRatio);
}
}  // namespace

TEST(TrackingControllerTest, RateCombinesCalibrationGeometryAndSiderealDay)
{
    fakes::FakeStepperAxis axis;
    core::TrackingController<> tracking = makeController();
    tracking.attach(&axis);

    tracking.setCalibration(1.0f);
    tracking.setStepsPerDegree(1000.0f);

    EXPECT_FLOAT_EQ(1000.0f * 32.0f * 360.0f / kSiderealSecondsPerDay, tracking.speed());
}

TEST(TrackingControllerTest, CalibrationScalesTheRate)
{
    core::TrackingController<> tracking = makeController();

    tracking.setCalibration(1.0f);
    tracking.setStepsPerDegree(1000.0f);
    const float nominal = tracking.speed();

    tracking.setCalibration(1.02f);
    tracking.setStepsPerDegree(1000.0f);

    EXPECT_FLOAT_EQ(nominal * 1.02f, tracking.speed());
    EXPECT_FLOAT_EQ(1.02f, tracking.calibration());
}

TEST(TrackingControllerTest, GeometryChangeRecalculatesTheRate)
{
    core::TrackingController<> tracking = makeController();
    tracking.setCalibration(1.0f);
    tracking.setStepsPerDegree(1000.0f);
    const float before = tracking.speed();

    tracking.setStepsPerDegree(2000.0f);

    EXPECT_FLOAT_EQ(before * 2.0f, tracking.speed());
}

TEST(TrackingControllerTest, ApplySpeedHandsTheRateToTheAxis)
{
    fakes::FakeStepperAxis axis;
    core::TrackingController<> tracking = makeController();
    tracking.attach(&axis);
    tracking.setCalibration(1.0f);
    tracking.setStepsPerDegree(1000.0f);

    tracking.applySpeed();

    EXPECT_FLOAT_EQ(tracking.speed(), axis.speed());
    EXPECT_EQ(1u, axis.setSpeedCalls());
}

TEST(TrackingControllerTest, StopStopsTheAxis)
{
    fakes::FakeStepperAxis axis;
    core::TrackingController<> tracking = makeController();
    tracking.attach(&axis);
    axis.moveTo(500);

    tracking.stop();

    EXPECT_EQ(1u, axis.stopCalls());
    EXPECT_FALSE(axis.isRunning());
}

TEST(TrackingControllerTest, TrackerOffIsCompensatedAtTheTrackingRate)
{
    fakes::FakeStepperAxis axis;
    core::TrackingController<> tracking = makeController();
    tracking.attach(&axis);
    tracking.setCalibration(1.0f);
    tracking.setStepsPerDegree(86164.0905f / (32.0f * 360.0f));  // exactly 1 step/sec
    ASSERT_FLOAT_EQ(1.0f, tracking.speed());

    tracking.noteStopped(1000);
    ASSERT_TRUE(tracking.compensationPending());

    const long steps = tracking.compensationSteps(3500);

    EXPECT_EQ(2, steps);
}

TEST(TrackingControllerTest, ApplyingCompensationAdvancesTheAxisAndClearsTheRequest)
{
    fakes::FakeStepperAxis axis;
    core::TrackingController<> tracking = makeController();
    tracking.attach(&axis);
    tracking.setCalibration(1.0f);
    tracking.setStepsPerDegree(86164.0905f / (32.0f * 360.0f));
    axis.setPosition(700);

    tracking.noteStopped(0);
    tracking.applyCompensation(tracking.compensationSteps(2000));

    EXPECT_EQ(702, axis.currentPosition());
    EXPECT_EQ(1u, axis.runToNewPositionCalls());
    EXPECT_FALSE(tracking.compensationPending());
}

TEST(TrackingControllerTest, CompensationSurvivesTheMillisecondCounterWrapping)
{
    fakes::FakeStepperAxis axis;
    core::TrackingController<> tracking = makeController();
    tracking.attach(&axis);
    tracking.setCalibration(1.0f);
    tracking.setStepsPerDegree(86164.0905f / (32.0f * 360.0f));

    tracking.noteStopped(0xFFFFFE00u);

    // 0x200 ms later, so the counter wrapped: 1 step/sec -> 0.512 steps, truncated.
    EXPECT_EQ(0, tracking.compensationSteps(0x100u));
    EXPECT_EQ(0xFFFFFE00u, tracking.stoppedAt());
}

TEST(TrackingControllerTest, TrackedHoursDeriveFromPositionAndRate)
{
    fakes::FakeStepperAxis axis;
    // 1 * 1 * 1 * 360 / 0.125 = 2880 u-steps/sec, so 10,368,000 steps are one hour.
    core::TrackingController<> fast(0.125f, 1);
    fast.attach(&axis);
    fast.setCalibration(1.0f);
    fast.setStepsPerDegree(1.0f);
    ASSERT_FLOAT_EQ(2880.0f, fast.speed());

    axis.setPosition(3600 * 2880);

    EXPECT_FLOAT_EQ(1.0f, fast.trackedHours());
}

TEST(TrackingControllerTest, DetachForgetsTheAxisAndAnyPendingCompensation)
{
    fakes::FakeStepperAxis axis;
    core::TrackingController<> tracking = makeController();
    tracking.attach(&axis);
    tracking.noteStopped(1000);
    ASSERT_TRUE(tracking.compensationPending());

    tracking.detach();

    EXPECT_FALSE(tracking.compensationPending());
    EXPECT_EQ(0u, tracking.stoppedAt());
}

TEST(TrackingControllerTest, TrackedHoursAreZeroBeforeTheRateIsKnown)
{
    fakes::FakeStepperAxis axis;
    core::TrackingController<> tracking = makeController();
    tracking.attach(&axis);
    axis.setPosition(1000);

    EXPECT_FLOAT_EQ(0.0f, tracking.trackedHours());
}
