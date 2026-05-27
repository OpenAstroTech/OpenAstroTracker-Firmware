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
