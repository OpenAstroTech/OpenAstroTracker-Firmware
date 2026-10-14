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

TEST(CalendarMathTest, LocalToUtcDateEastAfterMidnight)
{
    // UTC+2, local 2026-10-14 01:30 is UTC 2026-10-13 23:30
    CalendarDate result = core::localToUtcDate({2026, 10, 14}, 1 * 3600L + 30 * 60L, 2);
    EXPECT_EQ(2026, result.year);
    EXPECT_EQ(10, result.month);
    EXPECT_EQ(13, result.day);
}

TEST(CalendarMathTest, LocalToUtcDateEastSameDay)
{
    // UTC+2, local 2026-10-14 03:30 is UTC 2026-10-14 01:30
    CalendarDate result = core::localToUtcDate({2026, 10, 14}, 3 * 3600L + 30 * 60L, 2);
    EXPECT_EQ(2026, result.year);
    EXPECT_EQ(10, result.month);
    EXPECT_EQ(14, result.day);
}

TEST(CalendarMathTest, LocalToUtcDateWestEvening)
{
    // UTC-5, local 2026-01-15 20:00 is UTC 2026-01-16 01:00
    CalendarDate result = core::localToUtcDate({2026, 1, 15}, 20 * 3600L, -5);
    EXPECT_EQ(2026, result.year);
    EXPECT_EQ(1, result.month);
    EXPECT_EQ(16, result.day);
}

TEST(CalendarMathTest, LocalToUtcDateWestSameDay)
{
    // UTC-5, local 2026-01-15 18:59:59 is UTC 2026-01-15 23:59:59
    CalendarDate result = core::localToUtcDate({2026, 1, 15}, 18 * 3600L + 59 * 60L + 59, -5);
    EXPECT_EQ(2026, result.year);
    EXPECT_EQ(1, result.month);
    EXPECT_EQ(15, result.day);
}

TEST(CalendarMathTest, LocalToUtcDateUtcZero)
{
    CalendarDate result = core::localToUtcDate({2026, 10, 14}, 0, 0);
    EXPECT_EQ(14, result.day);
    result = core::localToUtcDate({2026, 10, 14}, 86399L, 0);
    EXPECT_EQ(14, result.day);
}

TEST(CalendarMathTest, LocalToUtcDateEastYearBoundary)
{
    // UTC+10, local 2027-01-01 05:00 is UTC 2026-12-31 19:00
    CalendarDate result = core::localToUtcDate({2027, 1, 1}, 5 * 3600L, 10);
    EXPECT_EQ(2026, result.year);
    EXPECT_EQ(12, result.month);
    EXPECT_EQ(31, result.day);
}

TEST(CalendarMathTest, LocalToUtcDateWestLeapDay)
{
    // UTC-8, local 2028-02-28 17:00 is UTC 2028-02-29 01:00
    CalendarDate result = core::localToUtcDate({2028, 2, 28}, 17 * 3600L, -8);
    EXPECT_EQ(2028, result.year);
    EXPECT_EQ(2, result.month);
    EXPECT_EQ(29, result.day);
}

TEST(CalendarMathTest, LocalToUtcDateWestYearBoundary)
{
    // UTC-12, local 2026-12-31 12:00 is UTC 2027-01-01 00:00
    CalendarDate result = core::localToUtcDate({2026, 12, 31}, 12 * 3600L, -12);
    EXPECT_EQ(2027, result.year);
    EXPECT_EQ(1, result.month);
    EXPECT_EQ(1, result.day);
}

TEST(CalendarMathTest, LocalToUtcDateExactUtcMidnight)
{
    // UTC+2, local 02:00 is exactly UTC 00:00 on the same date; 01:59:59 is still the day before
    CalendarDate result = core::localToUtcDate({2026, 10, 14}, 2 * 3600L, 2);
    EXPECT_EQ(14, result.day);
    result = core::localToUtcDate({2026, 10, 14}, 2 * 3600L - 1, 2);
    EXPECT_EQ(13, result.day);
}
