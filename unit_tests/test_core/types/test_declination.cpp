#include <gtest/gtest.h>
#include "core/types/Declination.hpp"

using core::Declination;

TEST(DeclinationTest, DefaultConstructor)
{
    Declination dec;
    EXPECT_EQ(0, dec.getTotalSeconds());
}

TEST(DeclinationTest, DegreesConstructor)
{
    Declination dec(15.0f);
    EXPECT_FLOAT_EQ(15.0f, dec.getTotalDegrees());
}

TEST(DeclinationTest, AddDegrees)
{
    Declination dec(10.0f);
    dec.addDegrees(5);
    EXPECT_FLOAT_EQ(15.0f, dec.getTotalDegrees());
}

TEST(DeclinationTest, SubtractDegrees)
{
    Declination dec(10.0f);
    dec.addDegrees(-25);
    EXPECT_FLOAT_EQ(-15.0f, dec.getTotalDegrees());
}

TEST(DeclinationTest, ClampUpper)
{
    Declination dec(179.0f);
    dec.addDegrees(5);
    EXPECT_FLOAT_EQ(180.0f, dec.getTotalDegrees());
}

TEST(DeclinationTest, ClampLower)
{
    Declination dec(-179.0f);
    dec.addDegrees(-5);
    EXPECT_FLOAT_EQ(-180.0f, dec.getTotalDegrees());
}

TEST(DeclinationTest, SetClamps)
{
    Declination dec;
    dec.set(200, 0, 0);
    EXPECT_FLOAT_EQ(180.0f, dec.getTotalDegrees());
}

TEST(DeclinationTest, CopyConstructor)
{
    Declination dec1(45.0f);
    Declination dec2(dec1);
    EXPECT_FLOAT_EQ(45.0f, dec2.getTotalDegrees());
}

TEST(DeclinationTest, GetTotalDegrees)
{
    Declination dec(30, 0, 0);
    EXPECT_FLOAT_EQ(30.0f, dec.getTotalDegrees());
}

// ---------------------------------------------------------------------------
// Hemisphere conversion (mount-axis coordinate <-> celestial declination).
//
// The mount stores DEC as an axis coordinate: 0 at the pole above the mount,
// +/-180 at the opposite pole. Meade clients speak celestial declination.
// These tests pin the exact relationship documented in src/Declination.cpp.
// ---------------------------------------------------------------------------

TEST(DeclinationTest, AxisToCelestialNorthernPole)
{
    // Axis 0 is the north celestial pole in the northern hemisphere.
    EXPECT_EQ(90L * 3600L, Declination::axisToCelestialSeconds(0, true));
}

TEST(DeclinationTest, AxisToCelestialNorthernEquator)
{
    // Both equator crossings (+90 and -90 axis) are celestial 0.
    EXPECT_EQ(0L, Declination::axisToCelestialSeconds(90L * 3600L, true));
    EXPECT_EQ(0L, Declination::axisToCelestialSeconds(-90L * 3600L, true));
}

TEST(DeclinationTest, AxisToCelestialNorthernSouthPole)
{
    EXPECT_EQ(-90L * 3600L, Declination::axisToCelestialSeconds(180L * 3600L, true));
    EXPECT_EQ(-90L * 3600L, Declination::axisToCelestialSeconds(-180L * 3600L, true));
}

TEST(DeclinationTest, AxisToCelestialSouthernPole)
{
    // Axis 0 is the south celestial pole in the southern hemisphere.
    EXPECT_EQ(-90L * 3600L, Declination::axisToCelestialSeconds(0, false));
}

TEST(DeclinationTest, AxisToCelestialSouthernEquator)
{
    EXPECT_EQ(0L, Declination::axisToCelestialSeconds(90L * 3600L, false));
    EXPECT_EQ(0L, Declination::axisToCelestialSeconds(-90L * 3600L, false));
}

TEST(DeclinationTest, AxisToCelestialSouthernNorthPole)
{
    EXPECT_EQ(90L * 3600L, Declination::axisToCelestialSeconds(180L * 3600L, false));
    EXPECT_EQ(90L * 3600L, Declination::axisToCelestialSeconds(-180L * 3600L, false));
}

TEST(DeclinationTest, AxisToCelestialNorthernSignFlip)
{
    // Northern mount with axis +100 points 10 degrees below the equator:
    // celestial sign is negative while the axis coordinate stays positive.
    EXPECT_EQ(-10L * 3600L, Declination::axisToCelestialSeconds(100L * 3600L, true));
}

TEST(DeclinationTest, CelestialToAxisNorthern)
{
    EXPECT_EQ(10L * 3600L, Declination::celestialToAxisSeconds(80L * 3600L, true));
    EXPECT_EQ(0L, Declination::celestialToAxisSeconds(90L * 3600L, true));
    EXPECT_EQ(180L * 3600L, Declination::celestialToAxisSeconds(-90L * 3600L, true));
}

TEST(DeclinationTest, CelestialToAxisSouthern)
{
    EXPECT_EQ(-10L * 3600L, Declination::celestialToAxisSeconds(-80L * 3600L, false));
    EXPECT_EQ(0L, Declination::celestialToAxisSeconds(-90L * 3600L, false));
    EXPECT_EQ(-180L * 3600L, Declination::celestialToAxisSeconds(90L * 3600L, false));
}

TEST(DeclinationTest, CelestialAxisRoundTrip)
{
    // The axis->celestial mapping is two-to-one (|axis|): the arm at +100 and
    // -100 both point at celestial -10, on opposite sides of the meridian.
    // The inverse maps back to the home branch only: non-negative axis in the
    // northern hemisphere, non-positive in the southern.
    for (long axis = 0; axis <= 180L * 3600L; axis += 1800L)
    {
        const long celestial = Declination::axisToCelestialSeconds(axis, true);
        EXPECT_EQ(axis, Declination::celestialToAxisSeconds(celestial, true)) << "axis=" << axis;
    }
    for (long axis = 0; axis >= -180L * 3600L; axis -= 1800L)
    {
        const long celestial = Declination::axisToCelestialSeconds(axis, false);
        EXPECT_EQ(axis, Declination::celestialToAxisSeconds(celestial, false)) << "axis=" << axis;
    }
}

TEST(DeclinationTest, FromTotalSecondsClamps)
{
    EXPECT_EQ(180L * 3600L, Declination::fromTotalSeconds(200L * 3600L).getTotalSeconds());
    EXPECT_EQ(-180L * 3600L, Declination::fromTotalSeconds(-200L * 3600L).getTotalSeconds());
    EXPECT_EQ(12345L, Declination::fromTotalSeconds(12345L).getTotalSeconds());
}
