// Wire-byte tests for the Meade Set-family dispatcher (`handleMeadeSet`).
//
// Each test exercises a single Meade `:S...` (or sync `:SY...`) sub-command
// suffix end-to-end: it calls the dispatcher with a stub handler and asserts
// the exact bytes emitted on the wire, plus the typed values the handler
// observed. The stub records which callback fired so we also catch silent
// regressions where the wrong handler is invoked.

#include <unity.h>

#include <string.h>

#include "core/MeadeParser.hpp"

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
    last = meade::handleMeadeSet(suffix, h);
    return last.c_str();
}

}  // namespace

namespace
{

// ---- Target DEC (d) ----------------------------------------------------

void test_set_target_dec_happy_path()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("1", dispatch("d+84*03:02", h));
    TEST_ASSERT_EQUAL_STRING("targetDec", h.lastCall);
    TEST_ASSERT_EQUAL_INT(84, h.dec.degrees);
    TEST_ASSERT_EQUAL_UINT8(3, h.dec.minutes);
    TEST_ASSERT_EQUAL_UINT8(2, h.dec.seconds);
}

void test_set_target_dec_negative_with_colon_separator()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("1", dispatch("d-12:45:30", h));
    TEST_ASSERT_EQUAL_INT(-12, h.dec.degrees);
    TEST_ASSERT_EQUAL_UINT8(45, h.dec.minutes);
    TEST_ASSERT_EQUAL_UINT8(30, h.dec.seconds);
}

void test_set_target_dec_handler_failure_returns_zero()
{
    FakeHandlers h;
    h.nextResult = false;
    TEST_ASSERT_EQUAL_STRING("0", dispatch("d+10*20:30", h));
    TEST_ASSERT_EQUAL_STRING("targetDec", h.lastCall);
}

void test_set_target_dec_malformed_does_not_call_handler()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("0", dispatch("d+84X03:02", h));
    TEST_ASSERT_NULL(h.lastCall);
}

// ---- Target RA (r) -----------------------------------------------------

void test_set_target_ra_happy_path()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("1", dispatch("r04:03:02", h));
    TEST_ASSERT_EQUAL_STRING("targetRa", h.lastCall);
    TEST_ASSERT_EQUAL_UINT8(4, h.ra.hours);
    TEST_ASSERT_EQUAL_UINT8(3, h.ra.minutes);
    TEST_ASSERT_EQUAL_UINT8(2, h.ra.seconds);
}

void test_set_target_ra_malformed_does_not_call_handler()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("0", dispatch("r04-03-02", h));
    TEST_ASSERT_NULL(h.lastCall);
}

// ---- Local Sidereal Time (HL) -----------------------------------------

void test_set_lst_with_seconds()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("1", dispatch("HL123456", h));
    TEST_ASSERT_EQUAL_STRING("lst", h.lastCall);
    TEST_ASSERT_EQUAL_UINT8(12, h.lst.hours);
    TEST_ASSERT_EQUAL_UINT8(34, h.lst.minutes);
    TEST_ASSERT_EQUAL_UINT8(56, h.lst.seconds);
}

void test_set_lst_without_seconds()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("1", dispatch("HL1234", h));
    TEST_ASSERT_EQUAL_UINT8(12, h.lst.hours);
    TEST_ASSERT_EQUAL_UINT8(34, h.lst.minutes);
    TEST_ASSERT_EQUAL_UINT8(0, h.lst.seconds);
}

void test_set_lst_malformed_length_does_not_call_handler()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("0", dispatch("HL12345", h));
    TEST_ASSERT_NULL(h.lastCall);
}

// ---- Home Point (HP) --------------------------------------------------

void test_set_home_point_happy_path()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("1", dispatch("HP", h));
    TEST_ASSERT_EQUAL_STRING("home", h.lastCall);
}

void test_set_home_point_handler_failure_returns_zero()
{
    FakeHandlers h;
    h.nextResult = false;
    TEST_ASSERT_EQUAL_STRING("0", dispatch("HP", h));
}

// ---- Hour Angle (H) ---------------------------------------------------

void test_set_hour_angle_happy_path()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("1", dispatch("H12:34", h));
    TEST_ASSERT_EQUAL_STRING("ha", h.lastCall);
    TEST_ASSERT_EQUAL_UINT8(12, h.haHours);
    TEST_ASSERT_EQUAL_UINT8(34, h.haMinutes);
}

void test_set_hour_angle_malformed_does_not_call_handler()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("0", dispatch("H1X:34", h));
    TEST_ASSERT_NULL(h.lastCall);
}

// ---- Sync Coordinates (Y) ---------------------------------------------

void test_sync_coordinates_happy_path()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("1", dispatch("Y+84*03:02.18:34:12", h));
    TEST_ASSERT_EQUAL_STRING("sync", h.lastCall);
    TEST_ASSERT_EQUAL_INT(84, h.syncDec.degrees);
    TEST_ASSERT_EQUAL_UINT8(3, h.syncDec.minutes);
    TEST_ASSERT_EQUAL_UINT8(2, h.syncDec.seconds);
    TEST_ASSERT_EQUAL_UINT8(18, h.syncRa.hours);
    TEST_ASSERT_EQUAL_UINT8(34, h.syncRa.minutes);
    TEST_ASSERT_EQUAL_UINT8(12, h.syncRa.seconds);
}

void test_sync_coordinates_missing_dot_does_not_call_handler()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("0", dispatch("Y+84*03:02X18:34:12", h));
    TEST_ASSERT_NULL(h.lastCall);
}

// ---- Site Latitude (t) ------------------------------------------------

void test_set_site_latitude_positive()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("1", dispatch("t+30*29", h));
    TEST_ASSERT_EQUAL_STRING("lat", h.lastCall);
    TEST_ASSERT_EQUAL_INT(30, h.lat.degrees);
    TEST_ASSERT_EQUAL_UINT8(29, h.lat.minutes);
}

void test_set_site_latitude_negative_with_colon()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("1", dispatch("t-45:15", h));
    TEST_ASSERT_EQUAL_INT(-45, h.lat.degrees);
    TEST_ASSERT_EQUAL_UINT8(15, h.lat.minutes);
}

void test_set_site_latitude_malformed_does_not_call_handler()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("0", dispatch("t30*29", h));  // missing sign
    TEST_ASSERT_NULL(h.lastCall);
}

// ---- Site Longitude (g) -----------------------------------------------

void test_set_site_longitude_three_digit_degrees()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("1", dispatch("g+097*34", h));
    TEST_ASSERT_EQUAL_STRING("lon", h.lastCall);
    TEST_ASSERT_EQUAL_INT(97, h.lon.degrees);
    TEST_ASSERT_EQUAL_UINT8(34, h.lon.minutes);
}

void test_set_site_longitude_malformed_short_does_not_call_handler()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("0", dispatch("g+97*34", h));  // 2-digit degrees
    TEST_ASSERT_NULL(h.lastCall);
}

// ---- UTC Offset (G) ---------------------------------------------------

void test_set_utc_offset_positive()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("1", dispatch("G+05", h));
    TEST_ASSERT_EQUAL_STRING("utc", h.lastCall);
    TEST_ASSERT_EQUAL_INT(5, h.utc);
}

void test_set_utc_offset_negative()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("1", dispatch("G-08", h));
    TEST_ASSERT_EQUAL_INT(-8, h.utc);
}

void test_set_utc_offset_malformed_length_does_not_call_handler()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("0", dispatch("G+5", h));
    TEST_ASSERT_NULL(h.lastCall);
}

// ---- Local Time (L) ---------------------------------------------------

void test_set_local_time_happy_path()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("1", dispatch("L19:33:03", h));
    TEST_ASSERT_EQUAL_STRING("time", h.lastCall);
    TEST_ASSERT_EQUAL_UINT8(19, h.time.hours);
    TEST_ASSERT_EQUAL_UINT8(33, h.time.minutes);
    TEST_ASSERT_EQUAL_UINT8(3, h.time.seconds);
}

void test_set_local_time_malformed_does_not_call_handler()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("0", dispatch("L19-33-03", h));
    TEST_ASSERT_NULL(h.lastCall);
}

// ---- Local Date (C) ---------------------------------------------------

void test_set_local_date_success_emits_planetary_ack()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("1Updating Planetary Data#                              #", dispatch("C04/30/24", h));
    TEST_ASSERT_EQUAL_STRING("date", h.lastCall);
    TEST_ASSERT_EQUAL_UINT8(4, h.date.month);
    TEST_ASSERT_EQUAL_UINT8(30, h.date.day);
    TEST_ASSERT_EQUAL_UINT16(2024, h.date.year);
}

void test_set_local_date_failure_returns_zero_only()
{
    FakeHandlers h;
    h.nextResult = false;
    TEST_ASSERT_EQUAL_STRING("0", dispatch("C04/30/24", h));
}

void test_set_local_date_malformed_does_not_call_handler()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("0", dispatch("C04-30-24", h));
    TEST_ASSERT_NULL(h.lastCall);
}

// ---- Top-level routing ------------------------------------------------

void test_unknown_set_subcommand_returns_zero()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("0", dispatch("Z42", h));
    TEST_ASSERT_NULL(h.lastCall);
}

void test_empty_suffix_returns_zero()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("0", dispatch("", h));
    TEST_ASSERT_NULL(h.lastCall);
}

}  // namespace

void register_meade_set_tests()
{
    RUN_TEST(test_set_target_dec_happy_path);
    RUN_TEST(test_set_target_dec_negative_with_colon_separator);
    RUN_TEST(test_set_target_dec_handler_failure_returns_zero);
    RUN_TEST(test_set_target_dec_malformed_does_not_call_handler);

    RUN_TEST(test_set_target_ra_happy_path);
    RUN_TEST(test_set_target_ra_malformed_does_not_call_handler);

    RUN_TEST(test_set_lst_with_seconds);
    RUN_TEST(test_set_lst_without_seconds);
    RUN_TEST(test_set_lst_malformed_length_does_not_call_handler);

    RUN_TEST(test_set_home_point_happy_path);
    RUN_TEST(test_set_home_point_handler_failure_returns_zero);

    RUN_TEST(test_set_hour_angle_happy_path);
    RUN_TEST(test_set_hour_angle_malformed_does_not_call_handler);

    RUN_TEST(test_sync_coordinates_happy_path);
    RUN_TEST(test_sync_coordinates_missing_dot_does_not_call_handler);

    RUN_TEST(test_set_site_latitude_positive);
    RUN_TEST(test_set_site_latitude_negative_with_colon);
    RUN_TEST(test_set_site_latitude_malformed_does_not_call_handler);

    RUN_TEST(test_set_site_longitude_three_digit_degrees);
    RUN_TEST(test_set_site_longitude_malformed_short_does_not_call_handler);

    RUN_TEST(test_set_utc_offset_positive);
    RUN_TEST(test_set_utc_offset_negative);
    RUN_TEST(test_set_utc_offset_malformed_length_does_not_call_handler);

    RUN_TEST(test_set_local_time_happy_path);
    RUN_TEST(test_set_local_time_malformed_does_not_call_handler);

    RUN_TEST(test_set_local_date_success_emits_planetary_ack);
    RUN_TEST(test_set_local_date_failure_returns_zero_only);
    RUN_TEST(test_set_local_date_malformed_does_not_call_handler);

    RUN_TEST(test_unknown_set_subcommand_returns_zero);
    RUN_TEST(test_empty_suffix_returns_zero);
}
