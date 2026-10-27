// Board configuration first: `ports/ILogger.hpp` decides at include time whether `OAT_LOG` exists
// (it reads `DEBUG_LEVEL`), and `SiteTimePort.hpp` pulls the logger in before anything else would
// have defined it — which silently compiled every log line in this file out on a logging build.
#include "../../Configuration.hpp"

#include "SiteTimePort.hpp"

#include "core/SiderealClock.hpp"
#include "ports/ILogger.hpp"

namespace adapters
{
SiteTimePort::LatitudeObserver SiteTimePort::noObserver()
{
    LatitudeObserver observer = {nullptr, nullptr};
    return observer;
}

SiteTimePort::SiteTimePort(
    core::SiteTime &siteTime, ports::IClock &clock, ports::IMountSettings &store, ports::ILogger &logger, LatitudeObserver observer)
    : _siteTime(siteTime), _clock(clock), _store(store), _logger(logger), _observer(observer)
{
}

types::Latitude SiteTimePort::latitude() const
{
    return _siteTime.latitude();
}

types::Longitude SiteTimePort::longitude() const
{
    return _siteTime.longitude();
}

void SiteTimePort::setLatitude(const types::Latitude &latitude)
{
    OAT_LOG(&_logger, DEBUG_GENERAL, "[MOUNT]: Setting latitude to %fs", latitude.getTotalHours());
    _siteTime.setLatitude(latitude);
    _store.storeLatitude(latitude);

    // Reconfiguring the hemisphere stops the axes and inverts their direction pins, so the mount
    // does it; all this adapter knows is that the sign matters.
    if (_observer.valid())
    {
        _observer.onChanged(_observer.context, latitude.getTotalHours() > 0);
    }
}

void SiteTimePort::setLongitude(const types::Longitude &longitude)
{
    OAT_LOG(&_logger, DEBUG_GENERAL, "[MOUNT]: Setting longitude to %fs", longitude.getTotalHours());
    _siteTime.setLongitude(longitude);
    _store.storeLongitude(longitude);

    reanchorSiderealTime();
}

types::DayTime SiteTimePort::getLocalTime()
{
    return _siteTime.localTimeAt(_clock.now());
}

types::DayTime SiteTimePort::getUtcTime()
{
    return _siteTime.utcTimeAt(_clock.now());
}

LocalDate SiteTimePort::getLocalDate()
{
    const core::CalendarDate date = _siteTime.localDateAt(_clock.now());
    return {date.year, date.month, date.day};
}

int SiteTimePort::getLocalUtcOffset() const
{
    return _siteTime.localUtcOffset();
}

void SiteTimePort::setLocalUtcOffset(int offset)
{
    _siteTime.setLocalUtcOffset(offset);
    _store.storeUtcOffset(_siteTime.localUtcOffset());

    reanchorSiderealTime();
}

void SiteTimePort::setLocalStartTime(const types::DayTime &time)
{
    _siteTime.setLocalStartTime(time, _clock.now());

    reanchorSiderealTime();
}

void SiteTimePort::setLocalStartDate(int year, int month, int day)
{
    _siteTime.setLocalStartDate({year, month, day});

    reanchorSiderealTime();
}

void SiteTimePort::setLST(const types::DayTime &lst)
{
    _siteTime.setLst(lst);
    _siteTime.adjustRaZeroPoint(RA_ZERO_POINT_SHIFT_HOURS);

#if DEBUG_LEVEL != 0
    const types::DayTime reanchored = _siteTime.lst();
    OAT_LOG(&_logger,
            DEBUG_MOUNT,
            "[MOUNT]: Set LST and ZeroPosRA to: %02d:%02d:%02d",
            reanchored.getHours(),
            reanchored.getMinutes(),
            reanchored.getSeconds());
#endif
}

void SiteTimePort::setHA(const types::DayTime &ha)
{
    OAT_LOG(&_logger, DEBUG_MOUNT, "[MOUNT]: setHA:  HA is %02d:%02d:%02d", ha.getHours(), ha.getMinutes(), ha.getSeconds());
    setLST(core::SiderealClock::lstFromHourAngle(ha, POLARIS_RA_HOUR, POLARIS_RA_MINUTE, POLARIS_RA_SECOND));
}

void SiteTimePort::reanchorSiderealTime()
{
    // Recompute from the clock rather than from the old value: the site or the time just changed,
    // so the previous sidereal time is stale.
    setLST(_siteTime.lstAt(_clock.now()));
}

}  // namespace adapters
