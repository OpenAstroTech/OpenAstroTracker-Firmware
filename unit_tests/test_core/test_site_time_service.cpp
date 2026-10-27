#include <gtest/gtest.h>

#include "core/SiderealClock.hpp"
#include "core/SiteTimeService.hpp"

#include "../test_common/hal_fakes/FakeSystemClock.hpp"
#include "../test_common/ports_fakes/FakeMountSettings.hpp"

namespace
{
/// Records the hemisphere callback the service fires on a latitude write.
struct ObserverCall {
    bool called   = false;
    bool northern = false;
};

void recordObserver(void *context, bool northern)
{
    auto *call     = static_cast<ObserverCall *>(context);
    call->called   = true;
    call->northern = northern;
}
}  // namespace

TEST(SiteTimeServiceTest, SetLatitudePersistsAndNotifiesObserverOfHemisphere)
{
    core::SiteTime siteTime;
    fakes::FakeClock clock;
    fakes::FakeMountSettings store;
    ObserverCall observer;
    core::SiteTimeService service(siteTime, clock, store, {&observer, recordObserver});

    service.setLatitude(types::Latitude(52.0f));

    EXPECT_NEAR(52.0f, siteTime.latitude().getTotalHours(), 0.001f);
    EXPECT_NEAR(52.0f, store.latitude.getTotalHours(), 0.001f);
    EXPECT_TRUE(observer.called);
    EXPECT_TRUE(observer.northern);

    observer = ObserverCall {};
    service.setLatitude(types::Latitude(-30.0f));

    EXPECT_NEAR(-30.0f, store.latitude.getTotalHours(), 0.001f);
    EXPECT_TRUE(observer.called);
    EXPECT_FALSE(observer.northern);
}

TEST(SiteTimeServiceTest, SetLongitudePersistsAndReanchorsSiderealTime)
{
    core::SiteTime siteTime;
    fakes::FakeClock clock;
    fakes::FakeMountSettings store;
    core::SiteTimeService service(siteTime, clock, store, core::SiteTimeService::noObserver());

    siteTime.setLocalStartTime(types::DayTime(20, 0, 0), 0);
    siteTime.setLocalStartDate({2022, 2, 7});

    service.setLongitude(types::Longitude(-100.0f));

    EXPECT_NEAR(-100.0f, store.longitude.getTotalHours(), 0.001f);

    const types::DayTime expected = siteTime.lstAt(clock.now());
    EXPECT_GT(expected.getTotalHours(), 0.0f);
    EXPECT_NEAR(expected.getTotalHours(), siteTime.lst().getTotalHours(), 0.001f);
}

TEST(SiteTimeServiceTest, SetHaProducesTheSiderealTimeForThatHourAngle)
{
    core::SiteTime siteTime;
    fakes::FakeClock clock;
    fakes::FakeMountSettings store;
    core::SiteTimeService service(siteTime, clock, store, core::SiteTimeService::noObserver());

    const types::DayTime hourAngle(1, 30, 0);
    service.setHA(hourAngle);

    const types::DayTime expected = core::SiderealClock::lstFromHourAngle(hourAngle, 3, 0, 8);
    EXPECT_NEAR(expected.getTotalHours(), siteTime.lst().getTotalHours(), 0.001f);
}

TEST(SiteTimeServiceTest, SetHaUsesTheInjectedPolarisCoordinates)
{
    core::SiteTime siteTime;
    fakes::FakeClock clock;
    fakes::FakeMountSettings store;
    core::SiteTimeService service(siteTime, clock, store, core::SiteTimeService::noObserver(), {0.0f, 10, 20, 30});

    const types::DayTime hourAngle(2, 0, 0);
    service.setHA(hourAngle);

    const types::DayTime expected = core::SiderealClock::lstFromHourAngle(hourAngle, 10, 20, 30);
    EXPECT_NEAR(expected.getTotalHours(), siteTime.lst().getTotalHours(), 0.001f);
}

TEST(SiteTimeServiceTest, SetLstAppliesTheInjectedZeroPointShift)
{
    core::SiteTime siteTime;
    fakes::FakeClock clock;
    fakes::FakeMountSettings store;
    core::SiteTimeService service(siteTime, clock, store, core::SiteTimeService::noObserver(), {6.0f, 3, 0, 8});

    service.setLST(types::DayTime(12, 0, 0));

    EXPECT_NEAR(12.0f, siteTime.lst().getTotalHours(), 0.001f);
    // The zero-point shift is a property of the mount (six hours on the EQ mounts), so it must
    // reach `raZeroPoint` even though `SiteTimeService` no longer reads `Configuration.hpp`.
    EXPECT_NEAR(18.0f, siteTime.raZeroPoint().getTotalHours(), 0.001f);
}
