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
    EXPECT_EQ(static_cast<uint16_t>(84), h.dec.degrees);
    EXPECT_EQ(static_cast<uint8_t>(3), h.dec.minutes);
    EXPECT_EQ(static_cast<uint8_t>(2), h.dec.seconds);
    EXPECT_FALSE(h.dec.negative);
}

TEST(MeadeSet, target_dec_negative_with_colon_separator)
{
    FakeHandlers h;
    EXPECT_STREQ("1", dispatch("d-12:45:30", h));
    EXPECT_EQ(static_cast<uint16_t>(12), h.dec.degrees);
    EXPECT_EQ(static_cast<uint8_t>(45), h.dec.minutes);
    EXPECT_EQ(static_cast<uint8_t>(30), h.dec.seconds);
    EXPECT_TRUE(h.dec.negative);
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
    EXPECT_EQ(static_cast<uint16_t>(84), h.syncDec.degrees);
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
    EXPECT_EQ(static_cast<uint16_t>(30), h.lat.degrees);
    EXPECT_EQ(static_cast<uint8_t>(29), h.lat.minutes);
    EXPECT_FALSE(h.lat.negative);
}

TEST(MeadeSet, site_latitude_negative_with_colon)
{
    FakeHandlers h;
    EXPECT_STREQ("1", dispatch("t-45:15", h));
    EXPECT_EQ(static_cast<uint16_t>(45), h.lat.degrees);
    EXPECT_EQ(static_cast<uint8_t>(15), h.lat.minutes);
    EXPECT_TRUE(h.lat.negative);
}

TEST(MeadeSet, site_latitude_malformed_does_not_call_handler)
{
    FakeHandlers h;
    EXPECT_STREQ("0", dispatch("t30*29", h));  // missing sign
    EXPECT_EQ(nullptr, h.lastCall);
}

// ---- Site Longitude (g) -----------------------------------------------

// :Sg is east-negative, so a '+' on the wire is a WEST longitude and reaches
// the east-positive struct as negative.
TEST(MeadeSet, site_longitude_three_digit_degrees)
{
    FakeHandlers h;
    EXPECT_STREQ("1", dispatch("g+097*34", h));
    EXPECT_STREQ("lon", h.lastCall);
    EXPECT_EQ(static_cast<uint16_t>(97), h.lon.degrees);
    EXPECT_EQ(static_cast<uint8_t>(34), h.lon.minutes);
    EXPECT_TRUE(h.lon.negative);
}

TEST(MeadeSet, site_longitude_malformed_short_does_not_call_handler)
{
    FakeHandlers h;
    EXPECT_STREQ("0", dispatch("g+97*34", h));  // 2-digit degrees
    EXPECT_EQ(nullptr, h.lastCall);
}

// A '-' on the wire is an EAST longitude, which the east-positive struct holds
// as a positive value. #291 dropped the negation on both sides at once, so the
// convention round-tripped perfectly while being backwards; this assertion is
// on the struct rather than the wire so that a future flip cannot hide the
// same way.
TEST(MeadeSet, site_longitude_signed_negative_is_east)
{
    FakeHandlers h;
    EXPECT_STREQ("1", dispatch("g-121*53", h));
    EXPECT_STREQ("lon", h.lastCall);
    EXPECT_EQ(static_cast<uint16_t>(121), h.lon.degrees);
    EXPECT_EQ(static_cast<uint8_t>(53), h.lon.minutes);
    EXPECT_FALSE(h.lon.negative);
}

// Unsigned longitudes count WESTWARD from Greenwich, 0..360, and are mirrored into
// the east-positive range the mount stores. INDI sends this form: a San Jose site
// at 121d53' west arrives as ":Sg121*53#" and must come back out as -121d53'.
TEST(MeadeSet, site_longitude_unsigned_west_of_greenwich)
{
    FakeHandlers h;
    EXPECT_STREQ("1", dispatch("g121*53", h));
    EXPECT_STREQ("lon", h.lastCall);
    EXPECT_EQ(static_cast<uint16_t>(121), h.lon.degrees);
    EXPECT_EQ(static_cast<uint8_t>(53), h.lon.minutes);
    EXPECT_TRUE(h.lon.negative);
}

// The unsigned form is not a second convention, it is the signed one with the
// sign taken as '+'. These two spellings of the same meridian must agree.
TEST(MeadeSet, site_longitude_unsigned_and_signed_agree)
{
    FakeHandlers u, s;
    EXPECT_STREQ("1", dispatch("g121*53", u));
    EXPECT_STREQ("1", dispatch("g+121*53", s));
    EXPECT_EQ(u.lon.degrees, s.lon.degrees);
    EXPECT_EQ(u.lon.minutes, s.lon.minutes);
    EXPECT_EQ(u.lon.negative, s.lon.negative);
}

// Past 180 the westward count has gone round to the eastern hemisphere.
TEST(MeadeSet, site_longitude_unsigned_east_of_greenwich)
{
    FakeHandlers h;
    EXPECT_STREQ("1", dispatch("g301*53", h));
    EXPECT_EQ(static_cast<uint16_t>(58), h.lon.degrees);
    EXPECT_EQ(static_cast<uint8_t>(7), h.lon.minutes);
    EXPECT_FALSE(h.lon.negative);
}

TEST(MeadeSet, site_longitude_unsigned_greenwich_is_zero)
{
    FakeHandlers h;
    EXPECT_STREQ("1", dispatch("g000*00", h));
    EXPECT_EQ(static_cast<uint16_t>(0), h.lon.degrees);
    EXPECT_EQ(static_cast<uint8_t>(0), h.lon.minutes);
    EXPECT_FALSE(h.lon.negative);
}

// 180 west and 180 east are the same meridian, so either sign would be right. This
// pins the half of the choice the parser makes — it wraps into (-180, 180], keeping
// the antimeridian positive — rather than leaving it for a reader to infer.
TEST(MeadeSet, site_longitude_unsigned_antimeridian_stays_positive)
{
    FakeHandlers h;
    EXPECT_STREQ("1", dispatch("g180*00", h));
    EXPECT_EQ(static_cast<uint16_t>(180), h.lon.degrees);
    EXPECT_EQ(static_cast<uint8_t>(0), h.lon.minutes);
    EXPECT_FALSE(h.lon.negative);
}

// Top of the accepted range: one arcminute short of a full circle west is one
// arcminute east.
TEST(MeadeSet, site_longitude_unsigned_upper_bound_wraps_to_east)
{
    FakeHandlers h;
    EXPECT_STREQ("1", dispatch("g359*59", h));
    EXPECT_EQ(static_cast<uint16_t>(0), h.lon.degrees);
    EXPECT_EQ(static_cast<uint8_t>(1), h.lon.minutes);
    EXPECT_FALSE(h.lon.negative);
}

// A west longitude smaller than one degree used to be unrepresentable: the sign
// lived in `degrees`, which is 0 here, so "000*30" (30' WEST) and "359*30" (30'
// east) both came out {0, 30} and were read downstream as 30' EAST -- a silent
// 1-degree error with a "1" reply. The separate `negative` field is what tells
// the two apart.
TEST(MeadeSet, site_longitude_unsigned_sub_degree_west_keeps_its_sign)
{
    FakeHandlers h;
    EXPECT_STREQ("1", dispatch("g000*30", h));
    EXPECT_STREQ("lon", h.lastCall);
    EXPECT_EQ(static_cast<uint16_t>(0), h.lon.degrees);
    EXPECT_EQ(static_cast<uint8_t>(30), h.lon.minutes);
    EXPECT_TRUE(h.lon.negative);

    // The east neighbour it used to collide with.
    FakeHandlers e;
    EXPECT_STREQ("1", dispatch("g359*30", e));
    EXPECT_EQ(static_cast<uint16_t>(0), e.lon.degrees);
    EXPECT_EQ(static_cast<uint8_t>(30), e.lon.minutes);
    EXPECT_FALSE(e.lon.negative);
}

// 360 west is the same meridian as 000, but it is refused rather than wrapped: the
// range check is what stops out-of-circle degrees reaching EEPROMStore, which clamps
// them into an int16 and persists a site that is wrong rather than merely unwrapped.
TEST(MeadeSet, site_longitude_unsigned_full_circle_is_rejected)
{
    FakeHandlers h;
    EXPECT_STREQ("0", dispatch("g360*00", h));
    EXPECT_EQ(nullptr, h.lastCall);
}

// The range check is shared, so it guards the signed path too.
TEST(MeadeSet, site_longitude_signed_out_of_range_does_not_call_handler)
{
    FakeHandlers h;
    EXPECT_STREQ("0", dispatch("g+400*00", h));
    EXPECT_EQ(nullptr, h.lastCall);
}

TEST(MeadeSet, site_longitude_minutes_out_of_range_does_not_call_handler)
{
    FakeHandlers h;
    EXPECT_STREQ("0", dispatch("g+121*99", h));
    EXPECT_EQ(nullptr, h.lastCall);
}

// Degrees this far out would also push the westward arcminute count past INT16_MAX,
// which is why the conversion works in `long` as well as rejecting the input.
TEST(MeadeSet, site_longitude_unsigned_beyond_int16_arcminutes_is_rejected)
{
    FakeHandlers h;
    EXPECT_STREQ("0", dispatch("g545*69", h));
    EXPECT_EQ(nullptr, h.lastCall);
}

// Two-digit degrees are refused on both paths. Pre-#291 DayTime::ParseFromMeade took
// two or three, so this is stricter than the legacy parser for a client that sends
// ":Sg97*34#"; nothing observed on the wire does, MeadeProtocol.hpp documents "DDD",
// and Cursor never backtracks, so accepting either width means hand-rolling the digit
// reads. Relaxing it should relax the signed path at the same time.
TEST(MeadeSet, site_longitude_unsigned_two_digit_degrees_does_not_call_handler)
{
    FakeHandlers h;
    EXPECT_STREQ("0", dispatch("g97*34", h));
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

// The exact bytes INDI puts on the wire when it pushes the site on connect.
TEST(MeadeSet, utc_offset_indi_fractional_form)
{
    FakeHandlers h;
    EXPECT_STREQ("1", dispatch("G+7.0", h));
    EXPECT_STREQ("utc", h.lastCall);
    EXPECT_EQ(7, h.utc);
}

TEST(MeadeSet, utc_offset_single_digit_positive)
{
    FakeHandlers h;
    EXPECT_STREQ("1", dispatch("G+5", h));
    EXPECT_EQ(5, h.utc);
}

TEST(MeadeSet, utc_offset_single_digit_negative)
{
    FakeHandlers h;
    EXPECT_STREQ("1", dispatch("G-3", h));
    EXPECT_EQ(-3, h.utc);
}

// The sign is required. Pre-#291 the offset went through String::toInt(), which
// accepts an unsigned value, so this is a narrowing rather than a restoration --
// but a missing sign is far more likely a client bug than a deliberate "+", and
// guessing wrong puts local sidereal time out by twice the offset.
TEST(MeadeSet, utc_offset_unsigned_is_rejected)
{
    FakeHandlers h;
    EXPECT_STREQ("0", dispatch("G07", h));
    EXPECT_EQ(nullptr, h.lastCall);
}

// Half-hour zones (India, Newfoundland) are unrepresentable: onSetUtcOffset takes
// whole hours, so ".5" is left unconsumed and the site lands 30 minutes out.
// Pinned here so the limitation is documented rather than discovered in the field.
TEST(MeadeSet, utc_offset_half_hour_zone_drops_the_fraction)
{
    FakeHandlers h;
    EXPECT_STREQ("1", dispatch("G+5.5", h));
    EXPECT_EQ(5, h.utc);
}

// Nothing range-checks the hours. "+13" is a real offset (Tonga); "-15" is not,
// and is taken all the same. Both pin the current permissive behaviour --
// whether to reject impossible offsets is deliberately left to a follow-up.
// Note that IMeadeSetHandlers::onSetUtcOffset documents "@param hours Signed
// wire value (-12..+14)"; that range is stated but has never been enforced,
// here or before this parser accepted the unsigned and single-digit forms.
TEST(MeadeSet, utc_offset_two_digit_high_value)
{
    FakeHandlers h;
    EXPECT_STREQ("1", dispatch("G+13", h));
    EXPECT_STREQ("utc", h.lastCall);
    EXPECT_EQ(13, h.utc);
}

TEST(MeadeSet, utc_offset_impossible_value_is_accepted)
{
    FakeHandlers h;
    EXPECT_STREQ("1", dispatch("G-15", h));
    EXPECT_STREQ("utc", h.lastCall);
    EXPECT_EQ(-15, h.utc);
}

TEST(MeadeSet, utc_offset_sign_without_digits_does_not_call_handler)
{
    FakeHandlers h;
    EXPECT_STREQ("0", dispatch("G+", h));
    EXPECT_EQ(nullptr, h.lastCall);
}

TEST(MeadeSet, utc_offset_non_numeric_does_not_call_handler)
{
    FakeHandlers h;
    EXPECT_STREQ("0", dispatch("Gx", h));
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

// ---- Sign of zero -----------------------------------------------------
//
// Every wire format in this family puts the sign in front of a degrees field
// that can legitimately be zero. Half a degree south of the equator is
// "-00*30:00", and reading it as "+00*30:00" is a one-degree error.

TEST(MeadeSet, target_dec_negative_zero_degrees_keeps_sign)
{
    FakeHandlers h;
    EXPECT_STREQ("1", dispatch("d-00*30:00", h));
    EXPECT_EQ(static_cast<uint16_t>(0), h.dec.degrees);
    EXPECT_EQ(static_cast<uint8_t>(30), h.dec.minutes);
    EXPECT_EQ(static_cast<uint8_t>(0), h.dec.seconds);
    EXPECT_TRUE(h.dec.negative);
}

TEST(MeadeSet, target_dec_positive_zero_degrees_keeps_sign)
{
    FakeHandlers h;
    EXPECT_STREQ("1", dispatch("d+00*30:00", h));
    EXPECT_EQ(static_cast<uint16_t>(0), h.dec.degrees);
    EXPECT_EQ(static_cast<uint8_t>(30), h.dec.minutes);
    EXPECT_FALSE(h.dec.negative);
}

TEST(MeadeSet, sync_coordinates_negative_zero_degrees_keeps_sign)
{
    FakeHandlers h;
    EXPECT_STREQ("1", dispatch("Y-00*30:00.18:34:12", h));
    EXPECT_STREQ("sync", h.lastCall);
    EXPECT_EQ(static_cast<uint16_t>(0), h.syncDec.degrees);
    EXPECT_EQ(static_cast<uint8_t>(30), h.syncDec.minutes);
    EXPECT_TRUE(h.syncDec.negative);
}

TEST(MeadeSet, site_latitude_negative_zero_degrees_keeps_sign)
{
    FakeHandlers h;
    EXPECT_STREQ("1", dispatch("t-00*30", h));
    EXPECT_EQ(static_cast<uint16_t>(0), h.lat.degrees);
    EXPECT_EQ(static_cast<uint8_t>(30), h.lat.minutes);
    EXPECT_TRUE(h.lat.negative);
}

TEST(MeadeSet, site_latitude_positive_zero_degrees_keeps_sign)
{
    FakeHandlers h;
    EXPECT_STREQ("1", dispatch("t+00*30", h));
    EXPECT_EQ(static_cast<uint16_t>(0), h.lat.degrees);
    EXPECT_FALSE(h.lat.negative);
}

TEST(MeadeSet, site_longitude_negative_zero_degrees_keeps_sign)
{
    FakeHandlers h;
    EXPECT_STREQ("1", dispatch("g-000*05", h));  // 5' EAST on an east-negative wire
    EXPECT_EQ(static_cast<uint16_t>(0), h.lon.degrees);
    EXPECT_EQ(static_cast<uint8_t>(5), h.lon.minutes);
    EXPECT_FALSE(h.lon.negative);
}

TEST(MeadeSet, site_longitude_positive_zero_degrees_keeps_sign)
{
    FakeHandlers h;
    EXPECT_STREQ("1", dispatch("g+000*05", h));  // 5' WEST
    EXPECT_EQ(static_cast<uint16_t>(0), h.lon.degrees);
    EXPECT_EQ(static_cast<uint8_t>(5), h.lon.minutes);
    EXPECT_TRUE(h.lon.negative);
}

TEST(MeadeSet, utc_offset_negative_zero_is_zero)
{
    FakeHandlers h;
    h.utc = 99;  // Poison, so the assertion below cannot pass on the default.
    EXPECT_STREQ("1", dispatch("G-00", h));
    EXPECT_STREQ("utc", h.lastCall);
    EXPECT_EQ(0, h.utc);
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
