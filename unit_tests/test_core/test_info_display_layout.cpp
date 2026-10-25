#include <gtest/gtest.h>

#include "core/InfoDisplayLayout.hpp"

using core::InfoDisplayScale;

namespace
{

/// The tracker's scale: half the DEC circle, the panel's own pixel layout.
constexpr InfoDisplayScale kTrackerScale = {69, 41, 51, 14, 43, 115, -6.0f, 6.0f, -90.0f, 270.0f};

/// The mount/explorer scale: the whole DEC circle.
constexpr InfoDisplayScale kFullCircleScale = {69, 41, 51, 14, 43, 115, -6.0f, 6.0f, -180.0f, 360.0f};

}  // namespace

TEST(InfoDisplayLayoutTest, RaScaleSpansTheSixHourWindow)
{
    EXPECT_EQ(73, core::xRAPixel(kTrackerScale, -6.0f));
    EXPECT_EQ(89, core::xRAPixel(kTrackerScale, 0.0f));
    EXPECT_EQ(105, core::xRAPixel(kTrackerScale, 6.0f));
}

TEST(InfoDisplayLayoutTest, DecScaleForHalfTheCircle)
{
    EXPECT_EQ(57, core::yDECPixel(kTrackerScale, -90.0f));
    EXPECT_EQ(43, core::yDECPixel(kTrackerScale, 0.0f));
    EXPECT_EQ(28, core::yDECPixel(kTrackerScale, 90.0f));
    EXPECT_EQ(14, core::yDECPixel(kTrackerScale, 180.0f));
}

TEST(InfoDisplayLayoutTest, DecScaleForTheWholeCircle)
{
    EXPECT_EQ(57, core::yDECPixel(kFullCircleScale, -180.0f));
    EXPECT_EQ(35, core::yDECPixel(kFullCircleScale, 0.0f));
    EXPECT_EQ(14, core::yDECPixel(kFullCircleScale, 180.0f));
}

TEST(InfoDisplayLayoutTest, ProgressBarIsAFullWidthAtOneHundredPercent)
{
    EXPECT_EQ(0, core::progressBarWidth(0));
    EXPECT_EQ(13, core::progressBarWidth(10));
    EXPECT_EQ(64, core::progressBarWidth(50));
    EXPECT_EQ(128, core::progressBarWidth(100));
}

TEST(InfoDisplayLayoutTest, RaMarkerCombinesTheTrackingAndSlewCounters)
{
    // A 2:1 microstep ratio means the tracking counter moves half as far per degree.
    EXPECT_FLOAT_EQ(0.0f, core::markerRaHours(0, 0, 120.0f, 32, 16));
    EXPECT_FLOAT_EQ(1.0f, core::markerRaHours(0, 1800, 120.0f, 32, 16));
    EXPECT_FLOAT_EQ(0.75f, core::markerRaHours(900, 900, 120.0f, 32, 16));
}

TEST(InfoDisplayLayoutTest, DecMarkerDividesStepsByStepsPerDegree)
{
    EXPECT_FLOAT_EQ(45.0f, core::markerDecDegrees(4500, 100.0f));
    EXPECT_FLOAT_EQ(-12.5f, core::markerDecDegrees(-1250, 100.0f));
}
