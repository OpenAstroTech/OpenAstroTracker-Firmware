#include <gtest/gtest.h>
#include "core/types/Declination.hpp"

using core::Declination;

TEST(DeclinationTest, DefaultConstructor)
{
    Declination dec;
    EXPECT_EQ(0, dec.getTotalSeconds());
}

TEST(DeclinationTest, DegreesConstructor)
{
    Declination dec(15.0f);
    EXPECT_FLOAT_EQ(15.0f, dec.getTotalDegrees());
}

TEST(DeclinationTest, AddDegrees)
{
    Declination dec(10.0f);
    dec.addDegrees(5);
    EXPECT_FLOAT_EQ(15.0f, dec.getTotalDegrees());
}

TEST(DeclinationTest, SubtractDegrees)
{
    Declination dec(10.0f);
    dec.addDegrees(-25);
    EXPECT_FLOAT_EQ(-15.0f, dec.getTotalDegrees());
}

TEST(DeclinationTest, ClampUpper)
{
    Declination dec(179.0f);
    dec.addDegrees(5);
    EXPECT_FLOAT_EQ(180.0f, dec.getTotalDegrees());
}

TEST(DeclinationTest, ClampLower)
{
    Declination dec(-179.0f);
    dec.addDegrees(-5);
    EXPECT_FLOAT_EQ(-180.0f, dec.getTotalDegrees());
}

TEST(DeclinationTest, SetClamps)
{
    Declination dec;
    dec.set(200, 0, 0);
    EXPECT_FLOAT_EQ(180.0f, dec.getTotalDegrees());
}

TEST(DeclinationTest, CopyConstructor)
{
    Declination dec1(45.0f);
    Declination dec2(dec1);
    EXPECT_FLOAT_EQ(45.0f, dec2.getTotalDegrees());
}

TEST(DeclinationTest, GetTotalDegrees)
{
    Declination dec(30, 0, 0);
    EXPECT_FLOAT_EQ(30.0f, dec.getTotalDegrees());
}
