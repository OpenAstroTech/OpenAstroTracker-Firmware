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

TEST(DeclinationTest, CelestialWireRoundTrip)
{
    // Reproduces the composition that Declination::fromCelestialDegrees and
    // Declination::getCelestialDegrees perform for the Meade :Sd/:Gd/:CM
    // commands. src/Declination.cpp itself needs Arduino String and the board
    // configuration, so it cannot be linked here: this pins that the sequence
    // is correct, not that src/Declination.cpp still uses it.
    struct WireDec {
        int deg;
        int min;
        int sec;
    };
    const WireDec cases[]    = {{-5, 30, 0}, {-24, 23, 0}, {-69, 6, 0}, {-89, 59, 59}, {5, 30, 0}, {0, 30, 0}, {45, 0, 0}, {89, 59, 59}};
    const bool hemispheres[] = {true, false};

    for (bool north : hemispheres)
    {
        for (const WireDec &wire : cases)
        {
            const long celestial = core::DayTime::joinSeconds(wire.deg, wire.min, wire.sec);
            const Declination onAxis(Declination::fromTotalSeconds(Declination::celestialToAxisSeconds(celestial, north)));

            int deg, min, sec;
            core::DayTime::splitSeconds(Declination::axisToCelestialSeconds(onAxis.getTotalSeconds(), north), deg, min, sec);

            EXPECT_EQ(wire.deg, deg) << "north=" << north << " deg=" << wire.deg;
            EXPECT_EQ(wire.min, min) << "north=" << north << " deg=" << wire.deg;
            EXPECT_EQ(wire.sec, sec) << "north=" << north << " deg=" << wire.deg;
        }
    }
}

TEST(DeclinationTest, CelestialSecondsFromKeepsTheSignOfZeroDegrees)
{
    // The magnitude is unsigned and the sign is separate, so a coordinate
    // inside the first degree south of the equator survives the join.
    EXPECT_EQ(-1800L, Declination::celestialSecondsFrom(0, 30, 0, true));
    EXPECT_EQ(1800L, Declination::celestialSecondsFrom(0, 30, 0, false));
    EXPECT_EQ(-59L, Declination::celestialSecondsFrom(0, 0, 59, true));

    // Exact zero has no sign to keep, either way round.
    EXPECT_EQ(0L, Declination::celestialSecondsFrom(0, 0, 0, true));
    EXPECT_EQ(0L, Declination::celestialSecondsFrom(0, 0, 0, false));

    // Whole degrees still join the way the signed-degrees form did.
    EXPECT_EQ(core::DayTime::joinSeconds(-5, 30, 0), Declination::celestialSecondsFrom(5, 30, 0, true));
    EXPECT_EQ(core::DayTime::joinSeconds(89, 59, 59), Declination::celestialSecondsFrom(89, 59, 59, false));
}

TEST(DeclinationTest, ZeroDegreesSouthLandsOneDegreeFromWhereSignedDegreesPutIt)
{
    // Pins the defect this pairing exists to close. MeadeCommandProcessor's
    // decFromWire used to flatten the parser's sign flag back into a signed
    // `deg`, so "-00*30:00" reached the join as joinSeconds(0, 30, 0) -- the
    // same value as "+00*30:00", one whole degree away from the truth.
    const long viaSignedDegrees = Declination::celestialToAxisSeconds(core::DayTime::joinSeconds(0, 30, 0), true);
    const long viaSeparateSign  = Declination::celestialToAxisSeconds(Declination::celestialSecondsFrom(0, 30, 0, true), true);

    EXPECT_EQ(322200L, viaSignedDegrees);
    EXPECT_EQ(325800L, viaSeparateSign);
    EXPECT_EQ(3600L, viaSeparateSign - viaSignedDegrees);

    // Southern mounts have the same blind spot on the same input, mirrored:
    // the signed-degrees path is the one that lands 1 degree out.
    EXPECT_EQ(-325800L, Declination::celestialToAxisSeconds(core::DayTime::joinSeconds(0, 30, 0), false));
    EXPECT_EQ(-322200L, Declination::celestialToAxisSeconds(Declination::celestialSecondsFrom(0, 30, 0, true), false));
}

TEST(DeclinationTest, CelestialSecondsFromRoundTripsThroughTheAxis)
{
    struct WireDec {
        uint16_t deg;
        uint8_t min;
        uint8_t sec;
        bool negative;
    };
    const WireDec cases[] = {
        {0, 30, 0, true}, {0, 30, 0, false}, {0, 0, 1, true}, {5, 30, 0, true}, {24, 23, 0, true}, {89, 59, 59, true}, {89, 59, 59, false}};
    const bool hemispheres[] = {true, false};

    for (bool north : hemispheres)
    {
        for (const WireDec &wire : cases)
        {
            const long celestial = Declination::celestialSecondsFrom(wire.deg, wire.min, wire.sec, wire.negative);
            const Declination onAxis(Declination::fromTotalSeconds(Declination::celestialToAxisSeconds(celestial, north)));

            EXPECT_EQ(celestial, Declination::axisToCelestialSeconds(onAxis.getTotalSeconds(), north))
                << "north=" << north << " deg=" << wire.deg << " negative=" << wire.negative;
        }
    }
}
