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

TEST(CoordinateMathTest, NorthPoleTarget)
{
    SlewGeometry geo  = makeGeo(3.0f, 90.0f, 2.0f);
    SlewResult result = calculateStepperPositions(geo);

    EXPECT_EQ(45000L, result.targetDECSteps);
    EXPECT_FLOAT_EQ(90.0f, result.adjustedDEC);
}

TEST(CoordinateMathTest, SouthPoleTarget)
{
    SlewGeometry geo  = makeGeo(3.0f, -90.0f, 2.0f);
    SlewResult result = calculateStepperPositions(geo);

    EXPECT_EQ(-45000L, result.targetDECSteps);
    EXPECT_FLOAT_EQ(-90.0f, result.adjustedDEC);
}

TEST(CoordinateMathTest, PoleTargetWithMeridianFlip)
{
    // Past the upper RA limit the DEC axis is inverted, pole targets included
    SlewGeometry geo  = makeGeo(4.0f, 90.0f, 7.0f);
    SlewResult result = calculateStepperPositions(geo);

    EXPECT_EQ(-45000L, result.targetDECSteps);
    EXPECT_FLOAT_EQ(-90.0f, result.adjustedDEC);
}

TEST(CoordinateMathTest, ReportsSelectedSolution)
{
    EXPECT_EQ(core::SlewSolution::Direct, calculateStepperPositions(makeGeo(3.0f, 45.0f, 2.0f)).selected);
    EXPECT_EQ(core::SlewSolution::FlipUpper, calculateStepperPositions(makeGeo(4.0f, 30.0f, 7.0f)).selected);
    EXPECT_EQ(core::SlewSolution::FlipLower, calculateStepperPositions(makeGeo(-4.0f, 20.0f, -7.0f)).selected);
    EXPECT_EQ(core::SlewSolution::Direct, calculateStepperPositions(makeGeo(3.0f, 30.0f, 6.0f)).selected);
}

TEST(CoordinateMathTest, ReportsAdjustedCoordinatesBeforeZeroOffset)
{
    // FlipUpper: RA -12, DEC negated; adjusted values exclude the zeroPosDEC offset
    SlewGeometry geo  = makeGeo(4.0f, 30.0f, 7.0f);
    geo.zeroPosDEC    = 5.0f;
    SlewResult result = calculateStepperPositions(geo);

    EXPECT_FLOAT_EQ(-8.0f, result.adjustedRA);
    EXPECT_FLOAT_EQ(-30.0f, result.adjustedDEC);
    // The offset still lands in the target
    EXPECT_EQ(static_cast<long>((-30.0f - 5.0f) * 500.0f), result.targetDECSteps);

    // Direct: unchanged
    SlewResult direct = calculateStepperPositions(makeGeo(3.0f, 45.0f, 2.0f));
    EXPECT_FLOAT_EQ(3.0f, direct.adjustedRA);
    EXPECT_FLOAT_EQ(45.0f, direct.adjustedDEC);
}

TEST(CoordinateMathTest, CandidateSolutionsUseWholeRoundedHours)
{
    // The candidates truncate RA to whole hours before scaling (legacy behaviour),
    // while the selected target scales the unrounded delta.
    SlewGeometry geo  = makeGeo(2.5f, 45.5f, 1.0f);
    SlewResult result = calculateStepperPositions(geo);

    // -2 (not -2.5) hours * 1000 = -2000
    EXPECT_EQ(-2000L, result.solutions[0]);
    // 45 (not 45.5) degrees * 500 = 22500
    EXPECT_EQ(22500L, result.solutions[1]);
    // The real target keeps the fraction
    EXPECT_EQ(-2500L, result.targetRASteps);
    EXPECT_EQ(22750L, result.targetDECSteps);
}

TEST(CoordinateMathTest, CandidateSolutionsScaleBeforeTruncating)
{
    // A non-integral steps-per-hour value is multiplied in float, not truncated first
    SlewGeometry geo         = makeGeo(3.0f, 10.0f, 0.0f);
    geo.stepsPerSiderealHour = 1000.5f;
    geo.stepsPerDECDegree    = 500.5f;
    SlewResult result        = calculateStepperPositions(geo);

    EXPECT_EQ(-3001L, result.solutions[0]);  // long(-3 * 1000.5) == -3001
    EXPECT_EQ(5005L, result.solutions[1]);   // long(10 * 500.5) == 5005
}
