#include <gtest/gtest.h>
#include "core/types/RightAscension.hpp"

using core::RightAscension;

TEST(RightAscensionTest, DefaultConstructor)
{
    RightAscension ra;
    EXPECT_EQ(0, ra.getTotalSeconds());
    EXPECT_EQ(0, ra.getHours());
}

TEST(RightAscensionTest, HMSConstructor)
{
    RightAscension ra(12, 34, 56);
    EXPECT_EQ(12, ra.getHours());
    EXPECT_EQ(34, ra.getMinutes());
    EXPECT_EQ(56, ra.getSeconds());
}

TEST(RightAscensionTest, FloatConstructor)
{
    RightAscension ra(18.5f);  // 18h 30m
    EXPECT_EQ(18, ra.getHours());
    EXPECT_EQ(30, ra.getMinutes());
}

TEST(RightAscensionTest, WrapAt24Hours)
{
    RightAscension ra(23, 30, 0);
    ra.addHours(2.0f);
    EXPECT_EQ(1, ra.getHours());
    EXPECT_EQ(30, ra.getMinutes());
}

TEST(RightAscensionTest, WrapNegativeToPositive)
{
    RightAscension ra(1, 0, 0);
    ra.addHours(-3.0f);
    EXPECT_EQ(22, ra.getHours());
}

TEST(RightAscensionTest, ConstructorWraps)
{
    // Constructor calls checkHours() so 25h wraps to 1h
    RightAscension ra(25, 0, 0);
    EXPECT_EQ(1, ra.getHours());
}

TEST(RightAscensionTest, SetWraps)
{
    RightAscension ra;
    ra.set(25, 30, 0);
    EXPECT_EQ(1, ra.getHours());
    EXPECT_EQ(30, ra.getMinutes());
}

TEST(RightAscensionTest, NonNegative)
{
    RightAscension ra(-1, 0, 0);
    EXPECT_EQ(23, ra.getHours());
}

TEST(RightAscensionTest, GetTotalHours)
{
    RightAscension ra(6, 30, 0);
    EXPECT_FLOAT_EQ(6.5f, ra.getTotalHours());
}

TEST(RightAscensionTest, CopyConstructor)
{
    RightAscension ra1(10, 20, 30);
    RightAscension ra2(ra1);
    EXPECT_EQ(10, ra2.getHours());
    EXPECT_EQ(20, ra2.getMinutes());
    EXPECT_EQ(30, ra2.getSeconds());
}
