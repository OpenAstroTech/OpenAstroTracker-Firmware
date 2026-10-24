#include <gtest/gtest.h>

#include "core/ManualSlew.hpp"

namespace
{
constexpr uint8_t bit(core::SlewDirection d)
{
    return static_cast<uint8_t>(d);
}

core::ManualSlewFacts facts()
{
    core::ManualSlewFacts f;
    f.raStepsPerDegree       = 100.0f;  // 1 h of RA = 1500 steps
    f.decStepsPerDegree      = 10.0f;
    f.raPhysicalLimitDegrees = 7.0f;
    f.raTrackingLimitDegrees = 5.0f;
    f.decLimitUpDegrees      = 135.0f;
    f.decLimitDownDegrees    = 135.0f;
    f.trackedHours           = 0.0f;
    f.northernHemisphere     = true;
    return f;
}
}  // namespace

TEST(ManualSlewTest, NorthUsesTheBuildLimitWhenNothingIsCalibrated)
{
    const core::ManualSlewTargets t = core::manualSlewTargets(bit(core::SlewDirection::North), facts());

    EXPECT_TRUE(t.movesDec);
    EXPECT_FALSE(t.movesRa);
    EXPECT_EQ(1350, t.decSteps);  // 10 steps/deg * 135 deg
}

TEST(ManualSlewTest, NorthPrefersTheCalibratedLimit)
{
    core::ManualSlewFacts f = facts();
    f.decUpperLimitSteps    = 640;

    const core::ManualSlewTargets t = core::manualSlewTargets(bit(core::SlewDirection::North), f);

    EXPECT_EQ(640, t.decSteps);
}

TEST(ManualSlewTest, SouthIsTheNegativeBuildLimit)
{
    const core::ManualSlewTargets t = core::manualSlewTargets(bit(core::SlewDirection::South), facts());

    EXPECT_TRUE(t.movesDec);
    EXPECT_EQ(-1350, t.decSteps);
}

TEST(ManualSlewTest, SouthPrefersTheCalibratedLimit)
{
    core::ManualSlewFacts f = facts();
    f.decLowerLimitSteps    = -700;

    EXPECT_EQ(-700, core::manualSlewTargets(bit(core::SlewDirection::South), f).decSteps);
}

TEST(ManualSlewTest, EastRunsAgainstTheHemisphereSign)
{
    const core::ManualSlewTargets north = core::manualSlewTargets(bit(core::SlewDirection::East), facts());

    core::ManualSlewFacts southern      = facts();
    southern.northernHemisphere         = false;
    const core::ManualSlewTargets south = core::manualSlewTargets(bit(core::SlewDirection::East), southern);

    EXPECT_TRUE(north.movesRa);
    EXPECT_FALSE(north.movesDec);
    EXPECT_EQ(-10500, north.raSteps);  // 100 * 15 * (7 + 0), negated
    EXPECT_EQ(10500, south.raSteps);   // same magnitude, opposite sign
}

TEST(ManualSlewTest, EastSubtractsTheTrackedHours)
{
    core::ManualSlewFacts f = facts();
    f.trackedHours          = 2.0f;

    // 100 * 15 * (7 - ...) — tracking extends the travel East, so it subtracts from nothing but
    // adds to the reach: (7 + 2) = 9 hours.
    EXPECT_EQ(-13500, core::manualSlewTargets(bit(core::SlewDirection::East), f).raSteps);
}

TEST(ManualSlewTest, WestIsCappedByTheTrackingLimitNotThePhysicalOne)
{
    const core::ManualSlewTargets t = core::manualSlewTargets(bit(core::SlewDirection::West), facts());

    // min(7, 5) = 5 hours, not 7.
    EXPECT_EQ(7500, t.raSteps);
}

TEST(ManualSlewTest, WestShrinksAsTrackingProceeds)
{
    core::ManualSlewFacts f = facts();
    f.trackedHours          = 3.0f;

    // min(7, 5) - 3 = 2 hours of travel left.
    EXPECT_EQ(3000, core::manualSlewTargets(bit(core::SlewDirection::West), f).raSteps);
}

TEST(ManualSlewTest, WestUsesThePhysicalLimitWhenItIsTheSmallerOne)
{
    core::ManualSlewFacts f  = facts();
    f.raTrackingLimitDegrees = 9.0f;  // larger than the ring's own travel

    EXPECT_EQ(10500, core::manualSlewTargets(bit(core::SlewDirection::West), f).raSteps);  // 7 hours
}

TEST(ManualSlewTest, ACombinedRequestProgramsBothAxes)
{
    const core::ManualSlewTargets t = core::manualSlewTargets(bit(core::SlewDirection::North) | bit(core::SlewDirection::West), facts());

    EXPECT_TRUE(t.movesDec);
    EXPECT_TRUE(t.movesRa);
    EXPECT_EQ(1350, t.decSteps);
    EXPECT_EQ(7500, t.raSteps);
}

TEST(ManualSlewTest, TrackingAsksForNoAxisTargets)
{
    // Tracking is not a manual slew; the caller handles it before asking for targets.
    const core::ManualSlewTargets t = core::manualSlewTargets(bit(core::SlewDirection::Tracking), facts());

    EXPECT_FALSE(t.movesDec);
    EXPECT_FALSE(t.movesRa);
}

TEST(ManualSlewTest, NothingRequestedMovesNothing)
{
    const core::ManualSlewTargets t = core::manualSlewTargets(0, facts());

    EXPECT_FALSE(t.movesDec);
    EXPECT_FALSE(t.movesRa);
}
