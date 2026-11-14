#include <gtest/gtest.h>
#include "core/CoordinateMath.hpp"

using core::calculateStepperPositions;
using core::SlewGeometry;
using core::SlewResult;

// Helper: create a default geometry for testing
static SlewGeometry makeGeo(float moveRA, float moveDEC, float deltaRA)
{
    SlewGeometry geo;
    geo.moveRA               = moveRA;
    geo.moveDEC              = moveDEC;
    geo.homeTargetDeltaRA    = deltaRA;
    geo.stepsPerSiderealHour = 1000.0f;
    geo.stepsPerDECDegree    = 500.0f;
    geo.zeroPosDEC           = 0.0f;
    geo.raLimitL             = -6.0f;
    geo.raLimitR             = 6.0f;
    return geo;
}

TEST(CoordinateMathTest, Solution1Direct)
{
    // Target within limits: no flip needed
    SlewGeometry geo  = makeGeo(3.0f, 45.0f, 2.0f);
    SlewResult result = calculateStepperPositions(geo);

    // moveRA = 3.0, moveDEC = 45.0
    EXPECT_EQ(static_cast<long>(-3.0f * 1000.0f), result.targetRASteps);
    EXPECT_EQ(static_cast<long>(45.0f * 500.0f), result.targetDECSteps);
}

TEST(CoordinateMathTest, Solution2FlipUpper)
{
    // Past upper limit: flip (moveRA -= 12, moveDEC = -moveDEC)
    SlewGeometry geo  = makeGeo(4.0f, 30.0f, 7.0f);  // deltaRA=7 > raLimitR=6
    SlewResult result = calculateStepperPositions(geo);

    // moveRA = 4 - 12 = -8, moveDEC = -30
    EXPECT_EQ(static_cast<long>(-(-8.0f) * 1000.0f), result.targetRASteps);
    EXPECT_EQ(static_cast<long>((-30.0f) * 500.0f), result.targetDECSteps);
}

TEST(CoordinateMathTest, Solution3FlipLower)
{
    // Past lower limit: flip (moveRA += 12, moveDEC = -moveDEC)
    SlewGeometry geo  = makeGeo(-4.0f, 20.0f, -7.0f);  // deltaRA=-7 < raLimitL=-6
    SlewResult result = calculateStepperPositions(geo);

    // moveRA = -4 + 12 = 8, moveDEC = -20
    EXPECT_EQ(static_cast<long>(-8.0f * 1000.0f), result.targetRASteps);
    EXPECT_EQ(static_cast<long>((-20.0f) * 500.0f), result.targetDECSteps);
}

TEST(CoordinateMathTest, ZeroPosDecOffset)
{
    SlewGeometry geo  = makeGeo(2.0f, 30.0f, 1.0f);
    geo.zeroPosDEC    = 5.0f;  // accumulated sync offset
    SlewResult result = calculateStepperPositions(geo);

    // moveDEC = 30 - 5 = 25
    EXPECT_EQ(static_cast<long>(25.0f * 500.0f), result.targetDECSteps);
}

TEST(CoordinateMathTest, SolutionsArray)
{
    SlewGeometry geo  = makeGeo(2.0f, 45.0f, 1.0f);
    SlewResult result = calculateStepperPositions(geo);

    // Solution 1
    EXPECT_EQ(static_cast<long>(-2.0f * 1000.0f), result.solutions[0]);
    EXPECT_EQ(static_cast<long>(45.0f * 500.0f), result.solutions[1]);
    // Solution 2
    EXPECT_EQ(static_cast<long>(-(2.0f - 12.0f) * 1000.0f), result.solutions[2]);
    EXPECT_EQ(static_cast<long>(-45.0f * 500.0f), result.solutions[3]);
    // Solution 3
    EXPECT_EQ(static_cast<long>(-(2.0f + 12.0f) * 1000.0f), result.solutions[4]);
    EXPECT_EQ(static_cast<long>(-45.0f * 500.0f), result.solutions[5]);
}

TEST(CoordinateMathTest, BoundaryExactlyAtLimit)
{
    // Exactly at the limit: no flip (only > triggers, not >=)
    SlewGeometry geo  = makeGeo(3.0f, 30.0f, 6.0f);
    SlewResult result = calculateStepperPositions(geo);

    // Should use solution 1 (direct)
    EXPECT_EQ(static_cast<long>(-3.0f * 1000.0f), result.targetRASteps);
    EXPECT_EQ(static_cast<long>(30.0f * 500.0f), result.targetDECSteps);
}

// Expected values are derived from the length of the sidereal day (86164.0905 s), not from the code.
TEST(UntrackedRaHoursTest, ZeroWhileTrackingWithNoOffTime)
{
    EXPECT_FLOAT_EQ(0.0f, core::untrackedRaHours(0.0f, 0UL));
}

TEST(UntrackedRaHoursTest, RunningOffPeriodAdvancesAtSiderealRate)
{
    // 30 s stopped (FW12, night 2): RA must advance 30 s * 86400 / 86164.0905 = 30.0821 s
    EXPECT_NEAR(30.0821f / 3600.0f, core::untrackedRaHours(0.0f, 30000UL), 1e-6f);
    // One sidereal day stopped is exactly 24 h of RA
    EXPECT_NEAR(24.0f, core::untrackedRaHours(0.0f, 86164091UL), 1e-4f);
}

TEST(UntrackedRaHoursTest, CompletedAndRunningOffTimeAdd)
{
    // 1 h completed + 1 h running = 2 solar hours = 2.0054758 RA hours
    EXPECT_NEAR(2.0054758f, core::untrackedRaHours(1.0f, 3600000UL), 1e-5f);
}

TEST(UntrackedRaHoursTest, CatchUpCancelsTheSlewTime)
{
    // A 20 s slew whose catch-up (20 s) has already been subtracted leaves nothing
    EXPECT_NEAR(0.0f, core::untrackedRaHours(-20.0f / 3600.0f, 20000UL), 1e-6f);
}
