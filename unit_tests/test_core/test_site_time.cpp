#include <gtest/gtest.h>

#include "core/SiteTime.hpp"

namespace
{
constexpr unsigned long kOneSecond = 1000;
constexpr unsigned long kOneDay    = 86400UL * 1000UL;
}  // namespace

TEST(SiteTimeTest, LocalTimeAdvancesWithTheClock)
{
    core::SiteTime site;
    site.setLocalStartTime(core::DayTime(20, 30, 0), 5000);

    EXPECT_NEAR(20.5f, site.localTimeAt(5000).getTotalHours(), 0.001f);
    EXPECT_NEAR(20.5f + 90.0f / 3600.0f, site.localTimeAt(5000 + 90 * kOneSecond).getTotalHours(), 0.001f);
}

TEST(SiteTimeTest, UtcTimeSubtractsTheOffset)
{
    core::SiteTime site;
    site.setLocalStartTime(core::DayTime(20, 0, 0), 0);
    site.setLocalUtcOffset(2);

    EXPECT_NEAR(18.0f, site.utcTimeAt(0).getTotalHours(), 0.001f);
}

TEST(SiteTimeTest, UtcTimeHandlesANegativeOffset)
{
    core::SiteTime site;
    site.setLocalStartTime(core::DayTime(20, 0, 0), 0);
    site.setLocalUtcOffset(-5);

    EXPECT_NEAR(1.0f, site.utcTimeAt(0).getTotalHours(), 0.001f);
}

TEST(SiteTimeTest, DateDoesNotRollBeforeLocalMidnight)
{
    core::SiteTime site;
    site.setLocalStartDate({2024, 3, 10});
    site.setLocalStartTime(core::DayTime(22, 0, 0), 0);

    const core::CalendarDate date = site.localDateAt(30 * 60 * kOneSecond);  // 22:30 local

    EXPECT_EQ(2024, date.year);
    EXPECT_EQ(3, date.month);
    EXPECT_EQ(10, date.day);
}

TEST(SiteTimeTest, DateRollsAtLocalMidnight)
{
    core::SiteTime site;
    site.setLocalStartDate({2024, 3, 10});
    site.setLocalStartTime(core::DayTime(22, 0, 0), 0);

    const core::CalendarDate date = site.localDateAt(3 * 3600 * kOneSecond);  // 01:00 local

    EXPECT_EQ(2024, date.year);
    EXPECT_EQ(3, date.month);
    EXPECT_EQ(11, date.day);
}

TEST(SiteTimeTest, DateRollsAcrossAMonthBoundary)
{
    core::SiteTime site;
    site.setLocalStartDate({2024, 2, 28});
    site.setLocalStartTime(core::DayTime(12, 0, 0), 0);

    const core::CalendarDate date = site.localDateAt(kOneDay);  // noon the next day

    EXPECT_EQ(2024, date.year);
    EXPECT_EQ(2, date.month);
    EXPECT_EQ(29, date.day);  // 2024 is a leap year
}
