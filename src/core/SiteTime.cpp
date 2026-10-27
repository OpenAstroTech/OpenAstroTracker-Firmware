#include "SiteTime.hpp"

#include "SiderealClock.hpp"

namespace core
{

void SiteTime::setLocalStartTime(const types::DayTime &localTime, unsigned long nowMillis)
{
    _localStartTime = localTime;
    _startSetMillis = nowMillis;
}

void SiteTime::adjustRaZeroPoint(float hours)
{
    _raZeroPoint.addHours(hours);
}

types::DayTime SiteTime::localTimeAt(unsigned long nowMillis) const
{
    types::DayTime timeLocal = _localStartTime;
    timeLocal.addSeconds((nowMillis - _startSetMillis) / 1000);
    return timeLocal;
}

types::DayTime SiteTime::utcTimeAt(unsigned long nowMillis) const
{
    types::DayTime timeUTC = localTimeAt(nowMillis);
    timeUTC.addHours(-_localUtcOffset);
    return timeUTC;
}

types::DayTime SiteTime::lstAt(unsigned long nowMillis) const
{
    const types::DayTime utc          = utcTimeAt(nowMillis);
    const CalendarDate date    = localDateAt(nowMillis);
    const CalendarDate utcDate = localToUtcDate(date, localTimeAt(nowMillis).getTotalSeconds(), _localUtcOffset);

    return SiderealClock::localSiderealTime(_longitude.getTotalHours(), utcDate.year, utcDate.month, utcDate.day, utc.getTotalHours());
}

CalendarDate SiteTime::localDateAt(unsigned long nowMillis) const
{
    const long secondsSinceSetDayEnd = ((nowMillis - _startSetMillis) / 1000) - (86400 - _localStartTime.getTotalSeconds());

    // Number of day ends passed since the date was set; a partial day still counts as one.
    const long daysElapsed = secondsSinceSetDayEnd >= 0 ? (secondsSinceSetDayEnd / 86400) + 1 : 0;

    return addDays(_localStartDate, daysElapsed);
}

}  // namespace core
