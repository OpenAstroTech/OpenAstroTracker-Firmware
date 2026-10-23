#include "SiteTime.hpp"

namespace core
{

void SiteTime::setLocalStartTime(const DayTime &localTime, unsigned long nowMillis)
{
    _localStartTime = localTime;
    _startSetMillis = nowMillis;
}

DayTime SiteTime::localTimeAt(unsigned long nowMillis) const
{
    DayTime timeLocal = _localStartTime;
    timeLocal.addSeconds((nowMillis - _startSetMillis) / 1000);
    return timeLocal;
}

DayTime SiteTime::utcTimeAt(unsigned long nowMillis) const
{
    DayTime timeUTC = localTimeAt(nowMillis);
    timeUTC.addHours(-_localUtcOffset);
    return timeUTC;
}

CalendarDate SiteTime::localDateAt(unsigned long nowMillis) const
{
    const long secondsSinceSetDayEnd = ((nowMillis - _startSetMillis) / 1000) - (86400 - _localStartTime.getTotalSeconds());

    // Number of day ends passed since the date was set; a partial day still counts as one.
    const long daysElapsed = secondsSinceSetDayEnd >= 0 ? (secondsSinceSetDayEnd / 86400) + 1 : 0;

    return addDays(_localStartDate, daysElapsed);
}

}  // namespace core
