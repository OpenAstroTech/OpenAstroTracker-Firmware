#pragma once

#include "core/SiteTime.hpp"
#include "ports/IClock.hpp"
#include "ports/ILogger.hpp"
#include "ports/IMountSettings.hpp"
#include "ports/IMountSiteTime.hpp"

namespace adapters
{

/// `ports::IMountSiteTime` over the session's site and time state.
///
/// The values live in `core::SiteTime`; what is left here is the three things a write has to do
/// besides storing a number: persist it, re-derive the sidereal clock from the new values, and —
/// for latitude only — tell the mount to reconfigure its hemisphere, which stops the axes and
/// inverts direction pins.
///
/// That last one is a motion consequence of a site change, so it is injected as a plain
/// callback rather than being implemented here: a function pointer and a context keep this
/// adapter off the mount's type (and cost no vtable, which on AVR is SRAM).
class SiteTimePort final : public ports::IMountSiteTime
{
  public:
    /// What to call when the latitude changes sign, i.e. when the axes have to be turned around.
    struct LatitudeObserver {
        void *context;
        void (*onChanged)(void *context, bool northernHemisphere);

        bool valid() const
        {
            return onChanged != nullptr;
        }
    };

    /// A callback that does nothing, for a board that never changes hemisphere.
    static LatitudeObserver noObserver();

    SiteTimePort(
        core::SiteTime &siteTime, ports::IClock &clock, ports::IMountSettings &store, ports::ILogger &logger, LatitudeObserver observer);

    types::Latitude latitude() const override;

    types::Longitude longitude() const override;

    void setLatitude(const types::Latitude &latitude) override;

    void setLongitude(const types::Longitude &longitude) override;

    types::DayTime getLocalTime() override;

    types::DayTime getUtcTime() override;

    LocalDate getLocalDate() override;

    int getLocalUtcOffset() const override;

    void setLocalUtcOffset(int offset) override;

    void setLocalStartTime(const types::DayTime &time) override;

    void setLocalStartDate(int year, int month, int day) override;

    void setLST(const types::DayTime &lst) override;

    void setHA(const types::DayTime &ha) override;

  private:
    /// Re-derives the sidereal time from the clock and the current site, which resets the RA
    /// zero point — the mount has always re-anchored its coordinates when the site or the time
    /// changes.
    void reanchorSiderealTime();

    core::SiteTime &_siteTime;
    ports::IClock &_clock;
    ports::IMountSettings &_store;
    ports::ILogger &_logger;
    LatitudeObserver _observer;
};

}  // namespace adapters
