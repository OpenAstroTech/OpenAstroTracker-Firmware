#pragma once

#include "CalendarMath.hpp"
#include "types/DayTime.hpp"
#include "types/Latitude.hpp"
#include "types/Longitude.hpp"

namespace core
{

/// Where the mount is and what time it thinks it is.
///
/// These are the values every other coordinate is expressed relative to, and the host sets all
/// of them at the start of a session (`:St#`, `:Sg#`, `:SG#`, `:SL#`, `:SC#`, `:Sd#`). They are
/// kept together because they travel together and because the mount's derived clock — local
/// time, UTC, the calendar date — is computed from them.
///
/// The current time is not stored: it is derived from the start time plus the elapsed
/// milliseconds, so the caller passes "now" in and this stays free of any clock.
class SiteTime
{
  public:
    Latitude latitude() const
    {
        return _latitude;
    }

    Longitude longitude() const
    {
        return _longitude;
    }

    void setLatitude(const Latitude &latitude)
    {
        _latitude = latitude;
    }

    void setLongitude(const Longitude &longitude)
    {
        _longitude = longitude;
    }

    /// Local sidereal time.
    DayTime lst() const
    {
        return _lst;
    }

    /// Sets the sidereal time **and** the right-ascension zero point, which is what the mount has
    /// always done: home RA is "wherever the sky was when the session was set up". A sync moves
    /// the zero point on its own, which is why the two are separate fields.
    void setLst(const DayTime &lst)
    {
        _lst         = lst;
        _raZeroPoint = lst;
    }

    /// Right ascension of the home position, in hours. Offset from `lst()` by whatever syncs have
    /// applied.
    DayTime raZeroPoint() const
    {
        return _raZeroPoint;
    }

    void setRaZeroPoint(const DayTime &zeroPoint)
    {
        _raZeroPoint = zeroPoint;
    }

    /// Shifts the reference point (rather than the clock) — what a sync does.
    void adjustRaZeroPoint(float hours);

    int localUtcOffset() const
    {
        return _localUtcOffset;
    }

    void setLocalUtcOffset(int offset)
    {
        _localUtcOffset = offset;
    }

    /// The local time the session started at, and the millisecond tick it was set on.
    DayTime localStartTime() const
    {
        return _localStartTime;
    }

    unsigned long startSetMillis() const
    {
        return _startSetMillis;
    }

    void setLocalStartTime(const DayTime &localTime, unsigned long nowMillis);

    CalendarDate localStartDate() const
    {
        return _localStartDate;
    }

    void setLocalStartDate(CalendarDate date)
    {
        _localStartDate = date;
    }

    /// Local time `nowMillis` milliseconds after the start time.
    DayTime localTimeAt(unsigned long nowMillis) const;

    /// Local time `nowMillis` milliseconds after the start time, less the UTC offset.
    DayTime utcTimeAt(unsigned long nowMillis) const;

    /// The local calendar date `nowMillis` milliseconds after the start time. A day rolls over at
    /// local midnight, so a partial day still counts as one.
    CalendarDate localDateAt(unsigned long nowMillis) const;

    /// Local sidereal time at `nowMillis`.
    ///
    /// The sidereal formula wants the *UTC* calendar date, which differs from the local date for
    /// |UTC offset| hours each day, so the local date is converted before it is used.
    DayTime lstAt(unsigned long nowMillis) const;

  private:
    Latitude _latitude;
    Longitude _longitude;
    DayTime _lst;
    DayTime _raZeroPoint;
    DayTime _localStartTime;
    unsigned long _startSetMillis = 0;
    CalendarDate _localStartDate  = {0, 0, 0};
    int _localUtcOffset           = 0;
};

}  // namespace core
