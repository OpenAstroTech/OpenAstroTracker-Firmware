#include <gtest/gtest.h>

#include "core/SlewProgressReport.hpp"

using core::SlewProgress;
using core::SlewProgressReport;

namespace
{
SlewProgress progress(float raTotal, float decTotal, long raToGo, long decToGo)
{
    return SlewProgressReport::evaluate(raTotal, decTotal, raToGo, decToGo);
}
}  // namespace

TEST(SlewProgressTest, AnIdleMountIsNotInProgressAndShowsBothAxesFinished)
{
    const SlewProgress p = progress(0.0f, 0.0f, 0, 0);

    EXPECT_FALSE(p.inProgress);
    EXPECT_EQ(100, p.raPercent);
    EXPECT_EQ(100, p.decPercent);
}

TEST(SlewProgressTest, BothAxesReportTheirOwnPercentage)
{
    // Half way on RA, three quarters on DEC.
    const SlewProgress p = progress(1000.0f, 400.0f, 500, 100);

    EXPECT_TRUE(p.inProgress);
    EXPECT_EQ(50, p.raPercent);
    EXPECT_EQ(75, p.decPercent);
}

TEST(SlewProgressTest, ADecOnlyMoveLeavesRaAtFinished)
{
    const SlewProgress p = progress(0.0f, 200.0f, 0, 50);

    EXPECT_TRUE(p.inProgress);
    EXPECT_EQ(100, p.raPercent);  // not moving: reads as done, not as 0 %
    EXPECT_EQ(75, p.decPercent);
}

TEST(SlewProgressTest, AnRaOnlyMoveLeavesDecAtFinished)
{
    const SlewProgress p = progress(800.0f, 0.0f, 200, 0);

    EXPECT_TRUE(p.inProgress);
    EXPECT_EQ(75, p.raPercent);
    EXPECT_EQ(100, p.decPercent);
}

TEST(SlewProgressTest, ATotalMoveBelowTheEpsilonIsNotAMove)
{
    // Rounding noise must not produce a progress bar that never fills.
    const SlewProgress p = progress(0.0005f, -0.0009f, 0, 0);

    EXPECT_FALSE(p.inProgress);
    EXPECT_EQ(100, p.raPercent);
    EXPECT_EQ(100, p.decPercent);
}

TEST(SlewProgressTest, TheEpsilonBoundaryIsExclusive)
{
    EXPECT_TRUE(progress(SlewProgressReport::kEpsilon * 2.0f, 0.0f, 0, 0).inProgress);
    EXPECT_FALSE(progress(SlewProgressReport::kEpsilon, 0.0f, 0, 0).inProgress);
}

TEST(SlewProgressTest, ARetargetThatOvershootsStillReportsAReasonablePercentage)
{
    // distanceToGo is signed: past the target it is negative, so the percentage exceeds 100
    // before the arrival sequence runs. The original arithmetic is preserved.
    const SlewProgress p = progress(100.0f, 0.0f, -10, 0);

    EXPECT_TRUE(p.inProgress);
    EXPECT_EQ(110, p.raPercent);
}
