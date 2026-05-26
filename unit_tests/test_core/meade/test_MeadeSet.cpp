// Wire-byte tests for the Meade Set-family dispatcher (`handleMeadeSet`).
//
// Each test exercises a single Meade `:S...` (or sync `:SY...`) sub-command
// suffix end-to-end: it calls the dispatcher with a stub handler and asserts
// the exact bytes emitted on the wire, plus the typed values the handler
// observed. The stub records which callback fired so we also catch silent
// regressions where the wrong handler is invoked.

#include <gtest/gtest.h>

#include <string.h>

#include "core/meade/MeadeParser.hpp"

namespace meade = oat::core::meade;

namespace
{

class FakeHandlers : public meade::IMeadeSetHandlers
{
  public:
    const char *lastCall = nullptr;

    // ---- Configurable return values --------------------------------------
    bool nextResult = true;  // What every onSet* returns by default.

    // ---- Captured arguments ---------------------------------------------
    meade::DecCoordinate dec {};
    meade::RaCoordinate ra {};
    meade::MeadeLocalTime lst {};
    uint8_t haHours   = 0;
    uint8_t haMinutes = 0;
    meade::DecCoordinate syncDec {};
    meade::RaCoordinate syncRa {};
    meade::MeadeLatitude lat {};
    meade::MeadeLongitude lon {};
    int utc = 0;
    meade::MeadeLocalTime time {};
    meade::MeadeLocalDate date {};

    bool onSetTargetDec(meade::DecCoordinate v) override
    {
        lastCall = "targetDec";
        dec      = v;
        return nextResult;
    }
    bool onSetTargetRa(meade::RaCoordinate v) override
    {
        lastCall = "targetRa";
        ra       = v;
        return nextResult;
    }
    bool onSetLocalSiderealTime(meade::MeadeLocalTime v) override
    {
        lastCall = "lst";
        lst      = v;
        return nextResult;
    }
    bool onSetHomePoint() override
    {
        lastCall = "home";
        return nextResult;
    }
    bool onSetHourAngle(uint8_t hh, uint8_t mm) override
    {
        lastCall  = "ha";
        haHours   = hh;
        haMinutes = mm;
        return nextResult;
    }
    bool onSyncCoordinates(meade::DecCoordinate d, meade::RaCoordinate r) override
    {
        lastCall = "sync";
        syncDec  = d;
        syncRa   = r;
        return nextResult;
    }
    bool onSetSiteLatitude(meade::MeadeLatitude v) override
    {
        lastCall = "lat";
        lat      = v;
        return nextResult;
    }
    bool onSetSiteLongitude(meade::MeadeLongitude v) override
    {
        lastCall = "lon";
        lon      = v;
        return nextResult;
    }
    bool onSetUtcOffset(int v) override
    {
        lastCall = "utc";
        utc      = v;
        return nextResult;
    }
    bool onSetLocalTime(meade::MeadeLocalTime v) override
    {
        lastCall = "time";
        time     = v;
        return nextResult;
    }
    bool onSetLocalDate(meade::MeadeLocalDate v) override
    {
        lastCall = "date";
        date     = v;
        return nextResult;
    }
};

const char *dispatch(const char *suffix, FakeHandlers &h)
{
    static meade::MeadeResponse last;
    last.clear();
    meade::handleMeadeSet(last, suffix, h);
    return last.c_str();
}

}  // namespace

// ---- Target DEC (d) ----------------------------------------------------

TEST(MeadeSet, target_dec_happy_path)
{
    FakeHandlers h;
    EXPECT_STREQ("1", dispatch("d+84*03:02", h));
    EXPECT_STREQ("targetDec", h.lastCall);
    EXPECT_EQ(84, h.dec.degrees);
    EXPECT_EQ(static_cast<uint8_t>(3), h.dec.minutes);
    EXPECT_EQ(static_cast<uint8_t>(2), h.dec.seconds);
}

TEST(MeadeSet, target_dec_negative_with_colon_separator)
{
    FakeHandlers h;
    EXPECT_STREQ("1", dispatch("d-12:45:30", h));
    EXPECT_EQ(-12, h.dec.degrees);
    EXPECT_EQ(static_cast<uint8_t>(45), h.dec.minutes);
    EXPECT_EQ(static_cast<uint8_t>(30), h.dec.seconds);
}

TEST(MeadeSet, target_dec_handler_failure_returns_zero)
{
    FakeHandlers h;
    h.nextResult = false;
    EXPECT_STREQ("0", dispatch("d+10*20:30", h));
    EXPECT_STREQ("targetDec", h.lastCall);
}

TEST(MeadeSet, target_dec_malformed_does_not_call_handler)
{
    FakeHandlers h;
    EXPECT_STREQ("0", dispatch("d+84X03:02", h));
    EXPECT_EQ(nullptr, h.lastCall);
}

// ---- Target RA (r) -----------------------------------------------------

TEST(MeadeSet, target_ra_happy_path)
{
    FakeHandlers h;
    EXPECT_STREQ("1", dispatch("r04:03:02", h));
    EXPECT_STREQ("targetRa", h.lastCall);
    EXPECT_EQ(static_cast<uint8_t>(4), h.ra.hours);
    EXPECT_EQ(static_cast<uint8_t>(3), h.ra.minutes);
    EXPECT_EQ(static_cast<uint8_t>(2), h.ra.seconds);
}

TEST(MeadeSet, target_ra_malformed_does_not_call_handler)
{
    FakeHandlers h;
    EXPECT_STREQ("0", dispatch("r04-03-02", h));
    EXPECT_EQ(nullptr, h.lastCall);
}

// ---- Local Sidereal Time (HL) -----------------------------------------

TEST(MeadeSet, lst_with_seconds)
{
    FakeHandlers h;
    EXPECT_STREQ("1", dispatch("HL123456", h));
    EXPECT_STREQ("lst", h.lastCall);
    EXPECT_EQ(static_cast<uint8_t>(12), h.lst.hours);
    EXPECT_EQ(static_cast<uint8_t>(34), h.lst.minutes);
    EXPECT_EQ(static_cast<uint8_t>(56), h.lst.seconds);
}

TEST(MeadeSet, lst_without_seconds)
{
    FakeHandlers h;
    EXPECT_STREQ("1", dispatch("HL1234", h));
    EXPECT_EQ(static_cast<uint8_t>(12), h.lst.hours);
    EXPECT_EQ(static_cast<uint8_t>(34), h.lst.minutes);
    EXPECT_EQ(static_cast<uint8_t>(0), h.lst.seconds);
}

TEST(MeadeSet, lst_malformed_length_does_not_call_handler)
{
    FakeHandlers h;
    EXPECT_STREQ("0", dispatch("HL12345", h));
    EXPECT_EQ(nullptr, h.lastCall);
}

// ---- Home Point (HP) --------------------------------------------------

TEST(MeadeSet, home_point_happy_path)
{
    FakeHandlers h;
    EXPECT_STREQ("1", dispatch("HP", h));
    EXPECT_STREQ("home", h.lastCall);
}

TEST(MeadeSet, home_point_handler_failure_returns_zero)
{
    FakeHandlers h;
    h.nextResult = false;
    EXPECT_STREQ("0", dispatch("HP", h));
}

// ---- Hour Angle (H) ---------------------------------------------------

TEST(MeadeSet, hour_angle_happy_path)
{
    FakeHandlers h;
    EXPECT_STREQ("1", dispatch("H12:34", h));
    EXPECT_STREQ("ha", h.lastCall);
    EXPECT_EQ(static_cast<uint8_t>(12), h.haHours);
    EXPECT_EQ(static_cast<uint8_t>(34), h.haMinutes);
}

TEST(MeadeSet, hour_angle_malformed_does_not_call_handler)
{
    FakeHandlers h;
    EXPECT_STREQ("0", dispatch("H1X:34", h));
    EXPECT_EQ(nullptr, h.lastCall);
}

// ---- Sync Coordinates (Y) ---------------------------------------------

TEST(MeadeSet, sync_coordinates_happy_path)
{
    FakeHandlers h;
    EXPECT_STREQ("1", dispatch("Y+84*03:02.18:34:12", h));
    EXPECT_STREQ("sync", h.lastCall);
    EXPECT_EQ(84, h.syncDec.degrees);
    EXPECT_EQ(static_cast<uint8_t>(3), h.syncDec.minutes);
    EXPECT_EQ(static_cast<uint8_t>(2), h.syncDec.seconds);
    EXPECT_EQ(static_cast<uint8_t>(18), h.syncRa.hours);
    EXPECT_EQ(static_cast<uint8_t>(34), h.syncRa.minutes);
    EXPECT_EQ(static_cast<uint8_t>(12), h.syncRa.seconds);
}

TEST(MeadeSet, sync_coordinates_missing_dot_does_not_call_handler)
{
    FakeHandlers h;
    EXPECT_STREQ("0", dispatch("Y+84*03:02X18:34:12", h));
    EXPECT_EQ(nullptr, h.lastCall);
}

// ---- Site Latitude (t) ------------------------------------------------

TEST(MeadeSet, site_latitude_positive)
{
    FakeHandlers h;
    EXPECT_STREQ("1", dispatch("t+30*29", h));
    EXPECT_STREQ("lat", h.lastCall);
    EXPECT_EQ(30, h.lat.degrees);
    EXPECT_EQ(static_cast<uint8_t>(29), h.lat.minutes);
}

TEST(MeadeSet, site_latitude_negative_with_colon)
{
    FakeHandlers h;
    EXPECT_STREQ("1", dispatch("t-45:15", h));
    EXPECT_EQ(-45, h.lat.degrees);
    EXPECT_EQ(static_cast<uint8_t>(15), h.lat.minutes);
}

TEST(MeadeSet, site_latitude_malformed_does_not_call_handler)
{
    FakeHandlers h;
    EXPECT_STREQ("0", dispatch("t30*29", h));  // missing sign
    EXPECT_EQ(nullptr, h.lastCall);
}

// ---- Site Longitude (g) -----------------------------------------------

TEST(MeadeSet, site_longitude_three_digit_degrees)
{
    FakeHandlers h;
    EXPECT_STREQ("1", dispatch("g+097*34", h));
    EXPECT_STREQ("lon", h.lastCall);
    EXPECT_EQ(97, h.lon.degrees);
    EXPECT_EQ(static_cast<uint8_t>(34), h.lon.minutes);
}

TEST(MeadeSet, site_longitude_malformed_short_does_not_call_handler)
{
    FakeHandlers h;
    EXPECT_STREQ("0", dispatch("g+97*34", h));  // 2-digit degrees
    EXPECT_EQ(nullptr, h.lastCall);
}

// ---- UTC Offset (G) ---------------------------------------------------

TEST(MeadeSet, utc_offset_positive)
{
    FakeHandlers h;
    EXPECT_STREQ("1", dispatch("G+05", h));
    EXPECT_STREQ("utc", h.lastCall);
    EXPECT_EQ(5, h.utc);
}

TEST(MeadeSet, utc_offset_negative)
{
    FakeHandlers h;
    EXPECT_STREQ("1", dispatch("G-08", h));
    EXPECT_EQ(-8, h.utc);
}

TEST(MeadeSet, utc_offset_malformed_length_does_not_call_handler)
{
    FakeHandlers h;
    EXPECT_STREQ("0", dispatch("G+5", h));
    EXPECT_EQ(nullptr, h.lastCall);
}

// ---- Local Time (L) ---------------------------------------------------

TEST(MeadeSet, local_time_happy_path)
{
    FakeHandlers h;
    EXPECT_STREQ("1", dispatch("L19:33:03", h));
    EXPECT_STREQ("time", h.lastCall);
    EXPECT_EQ(static_cast<uint8_t>(19), h.time.hours);
    EXPECT_EQ(static_cast<uint8_t>(33), h.time.minutes);
    EXPECT_EQ(static_cast<uint8_t>(3), h.time.seconds);
}

TEST(MeadeSet, local_time_malformed_does_not_call_handler)
{
    FakeHandlers h;
    EXPECT_STREQ("0", dispatch("L19-33-03", h));
    EXPECT_EQ(nullptr, h.lastCall);
}

// ---- Local Date (C) ---------------------------------------------------

TEST(MeadeSet, local_date_success_emits_planetary_ack)
{
    FakeHandlers h;
    EXPECT_STREQ("1Updating Planetary Data#                              #", dispatch("C04/30/24", h));
    EXPECT_STREQ("date", h.lastCall);
    EXPECT_EQ(static_cast<uint8_t>(4), h.date.month);
    EXPECT_EQ(static_cast<uint8_t>(30), h.date.day);
    EXPECT_EQ(static_cast<uint16_t>(2024), h.date.year);
}

TEST(MeadeSet, local_date_failure_returns_zero_only)
{
    FakeHandlers h;
    h.nextResult = false;
    EXPECT_STREQ("0", dispatch("C04/30/24", h));
}

TEST(MeadeSet, local_date_malformed_does_not_call_handler)
{
    FakeHandlers h;
    EXPECT_STREQ("0", dispatch("C04-30-24", h));
    EXPECT_EQ(nullptr, h.lastCall);
}

// ---- Top-level routing ------------------------------------------------

TEST(MeadeSet, unknown_subcommand_returns_zero)
{
    FakeHandlers h;
    EXPECT_STREQ("0", dispatch("Z42", h));
    EXPECT_EQ(nullptr, h.lastCall);
}

TEST(MeadeSet, empty_suffix_returns_zero)
{
    FakeHandlers h;
    EXPECT_STREQ("0", dispatch("", h));
    EXPECT_EQ(nullptr, h.lastCall);
}
