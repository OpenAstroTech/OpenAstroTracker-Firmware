#include <gtest/gtest.h>
#include "core/types/DayTime.hpp"

using core::DayTime;

TEST(DayTimeTest, DefaultConstructor)
{
    DayTime dt;
    EXPECT_EQ(0, dt.getTotalSeconds());
    EXPECT_EQ(0, dt.getHours());
    EXPECT_EQ(0, dt.getMinutes());
    EXPECT_EQ(0, dt.getSeconds());
}

TEST(DayTimeTest, HMSConstructor)
{
    DayTime dt(1, 30, 45);
    EXPECT_EQ(1, dt.getHours());
    EXPECT_EQ(30, dt.getMinutes());
    EXPECT_EQ(45, dt.getSeconds());
}

TEST(DayTimeTest, FloatConstructor)
{
    DayTime dt(1.5f);  // 1.5 hours = 1h 30m
    EXPECT_EQ(1, dt.getHours());
    EXPECT_EQ(30, dt.getMinutes());
    EXPECT_EQ(0, dt.getSeconds());
}

TEST(DayTimeTest, CopyConstructor)
{
    DayTime dt1(2, 15, 30);
    DayTime dt2(dt1);
    EXPECT_EQ(dt1.getTotalSeconds(), dt2.getTotalSeconds());
}

TEST(DayTimeTest, GetTotalHours)
{
    DayTime dt(2, 30, 0);
    EXPECT_FLOAT_EQ(2.5f, dt.getTotalHours());
}

TEST(DayTimeTest, GetTotalMinutes)
{
    DayTime dt(1, 30, 0);
    EXPECT_FLOAT_EQ(90.0f, dt.getTotalMinutes());
}

TEST(DayTimeTest, SetHMS)
{
    DayTime dt;
    dt.set(12, 34, 56);
    EXPECT_EQ(12, dt.getHours());
    EXPECT_EQ(34, dt.getMinutes());
    EXPECT_EQ(56, dt.getSeconds());
}

TEST(DayTimeTest, AddHours)
{
    DayTime dt(1, 0, 0);
    dt.addHours(2.5f);
    EXPECT_EQ(3, dt.getHours());
    EXPECT_EQ(30, dt.getMinutes());
}

TEST(DayTimeTest, AddMinutes)
{
    DayTime dt(0, 30, 0);
    dt.addMinutes(45);
    EXPECT_EQ(1, dt.getHours());
    EXPECT_EQ(15, dt.getMinutes());
}

TEST(DayTimeTest, AddSeconds)
{
    DayTime dt(0, 0, 30);
    dt.addSeconds(45);
    EXPECT_EQ(1, dt.getMinutes());
    EXPECT_EQ(15, dt.getSeconds());
}

TEST(DayTimeTest, AddTimeComponents)
{
    DayTime dt(1, 30, 0);
    dt.addTime(1, 45, 30);
    EXPECT_EQ(3, dt.getHours());
    EXPECT_EQ(15, dt.getMinutes());
    EXPECT_EQ(30, dt.getSeconds());
}

TEST(DayTimeTest, AddTimeObject)
{
    DayTime dt1(1, 0, 0);
    DayTime dt2(2, 30, 0);
    dt1.addTime(dt2);
    EXPECT_EQ(3, dt1.getHours());
    EXPECT_EQ(30, dt1.getMinutes());
}

TEST(DayTimeTest, SubtractTime)
{
    DayTime dt1(3, 0, 0);
    DayTime dt2(1, 30, 0);
    dt1.subtractTime(dt2);
    EXPECT_EQ(1, dt1.getHours());
    EXPECT_EQ(30, dt1.getMinutes());
}

TEST(DayTimeTest, WrapAt24Hours)
{
    DayTime dt(22, 0, 0);
    dt.addHours(4.0f);
    EXPECT_EQ(2, dt.getHours());
}

TEST(DayTimeTest, WrapNegative)
{
    DayTime dt(1, 0, 0);
    dt.subtractTime(DayTime(3, 0, 0));
    EXPECT_EQ(22, dt.getHours());
}

TEST(DayTimeTest, NegativeConstructor)
{
    // Constructor only uses sign of hours; abs() on hours, minutes as-is.
    // (-1, -30, 0) = -1 * ((60*1 + (-30))*60 + 0) = -1800 sec
    // getTime: labs(1800) => h=0, m=30, s=0, h*=sign(-1800)=0
    DayTime dt(-1, -30, -0);
    EXPECT_EQ(0, dt.getHours());
    EXPECT_EQ(30, dt.getMinutes());
    EXPECT_EQ(0, dt.getSeconds());
}

TEST(DayTimeTest, NegativeFloatConstructor)
{
    // -1.5h = -5400 sec => h=-1, m=30
    DayTime dt(-1.5f);
    EXPECT_EQ(-1, dt.getHours());
    EXPECT_EQ(30, dt.getMinutes());
}

TEST(DayTimeTest, SetWraps)
{
    // Constructor does NOT call checkHours(). set() does.
    DayTime dt(25, 0, 0);
    EXPECT_EQ(25, dt.getHours());
    // set() wraps
    dt.set(25, 0, 0);
    EXPECT_EQ(1, dt.getHours());
}

TEST(DayTimeTest, AddHoursNegative)
{
    DayTime dt(2, 0, 0);
    dt.addHours(-3.0f);
    EXPECT_EQ(23, dt.getHours());
}

TEST(DayTimeTest, GetTimeRef)
{
    DayTime dt(5, 15, 30);
    int h, m, s;
    dt.getTime(h, m, s);
    EXPECT_EQ(5, h);
    EXPECT_EQ(15, m);
    EXPECT_EQ(30, s);
}

TEST(DayTimeTest, GetTimeRefNegative)
{
    // (-2, -30, -15) = -1 * ((60*2 + (-30))*60 + (-15)) = -5385 sec
    // labs(5385): h=1, rem=1785, m=29, s=45, h*=-1 => (-1, 29, 45)
    DayTime dt(-2, -30, -15);
    int h, m, s;
    dt.getTime(h, m, s);
    EXPECT_EQ(-1, h);
    EXPECT_EQ(29, m);
    EXPECT_EQ(45, s);
}
