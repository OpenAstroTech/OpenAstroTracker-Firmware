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
/// every session and every host writes them (`:St#`, `:Sg#`, `:SG#`, `:SL#`, `:SC#`, `:Sd#`). They
/// are also the values that make every other coordinate mean something, so the read side belongs
/// beside the write side.
///
/// Typed on `core::` values for the same reason `IMountReadModel` is: the `src/` overlays derive
/// from them and a port cannot be overridden by a method whose *parameter* is the derived type —
/// parameter types must match exactly. Callers keep using the overlays; they convert implicitly
/// on the way in and explicitly on the way out.
class IMountSiteTime
{
  public:
    virtual ~IMountSiteTime()
    {
    }

    virtual types::Latitude latitude() const   = 0;
    virtual types::Longitude longitude() const = 0;

    virtual void setLatitude(const types::Latitude &latitude)    = 0;
    virtual void setLongitude(const types::Longitude &longitude) = 0;

    virtual types::DayTime getLocalTime() = 0;
    virtual types::DayTime getUtcTime()   = 0;
    virtual LocalDate getLocalDate()     = 0;

    /// The offset the mount adds to local time to reach UTC. The Meade wire carries the inverse
    /// (`:GG#` is UTC-offset, not UTC-to-local), which the adapter flips in one place.
    virtual int getLocalUtcOffset() const      = 0;
    virtual void setLocalUtcOffset(int offset) = 0;

    virtual void setLocalStartTime(const types::DayTime &time)    = 0;
    virtual void setLocalStartDate(int year, int month, int day) = 0;

    /// Sets the local sidereal time (`:St#`) / the hour angle (`:SH#`) and re-derives the
    /// mount's internal clock from it.
    virtual void setLST(const types::DayTime &lst) = 0;
    virtual void setHA(const types::DayTime &ha)   = 0;
};

}  // namespace ports
