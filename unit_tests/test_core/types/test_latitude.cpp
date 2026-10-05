#include <gtest/gtest.h>
#include "core/types/Latitude.hpp"

using core::Latitude;

TEST(LatitudeTest, DefaultConstructor)
{
    Latitude lat;
    EXPECT_EQ(0, lat.getTotalSeconds());
}

TEST(LatitudeTest, DegreesConstructor)
{
    Latitude lat(45.0f);
    EXPECT_FLOAT_EQ(45.0f, lat.getTotalHours());
}

TEST(LatitudeTest, ClampUpper)
{
    Latitude lat(89.0f);
    lat.addHours(5.0f);
    EXPECT_FLOAT_EQ(90.0f, lat.getTotalHours());
}

TEST(LatitudeTest, ClampLower)
{
    Latitude lat(-89.0f);
    lat.addHours(-5.0f);
    EXPECT_FLOAT_EQ(-90.0f, lat.getTotalHours());
}

TEST(LatitudeTest, NorthPole)
{
    Latitude lat(90, 0, 0);
    EXPECT_EQ(90, lat.getHours());
    EXPECT_EQ(0, lat.getMinutes());
}

TEST(LatitudeTest, SouthPole)
{
    Latitude lat(-90, 0, 0);
    EXPECT_EQ(-90, lat.getHours());
}

TEST(LatitudeTest, SetClamps)
{
    Latitude lat;
    lat.set(95, 0, 0);
    EXPECT_FLOAT_EQ(90.0f, lat.getTotalHours());
}

TEST(LatitudeTest, CopyConstructor)
{
    Latitude lat1(45.0f);
    Latitude lat2(lat1);
    EXPECT_FLOAT_EQ(45.0f, lat2.getTotalHours());
}

TEST(LatitudeTest, AddSecondsKeepsSignBelowOneDegree)
{
    // The (h, m, s) constructor derives the sign from `h`, so it cannot build
    // a site half a degree south of the equator. Accumulating signed seconds
    // can, and the clamp in checkHours() leaves the value alone.
    Latitude lat;
    lat.addSeconds(-1800);
    EXPECT_EQ(-1800, lat.getTotalSeconds());
    EXPECT_EQ(0, lat.getHours());
    EXPECT_EQ(30, lat.getMinutes());
}
