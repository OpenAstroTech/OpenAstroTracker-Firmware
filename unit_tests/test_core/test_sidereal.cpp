#include <gtest/gtest.h>
#include <math.h>
#include "core/SiderealClock.hpp"

using core::SiderealClock;

// Known-good values sourced from stargazing.net/kepler/altaz.html
TEST(SiderealClockTest, DeltaJdJ2000)
{
    // J2000 epoch: 2000-01-01 should have deltaJd = 0
    int deltaJd = SiderealClock::calculateDeltaJd(2000, 1, 1);
    EXPECT_EQ(0, deltaJd);
}

TEST(SiderealClockTest, DeltaJdKnownDate)
{
    // Just verify it's deterministic and doesn't crash
    int deltaJd = SiderealClock::calculateDeltaJd(2020, 3, 20);
    EXPECT_GT(deltaJd, 0);
}

TEST(SiderealClockTest, DeltaJdLeapYear)
{
    // 2020 is a leap year
    int jan31 = SiderealClock::calculateDeltaJd(2020, 1, 31);
    int feb1  = SiderealClock::calculateDeltaJd(2020, 2, 1);
    EXPECT_EQ(1, feb1 - jan31);
}

TEST(SiderealClockTest, DeltaJdMonotonic)
{
    int d1 = SiderealClock::calculateDeltaJd(2023, 1, 1);
    int d2 = SiderealClock::calculateDeltaJd(2023, 12, 31);
    EXPECT_LT(d1, d2);
}

TEST(SiderealClockTest, ThetaOutputInRange)
{
    double theta = SiderealClock::calculateTheta(100.0, 0.0, 12.0f);
    EXPECT_GE(theta, 0.0);
    EXPECT_LT(theta, 360.0);
}

TEST(SiderealClockTest, HaDegreesPositive)
{
    double ha = SiderealClock::calculateHaDegrees(0.0);
    EXPECT_GE(ha, 0.0);
    EXPECT_LT(ha, 360.0);
}

TEST(SiderealClockTest, HaDegreesKnownPolaris)
{
    // At LST = 0h, Polaris HA should be roughly 360 - PolarisRA*15 in degrees
    double ha = SiderealClock::calculateHaDegrees(0.0);
    // Polaris RA is ~3h = 45 deg, so HA should be ~315 deg
    EXPECT_NEAR(315.0, ha, 10.0);
}

TEST(SiderealClockTest, HaDegreesCustomPolaris)
{
    // Use custom Polaris RA values
    double ha1 = SiderealClock::calculateHaDegrees(0.0, 3, 0, 8);
    double ha2 = SiderealClock::calculateHaDegrees(0.0, 6, 0, 0);
    // Different RA => different HA
    EXPECT_NE(ha1, ha2);
}
