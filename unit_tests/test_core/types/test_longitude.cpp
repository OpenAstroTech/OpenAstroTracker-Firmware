#include <gtest/gtest.h>
#include "core/types/Longitude.hpp"

using core::Longitude;

TEST(LongitudeTest, DefaultConstructor)
{
    Longitude lon;
    EXPECT_EQ(0, lon.getTotalSeconds());
}

TEST(LongitudeTest, DegreesConstructor)
{
    Longitude lon(100.0f);
    EXPECT_FLOAT_EQ(100.0f, lon.getTotalHours());
}

TEST(LongitudeTest, WrapUpper)
{
    Longitude lon(179.0f);
    lon.addHours(5.0f);
    EXPECT_FLOAT_EQ(-176.0f, lon.getTotalHours());
}

TEST(LongitudeTest, WrapLower)
{
    Longitude lon(-179.0f);
    lon.addHours(-5.0f);
    EXPECT_FLOAT_EQ(176.0f, lon.getTotalHours());
}

TEST(LongitudeTest, NegativeWest)
{
    Longitude lon(-75.0f);
    EXPECT_FLOAT_EQ(-75.0f, lon.getTotalHours());
}

TEST(LongitudeTest, PositiveEast)
{
    Longitude lon(120.0f);
    EXPECT_FLOAT_EQ(120.0f, lon.getTotalHours());
}

TEST(LongitudeTest, WrapAt180)
{
    // Constructor doesn't call checkHours(); set() does.
    Longitude lon(181, 0, 0);
    EXPECT_FLOAT_EQ(181.0f, lon.getTotalHours());
    lon.set(181, 0, 0);
    EXPECT_FLOAT_EQ(-179.0f, lon.getTotalHours());
}

TEST(LongitudeTest, CopyConstructor)
{
    Longitude lon1(50.0f);
    Longitude lon2(lon1);
    EXPECT_FLOAT_EQ(50.0f, lon2.getTotalHours());
}
