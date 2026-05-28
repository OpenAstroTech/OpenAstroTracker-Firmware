#include <gtest/gtest.h>
#include "core/CalendarMath.hpp"

using core::addDays;
using core::CalendarDate;

TEST(CalendarMathTest, NoOverflow)
{
    CalendarDate d {2023, 6, 15};
    CalendarDate result = addDays(d, 1);
    EXPECT_EQ(2023, result.year);
    EXPECT_EQ(6, result.month);
    EXPECT_EQ(16, result.day);
}

TEST(CalendarMathTest, MonthEndJanuary)
{
    CalendarDate d {2023, 1, 31};
    CalendarDate result = addDays(d, 1);
    EXPECT_EQ(2023, result.year);
    EXPECT_EQ(2, result.month);
    EXPECT_EQ(1, result.day);
}

TEST(CalendarMathTest, FebruaryLeapYear)
{
    CalendarDate d {2020, 2, 28};
    CalendarDate result = addDays(d, 1);
    EXPECT_EQ(2020, result.year);
    EXPECT_EQ(2, result.month);
    EXPECT_EQ(29, result.day);
    // One more day into March
    result = addDays(result, 1);
    EXPECT_EQ(2020, result.year);
    EXPECT_EQ(3, result.month);
    EXPECT_EQ(1, result.day);
}

TEST(CalendarMathTest, FebruaryNonLeapYear)
{
    CalendarDate d {2021, 2, 28};
    CalendarDate result = addDays(d, 1);
    EXPECT_EQ(2021, result.year);
    EXPECT_EQ(3, result.month);
    EXPECT_EQ(1, result.day);
}

TEST(CalendarMathTest, YearEnd)
{
    CalendarDate d {2023, 12, 31};
    CalendarDate result = addDays(d, 1);
    EXPECT_EQ(2024, result.year);
    EXPECT_EQ(1, result.month);
    EXPECT_EQ(1, result.day);
}

TEST(CalendarMathTest, CenturyLeapRule)
{
    // 1900 is not a leap year (divisible by 100 but not 400)
    CalendarDate d {1900, 2, 28};
    CalendarDate result = addDays(d, 1);
    EXPECT_EQ(1900, result.year);
    EXPECT_EQ(3, result.month);
    EXPECT_EQ(1, result.day);
}

TEST(CalendarMathTest, CenturyLeapRule400)
{
    // 2000 IS a leap year (divisible by 400)
    CalendarDate d {2000, 2, 28};
    CalendarDate result = addDays(d, 1);
    EXPECT_EQ(2000, result.year);
    EXPECT_EQ(2, result.month);
    EXPECT_EQ(29, result.day);
}

TEST(CalendarMathTest, AddMultipleDays)
{
    CalendarDate d {2023, 1, 1};
    CalendarDate result = addDays(d, 365);
    // 2023 is not a leap year, so 365 days = Jan 1 2024
    EXPECT_EQ(2024, result.year);
    EXPECT_EQ(1, result.month);
    EXPECT_EQ(1, result.day);
}

TEST(CalendarMathTest, MonthEnd30Day)
{
    CalendarDate d {2023, 4, 30};
    CalendarDate result = addDays(d, 1);
    EXPECT_EQ(2023, result.year);
    EXPECT_EQ(5, result.month);
    EXPECT_EQ(1, result.day);
}

TEST(CalendarMathTest, NegativeDays)
{
    CalendarDate d {2023, 1, 1};
    CalendarDate result = addDays(d, -1);
    EXPECT_EQ(2022, result.year);
    EXPECT_EQ(12, result.month);
    EXPECT_EQ(31, result.day);
}
