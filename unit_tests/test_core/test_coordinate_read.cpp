#include <gtest/gtest.h>

#include "core/CoordinateRead.hpp"

namespace
{

core::PositionFacts factsAt(long raSteps, long decSteps, float zeroPosRaHours = 0.0f, float zeroPosDecDegrees = 0.0f)
{
    core::PositionFacts facts;
    facts.raSteps           = raSteps;
    facts.decSteps          = decSteps;
    facts.raStepsPerDegree  = 360.0f;  // 1 degree per step keeps the arithmetic readable
    facts.decStepsPerDegree = 360.0f;
    facts.zeroPosRaHours    = zeroPosRaHours;
    facts.zeroPosDecDegrees = zeroPosDecDegrees;
    return facts;
}

}  // namespace

TEST(CoordinateReadTest, ZeroStepsReadsTheZeroPoint)
{
    const types::DayTime ra = core::currentRa(factsAt(0, 0, 5.0f, 10.0f));
    EXPECT_NEAR(5.0f, ra.getTotalHours(), 0.001f);

    const types::Declination dec = core::currentDec(factsAt(0, 0, 5.0f, 10.0f));
    EXPECT_NEAR(10.0f, dec.getTotalHours(), 0.001f);
}

TEST(CoordinateReadTest, RaMovesBackwardsOneDegreeAtATime)
{
    // 360 steps at 360 steps/degree is one degree, i.e. 1 / 14.959 hours of RA. The sign is
    // negative because the RA ring turns the other way.
    const float hoursPerDegree = 1.0f / core::kSiderealDegreesPerHour;
    const types::DayTime ra    = core::currentRa(factsAt(360, 0, 1.0f, 0.0f));
    EXPECT_NEAR(1.0f - hoursPerDegree, ra.getTotalHours(), 0.001f);
}

TEST(CoordinateReadTest, DecBelowThePoleReadsTheOtherSideOfTheSky)
{
    // A negative DEC adds twelve hours, folded back into 0..24.
    const types::DayTime ra = core::currentRa(factsAt(0, 0, 1.0f, -10.0f));
    EXPECT_NEAR(13.0f, ra.getTotalHours(), 0.001f);
}

TEST(CoordinateReadTest, TheTwelveHourFlipWrapsPastMidnight)
{
    const types::DayTime ra = core::currentRa(factsAt(0, 0, 20.0f, -10.0f));
    EXPECT_NEAR(8.0f, ra.getTotalHours(), 0.001f);
}

TEST(CoordinateReadTest, DeclinationIncludesTheZeroOffset)
{
    const types::Declination dec = core::currentDec(factsAt(0, 360, 0.0f, 5.0f));
    EXPECT_NEAR(6.0f, dec.getTotalHours(), 0.001f);
}

TEST(CoordinateReadTest, HourAngleWrapsWhenItRunsPastTwentyFourHours)
{
    // Zero point just short of midnight; negative RA steps push the hour angle past it.
    core::PositionFacts facts = factsAt(-1000, 0, 23.99f);
    facts.raStepsPerDegree    = 360.0f;

    const float hours = core::currentRa(facts).getTotalHours();

    EXPECT_LT(hours, 1.0f) << "should have wrapped into the new day, was " << hours;
    EXPECT_GE(hours, 0.0f);
}

TEST(CoordinateReadTest, HourAngleWrapsWhenItRunsBelowZero)
{
    // Zero point just after midnight; positive RA steps take the hour angle negative.
    core::PositionFacts facts = factsAt(1000, 0, 0.1f);
    facts.raStepsPerDegree    = 360.0f;

    const float hours = core::currentRa(facts).getTotalHours();

    EXPECT_GT(hours, 23.0f) << "should have wrapped to the previous day, was " << hours;
    EXPECT_LE(hours, 24.0f);
}
