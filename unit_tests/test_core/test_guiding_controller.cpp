#include <gtest/gtest.h>

#include "../test_common/hal_fakes/FakeSystemClock.hpp"
#include "core/GuidingController.hpp"

namespace
{
// The shipped geometry: TMC2209 UART, 256x tracking microsteps, 8x for slewing.
constexpr float kSiderealDegreesPerHour = 14.95904348958f;
constexpr float kRaStepsPerDegree       = 360.0f;
constexpr float kDecStepsPerDegree      = 360.0f;

core::GuideAxisGeometry raGeometry()
{
    return core::GuideAxisGeometry {kRaStepsPerDegree, 256, 8, 1.5f};
}

core::GuideAxisGeometry decGeometry()
{
    return core::GuideAxisGeometry {kDecStepsPerDegree, 256, 8, 0.5f};
}
}  // namespace

TEST(GuidingControllerTest, RaPulseBaseSpeedUsesSiderealRateAndMicrostepRatio)
{
    const core::RaPulse pulse = core::GuidingController::raPulse(true, raGeometry(), kSiderealDegreesPerHour, 1.0f);

    const float expected = kRaStepsPerDegree * (1.0 * 256 / 8) * kSiderealDegreesPerHour / 3600.0f;
    EXPECT_FLOAT_EQ(expected, pulse.baseSpeed);
}

TEST(GuidingControllerTest, WestPulseIsFasterThanSidereal)
{
    const core::RaPulse pulse = core::GuidingController::raPulse(true, raGeometry(), kSiderealDegreesPerHour, 1.0f);

    EXPECT_FLOAT_EQ(pulse.adjustedSpeed * 1.5f, pulse.targetSpeed);
    EXPECT_GT(pulse.targetSpeed, pulse.adjustedSpeed);
}

TEST(GuidingControllerTest, EastPulseIsSlowerThanSidereal)
{
    const core::RaPulse pulse = core::GuidingController::raPulse(false, raGeometry(), kSiderealDegreesPerHour, 1.0f);

    EXPECT_FLOAT_EQ(pulse.adjustedSpeed * (2.0f - 1.5f), pulse.targetSpeed);
    EXPECT_LT(pulse.targetSpeed, pulse.adjustedSpeed);
}

TEST(GuidingControllerTest, RaPulseIsScaledByTheTrackingCalibration)
{
    const core::RaPulse nominal  = core::GuidingController::raPulse(true, raGeometry(), kSiderealDegreesPerHour, 1.0f);
    const core::RaPulse adjusted = core::GuidingController::raPulse(true, raGeometry(), kSiderealDegreesPerHour, 1.04f);

    EXPECT_FLOAT_EQ(nominal.baseSpeed, adjusted.baseSpeed);
    EXPECT_FLOAT_EQ(nominal.targetSpeed * 1.04f, adjusted.targetSpeed);
}

TEST(GuidingControllerTest, DecPulseIsSiderealInBothDirections)
{
    const core::DecPulse north = core::GuidingController::decPulse(true, decGeometry(), kSiderealDegreesPerHour, 0, 0, 0);
    const core::DecPulse south = core::GuidingController::decPulse(false, decGeometry(), kSiderealDegreesPerHour, 0, 0, 0);

    EXPECT_TRUE(north.allowed);
    EXPECT_TRUE(south.allowed);
    EXPECT_FLOAT_EQ(north.baseSpeed * 0.5f, north.targetSpeed);
    EXPECT_FLOAT_EQ(-north.baseSpeed * 0.5f, south.targetSpeed);
}

TEST(GuidingControllerTest, DecPulseIsRefusedAtTheLimitItWouldPass)
{
    const long lowerLimit = -1000;
    const long upperLimit = 1000;

    EXPECT_TRUE(core::GuidingController::decPulse(true, decGeometry(), kSiderealDegreesPerHour, 999, lowerLimit, upperLimit).allowed);
    EXPECT_FALSE(core::GuidingController::decPulse(true, decGeometry(), kSiderealDegreesPerHour, 1000, lowerLimit, upperLimit).allowed);
    EXPECT_TRUE(core::GuidingController::decPulse(false, decGeometry(), kSiderealDegreesPerHour, -999, lowerLimit, upperLimit).allowed);
    EXPECT_FALSE(core::GuidingController::decPulse(false, decGeometry(), kSiderealDegreesPerHour, -1000, lowerLimit, upperLimit).allowed);
    // The pulse away from the limit is always allowed.
    EXPECT_TRUE(core::GuidingController::decPulse(false, decGeometry(), kSiderealDegreesPerHour, 1000, lowerLimit, upperLimit).allowed);
    EXPECT_TRUE(core::GuidingController::decPulse(true, decGeometry(), kSiderealDegreesPerHour, -1000, lowerLimit, upperLimit).allowed);
}

TEST(GuidingControllerTest, AConfiguredLimitOfZeroMeansNoLimit)
{
    EXPECT_TRUE(core::GuidingController::decPulse(true, decGeometry(), kSiderealDegreesPerHour, 1000000, 0, 0).allowed);
    EXPECT_TRUE(core::GuidingController::decPulse(false, decGeometry(), kSiderealDegreesPerHour, -1000000, 0, 0).allowed);
}

TEST(GuidingControllerTest, PulseExpiresWhenItsTimeIsUp)
{
    fakes::FakeClock clock;
    core::GuidingController guiding(&clock);

    guiding.startRa(500);
    EXPECT_FALSE(guiding.raExpired());

    clock.advance(499);
    EXPECT_FALSE(guiding.raExpired());

    clock.advance(1);
    EXPECT_FALSE(guiding.raExpired());  // the deadline itself is not yet past

    clock.advance(1);
    EXPECT_TRUE(guiding.raExpired());
}

TEST(GuidingControllerTest, AxesExpireIndependently)
{
    fakes::FakeClock clock;
    core::GuidingController guiding(&clock);

    guiding.startRa(100);
    clock.advance(50);
    guiding.startDec(100);

    clock.advance(60);

    EXPECT_TRUE(guiding.raExpired());
    EXPECT_FALSE(guiding.decExpired());
}

TEST(GuidingControllerTest, AnUnarmedPulseNeverExpires)
{
    fakes::FakeClock clock;
    core::GuidingController guiding(&clock);
    clock.set(100000);

    EXPECT_FALSE(guiding.raExpired());
    EXPECT_FALSE(guiding.decExpired());
    EXPECT_EQ(0u, guiding.raDeadline());
    EXPECT_EQ(0u, guiding.decDeadline());
}

TEST(GuidingControllerTest, ClearingAPulseForgetsItsDeadline)
{
    fakes::FakeClock clock;
    core::GuidingController guiding(&clock);

    guiding.startRa(100);
    guiding.startDec(100);
    guiding.clearRa();
    guiding.clearDec();

    clock.advance(1000);

    EXPECT_FALSE(guiding.raExpired());
    EXPECT_FALSE(guiding.decExpired());
    EXPECT_EQ(0u, guiding.raDeadline());
    EXPECT_EQ(0u, guiding.decDeadline());
}

TEST(GuidingControllerTest, PulseTimingSurvivesTheMillisecondCounterWrapping)
{
    fakes::FakeClock clock;
    core::GuidingController guiding(&clock);
    clock.set(0xFFFFFF00u);

    guiding.startRa(500);  // deadline wraps to 0x000000F4

    clock.advance(499);
    EXPECT_FALSE(guiding.raExpired());

    clock.advance(2);
    EXPECT_TRUE(guiding.raExpired());
}

TEST(GuidingControllerTest, DeadlinesAreReportedForDiagnostics)
{
    fakes::FakeClock clock;
    core::GuidingController guiding(&clock);
    clock.set(1000);

    guiding.startRa(250);
    guiding.startDec(500);

    EXPECT_EQ(1250u, guiding.raDeadline());
    EXPECT_EQ(1500u, guiding.decDeadline());
}
