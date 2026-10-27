#include <gtest/gtest.h>
#include <math.h>
#include "core/SiderealClock.hpp"

using core::SiderealClock;

// Compose the two core primitives exactly as Sidereal::calculateByDateAndTime
// does in the firmware, and return local sidereal time in hours.
static double lstHours(int year, int month, int day, float utcHours, double longitude)
{
    const int deltaJd   = SiderealClock::calculateDeltaJd(year, month, day);
    const double deltaJ = deltaJd + (utcHours / 24.0);
    return SiderealClock::calculateTheta(deltaJ, longitude, utcHours) / 15.0;
}

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

// Reference GMST values computed independently with the Meeus formula
// (280.46061837 + 360.98564736629 * (JD - 2451545.0)), 0.01 h tolerance.
TEST(SiderealClockTest, LstAtJ2000Epoch)
{
    // 2000-01-01 12:00 UTC at Greenwich
    EXPECT_NEAR(18.6974, lstHours(2000, 1, 1, 12.0f, 0.0), 0.01);
}

TEST(SiderealClockTest, LstAtMarchEquinox)
{
    // 2024-03-20 12:00 UTC at Greenwich
    EXPECT_NEAR(23.9008, lstHours(2024, 3, 20, 12.0f, 0.0), 0.01);
}

TEST(SiderealClockTest, LstAtJuneSolstice)
{
    // 2024-06-20 12:00 UTC at Greenwich
    EXPECT_NEAR(5.9461, lstHours(2024, 6, 20, 12.0f, 0.0), 0.01);
}

TEST(SiderealClockTest, LstAtSeptemberEquinox)
{
    // 2024-09-22 12:00 UTC at Greenwich
    EXPECT_NEAR(12.1228, lstHours(2024, 9, 22, 12.0f, 0.0), 0.01);
}

TEST(SiderealClockTest, LstAtDecemberSolstice)
{
    // 2024-12-21 12:00 UTC at Greenwich
    EXPECT_NEAR(18.0367, lstHours(2024, 12, 21, 12.0f, 0.0), 0.01);
}

TEST(SiderealClockTest, LstShiftsEastWithLongitudeAndWraps)
{
    // 13.5 deg east of Greenwich advances LST by 0.9 h, past 24 h here
    EXPECT_NEAR(0.8008, lstHours(2024, 3, 20, 12.0f, 13.5), 0.01);
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

TEST(SiderealTest, LocalSiderealTimeMatchesTheThetaFormula)
{
    const double longitude = 11.5;
    const int year         = 2026;
    const int month        = 10;
    const int day          = 23;
    const float utcHours   = 12.25f;

    const int deltaJd    = core::SiderealClock::calculateDeltaJd(year, month, day);
    const double deltaJ  = static_cast<double>(deltaJd) + (static_cast<double>(utcHours) / 24.0);
    const float expected = static_cast<float>(core::SiderealClock::calculateTheta(deltaJ, longitude, utcHours) / 15.0);

    const types::DayTime actual = core::SiderealClock::localSiderealTime(longitude, year, month, day, utcHours);
    EXPECT_NEAR(expected, actual.getTotalHours(), 0.0005f);
}

TEST(SiderealTest, HourAngleIsThePolarisAngleInHours)
{
    const float lst      = 7.5f;
    const float expected = static_cast<float>(core::SiderealClock::calculateHaDegrees(static_cast<double>(lst), 3, 0, 8) / 15.0);

    const types::DayTime actual = core::SiderealClock::hourAngle(lst, 3, 0, 8);
    EXPECT_NEAR(expected, actual.getTotalHours(), 0.0005f);
}

TEST(SiderealClockTest, LstFromHourAngleIsTheInverseOfHourAngle)
{
    // `:SH#` sets the hour angle; the mount converts it to a sidereal time and re-anchors there.
    const float hourAngles[] = {0.0f, 1.5f, -3.25f, 11.75f, 23.5f};

    for (float ha : hourAngles)
    {
        const types::DayTime lst  = core::SiderealClock::lstFromHourAngle(types::DayTime(ha), 3, 0, 8);
        const types::DayTime back = core::SiderealClock::hourAngle(lst.getTotalHours(), 3, 0, 8);

        float delta = back.getTotalHours() - ha;
        while (delta > 12.0f)
        {
            delta -= 24.0f;
        }
        while (delta < -12.0f)
        {
            delta += 24.0f;
        }

        EXPECT_NEAR(0.0f, delta, 0.002f) << "ha=" << ha;
    }
}

TEST(SiderealClockTest, LstFromHourAngleAddsThePoleStarRightAscension)
{
    // 0h HA means the scope points at Polaris' RA, so LST equals it.
    const types::DayTime lst = core::SiderealClock::lstFromHourAngle(types::DayTime(0), 3, 0, 8);

    EXPECT_NEAR(3.0f + 8.0f / 3600.0f, lst.getTotalHours(), 0.001f);
}
