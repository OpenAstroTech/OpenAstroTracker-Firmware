#pragma once

#include <stdint.h>

#include "ports/PortVocabulary.hpp"
#include "types/DayTime.hpp"
#include "types/Latitude.hpp"
#include "types/Longitude.hpp"

namespace ports
{
/// The mount's site and clock: where it is and what time it thinks it is.
///
/// Separate from `IMountConfiguration` because these are not calibration values: they change at
/// every session and every host writes them (`:St#`, `:Sg#`, `:SG#`, `:SL#`, `:SC#`, `:SH#`). They
/// are also the values that make every other coordinate mean something, so the read side belongs
/// beside the write side.
///
/// Typed on `types::` values for the same reason `IMountReadModel` is: the `DayTime` / `Latitude` /
/// `Longitude` overlays in `adapters/` derive from them, and a virtual method cannot narrow a
/// parameter to a derived type — parameter types must match exactly. Callers keep using the
/// overlays; they convert implicitly on the way in and explicitly on the way out.
class IMountSiteTime
{
  public:
    virtual ~IMountSiteTime()
    {
    }

    /// The site the mount is configured for.
    virtual types::Latitude latitude() const   = 0;
    virtual types::Longitude longitude() const = 0;

    /// Sets the site and persists it. A latitude sign change also flips the axes' direction pins,
    /// because it changes the hemisphere.
    virtual void setLatitude(const types::Latitude &latitude)    = 0;
    virtual void setLongitude(const types::Longitude &longitude) = 0;

    /// The mount's clock: local wall time, UTC (derived from local and the offset), and the
    /// calendar date the local time belongs to.
    virtual types::DayTime getLocalTime() = 0;
    virtual types::DayTime getUtcTime()   = 0;
    virtual LocalDate getLocalDate()      = 0;

    /// The site's timezone offset from UTC (`local = UTC + offset`). The Meade wire carries its
    /// negative (`:GG#` is the hours added to local time to reach UTC), which the adapter flips in
    /// one place.
    virtual int getLocalUtcOffset() const      = 0;
    virtual void setLocalUtcOffset(int offset) = 0;

    /// Anchors the clock: the local time and the calendar date the mount counts forward from. The
    /// tick the anchor was set on is recorded by the implementation.
    virtual void setLocalStartTime(const types::DayTime &time)   = 0;
    virtual void setLocalStartDate(int year, int month, int day) = 0;

    /// Sets the local sidereal time (`:SHL#`) or the hour angle (`:SH#`) and re-derives the
    /// mount's internal clock from it.
    virtual void setLST(const types::DayTime &lst) = 0;
    virtual void setHA(const types::DayTime &ha)   = 0;
};

}  // namespace ports
