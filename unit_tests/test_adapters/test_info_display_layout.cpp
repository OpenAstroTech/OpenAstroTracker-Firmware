#include <gtest/gtest.h>

#include "adapters/InfoDisplayLayout.hpp"

using adapters::InfoDisplayScale;

namespace
{

/// The tracker's scale: half the DEC circle, the panel's own pixel layout.
constexpr InfoDisplayScale kTrackerScale = {69, 41, 51, 14, 43, 115, -6.0f, 6.0f, -90.0f, 270.0f};

/// The mount/explorer scale: the whole DEC circle.
constexpr InfoDisplayScale kFullCircleScale = {69, 41, 51, 14, 43, 115, -6.0f, 6.0f, -180.0f, 360.0f};

}  // namespace

TEST(InfoDisplayLayoutTest, RaScaleSpansTheSixHourWindow)
{
    EXPECT_EQ(73, adapters::xRAPixel(kTrackerScale, -6.0f));
    EXPECT_EQ(89, adapters::xRAPixel(kTrackerScale, 0.0f));
    EXPECT_EQ(105, adapters::xRAPixel(kTrackerScale, 6.0f));
}

TEST(InfoDisplayLayoutTest, DecScaleForHalfTheCircle)
{
    EXPECT_EQ(57, adapters::yDECPixel(kTrackerScale, -90.0f));
    EXPECT_EQ(43, adapters::yDECPixel(kTrackerScale, 0.0f));
    EXPECT_EQ(28, adapters::yDECPixel(kTrackerScale, 90.0f));
    EXPECT_EQ(14, adapters::yDECPixel(kTrackerScale, 180.0f));
}

TEST(InfoDisplayLayoutTest, DecScaleForTheWholeCircle)
{
    EXPECT_EQ(57, adapters::yDECPixel(kFullCircleScale, -180.0f));
    EXPECT_EQ(35, adapters::yDECPixel(kFullCircleScale, 0.0f));
    EXPECT_EQ(14, adapters::yDECPixel(kFullCircleScale, 180.0f));
}

TEST(InfoDisplayLayoutTest, ProgressBarIsAFullWidthAtOneHundredPercent)
{
    EXPECT_EQ(0, adapters::progressBarWidth(0));
    EXPECT_EQ(13, adapters::progressBarWidth(10));
    EXPECT_EQ(64, adapters::progressBarWidth(50));
    EXPECT_EQ(128, adapters::progressBarWidth(100));
}

TEST(InfoDisplayLayoutTest, RaMarkerCombinesTheTrackingAndSlewCounters)
{
    // A 2:1 microstep ratio means the tracking counter moves half as far per degree.
    EXPECT_FLOAT_EQ(0.0f, adapters::markerRaHours(0, 0, 120.0f, 32, 16));
    EXPECT_FLOAT_EQ(1.0f, adapters::markerRaHours(0, 1800, 120.0f, 32, 16));
    EXPECT_FLOAT_EQ(0.75f, adapters::markerRaHours(900, 900, 120.0f, 32, 16));
}

TEST(InfoDisplayLayoutTest, DecMarkerDividesStepsByStepsPerDegree)
{
    EXPECT_FLOAT_EQ(45.0f, adapters::markerDecDegrees(4500, 100.0f));
    EXPECT_FLOAT_EQ(-12.5f, adapters::markerDecDegrees(-1250, 100.0f));
}
