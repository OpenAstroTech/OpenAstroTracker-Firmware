#include "SiteTimeService.hpp"

#include "SiderealClock.hpp"

namespace core
{
SiteTimeService::LatitudeObserver SiteTimeService::noObserver()
{
    LatitudeObserver observer = {nullptr, nullptr};
    return observer;
}

SiteTimeService::SiteTimeService(
    core::SiteTime &siteTime, ports::IClock &clock, ports::IMountSettings &store, LatitudeObserver observer, Constants constants)
    : _siteTime(siteTime), _clock(clock), _store(store), _observer(observer), _constants(constants)
{
}

types::Latitude SiteTimeService::latitude() const
{
    return _siteTime.latitude();
}

types::Longitude SiteTimeService::longitude() const
{
    return _siteTime.longitude();
}

void SiteTimeService::setLatitude(const types::Latitude &latitude)
{
    _siteTime.setLatitude(latitude);
    _store.storeLatitude(latitude);

    // Reconfiguring the hemisphere stops the axes and inverts their direction pins, so the mount
    // does it; all this adapter knows is that the sign matters.
    if (_observer.valid())
    {
        _observer.onChanged(_observer.context, latitude.getTotalHours() > 0);
    }
}

void SiteTimeService::setLongitude(const types::Longitude &longitude)
{
    _siteTime.setLongitude(longitude);
    _store.storeLongitude(longitude);

    reanchorSiderealTime();
}

types::DayTime SiteTimeService::getLocalTime()
{
    return _siteTime.localTimeAt(_clock.now());
}

types::DayTime SiteTimeService::getUtcTime()
{
    return _siteTime.utcTimeAt(_clock.now());
}

LocalDate SiteTimeService::getLocalDate()
{
    const core::CalendarDate date = _siteTime.localDateAt(_clock.now());
    return {date.year, date.month, date.day};
}

int SiteTimeService::getLocalUtcOffset() const
{
    return _siteTime.localUtcOffset();
}

void SiteTimeService::setLocalUtcOffset(int offset)
{
    _siteTime.setLocalUtcOffset(offset);
    _store.storeUtcOffset(_siteTime.localUtcOffset());

    reanchorSiderealTime();
}

void SiteTimeService::setLocalStartTime(const types::DayTime &time)
{
    _siteTime.setLocalStartTime(time, _clock.now());

    reanchorSiderealTime();
}

void SiteTimeService::setLocalStartDate(int year, int month, int day)
{
    _siteTime.setLocalStartDate({year, month, day});

    reanchorSiderealTime();
}

void SiteTimeService::setLST(const types::DayTime &lst)
{
    _siteTime.setLst(lst);
    _siteTime.adjustRaZeroPoint(_constants.raZeroPointShiftHours);

}

void SiteTimeService::setHA(const types::DayTime &ha)
{
    setLST(core::SiderealClock::lstFromHourAngle(ha, _constants.polarisRaHour, _constants.polarisRaMinute, _constants.polarisRaSecond));
}

void SiteTimeService::reanchorSiderealTime()
{
    // Recompute from the clock rather than from the old value: the site or the time just changed,
    // so the previous sidereal time is stale.
    setLST(_siteTime.lstAt(_clock.now()));
}

}  // namespace core
