#include <gtest/gtest.h>

#include "core/SiteTime.hpp"
#include "core/SiderealClock.hpp"

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

TEST(SiteTimeTest, LstMatchesTheSiderealFormulaForTheUtcInstant)
{
    // J2000.0: 2000-01-01 12:00 UTC at Greenwich gives a sidereal time of about 18h 41m 50s.
    core::SiteTime site;
    site.setLongitude(core::Longitude(0.0f));
    site.setLocalUtcOffset(0);
    site.setLocalStartDate({2000, 1, 1});
    site.setLocalStartTime(core::DayTime(12, 0, 0), 0);

    const float lst = site.lstAt(0).getTotalHours();

    EXPECT_NEAR(18.6974f, lst, 0.01f) << "LST was " << lst;
}

TEST(SiteTimeTest, LstUsesTheUtcDateNotTheLocalOne)
{
    // 01:00 local at UTC+2 is 23:00 UTC on the *previous* day; using the local date here would
    // shift the answer by nearly 24 sidereal hours.
    core::SiteTime site;
    site.setLongitude(core::Longitude(13.0f));
    site.setLocalUtcOffset(2);
    site.setLocalStartDate({2024, 6, 15});
    site.setLocalStartTime(core::DayTime(1, 0, 0), 0);

    const float lst = site.lstAt(0).getTotalHours();

    const float expected = core::SiderealClock::localSiderealTime(13.0, 2024, 6, 14, 23.0f).getTotalHours();
    EXPECT_NEAR(expected, lst, 0.001f);
}

TEST(SiteTimeTest, LstAdvancesAtTheSiderealRate)
{
    // One solar hour advances sidereal time by 1.0027 hours, because the sidereal day is shorter.
    core::SiteTime site;
    site.setLongitude(core::Longitude(-70.0f));
    site.setLocalUtcOffset(-5);
    site.setLocalStartDate({2024, 2, 1});
    site.setLocalStartTime(core::DayTime(20, 0, 0), 0);

    const float start  = site.lstAt(0).getTotalHours();
    const float anHour = site.lstAt(3600UL * 1000UL).getTotalHours();

    float delta = anHour - start;
    if (delta < -12.0f)
    {
        delta += 24.0f;
    }

    EXPECT_NEAR(1.0027f, delta, 0.01f);
}
