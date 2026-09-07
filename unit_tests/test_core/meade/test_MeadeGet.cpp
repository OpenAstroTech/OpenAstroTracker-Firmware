// Wire-byte tests for the Meade Get-family dispatcher (`handleMeadeGet`).
//
// Each test exercises a single Meade `:G...` sub-command suffix end-to-end:
// it calls the parser entry point with a stub handler and asserts the exact
// bytes emitted on the wire. The stub records which callback fired so we
// also catch silent regressions where the wrong handler is invoked.

#include <gtest/gtest.h>

#include <string.h>

#include "core/meade/MeadeParser.hpp"

namespace meade = oat::core::meade;

namespace
{

class FakeHandlers : public meade::IMeadeGetHandlers
{
  public:
    const char *lastCall = nullptr;

    // ---- Defaults (overridable per test via direct member assignment) ----
    const char *firmware  = "V1.2.3";
    const char *product   = "OpenAstroTracker";
    const char *status    = "Idle,---,0,0";
    const char *siteNames = nullptr;  // when null, default OAT<index> is returned

    meade::RaCoordinate currentRa      = {1, 2, 3};
    meade::RaCoordinate targetRa       = {4, 5, 6};
    meade::DecCoordinate currentDec    = {7, 8, 9, false};
    meade::DecCoordinate targetDec     = {10, 11, 12, true};
    bool isSlewing                     = false;
    bool isTracking                    = true;
    bool isGuiding                     = false;
    meade::MeadeLatitude latitude      = {47, 30, false};
    meade::MeadeLongitude longitude    = {12, 30, true};
    int utcOffset                      = -5;
    meade::MeadeLocalTime localTime    = {14, 45, 6};
    meade::MeadeLocalDate localDate    = {3, 7, 2024};
    meade::MeadeClockFormat clockFmt   = meade::MeadeClockFormat::Hours24;
    meade::MeadeTrackingRate trackRate = meade::MeadeTrackingRate::Sidereal;

    char siteScratch[8] = {0};

    const char *onFirmwareVersion() override
    {
        lastCall = "fw";
        return firmware;
    }
    const char *onProductName() override
    {
        lastCall = "product";
        return product;
    }
    meade::RaCoordinate onCurrentRa() override
    {
        lastCall = "currentRa";
        return currentRa;
    }
    meade::RaCoordinate onTargetRa() override
    {
        lastCall = "targetRa";
        return targetRa;
    }
    meade::DecCoordinate onCurrentDec() override
    {
        lastCall = "currentDec";
        return currentDec;
    }
    meade::DecCoordinate onTargetDec() override
    {
        lastCall = "targetDec";
        return targetDec;
    }
    const char *onMountStatus() override
    {
        lastCall = "status";
        return status;
    }
    bool onIsSlewing() override
    {
        lastCall = "slewing";
        return isSlewing;
    }
    bool onIsTracking() override
    {
        lastCall = "tracking";
        return isTracking;
    }
    bool onIsGuiding() override
    {
        lastCall = "guiding";
        return isGuiding;
    }
    meade::MeadeLatitude onSiteLatitude() override
    {
        lastCall = "lat";
        return latitude;
    }
    meade::MeadeLongitude onSiteLongitude() override
    {
        lastCall = "lon";
        return longitude;
    }
    int onUtcOffset() override
    {
        lastCall = "utc";
        return utcOffset;
    }
    meade::MeadeLocalTime onLocalTime() override
    {
        lastCall = "time";
        return localTime;
    }
    meade::MeadeLocalDate onLocalDate() override
    {
        lastCall = "date";
        return localDate;
    }
    meade::MeadeClockFormat onClockFormat() override
    {
        lastCall = "clock";
        return clockFmt;
    }
    meade::MeadeTrackingRate onTrackingRate() override
    {
        lastCall = "rate";
        return trackRate;
    }
    const char *onSiteName(uint8_t index) override
    {
        lastCall = "siteName";
        if (siteNames)
        {
            return siteNames;
        }
        siteScratch[0] = 'O';
        siteScratch[1] = 'A';
        siteScratch[2] = 'T';
        siteScratch[3] = static_cast<char>('0' + index);
        siteScratch[4] = '\0';
        return siteScratch;
    }
};

const char *dispatch(const char *suffix, FakeHandlers &h)
{
    static meade::MeadeResponse last;
    last.clear();
    meade::handleMeadeGet(last, suffix, h);
    return last.c_str();
}

// Pipes the values the Set family parses back into the Get family's fake, so
// a test can drive `:S...` and read the result out through `:G...`.
//
// This joins the two parser families and nothing else. A real client's bytes
// also pass through MeadeCommandProcessor and the Declination / Latitude /
// Longitude types, which no native test reaches; passing here does not mean
// the mount stores what was sent.
class RoundTripSetHandlers : public meade::IMeadeSetHandlers
{
  public:
    explicit RoundTripSetHandlers(FakeHandlers &sink) : _sink(sink)
    {
    }

    bool onSetTargetDec(meade::DecCoordinate v) override
    {
        _sink.currentDec = v;
        _sink.targetDec  = v;
        return true;
    }
    bool onSetSiteLatitude(meade::MeadeLatitude v) override
    {
        _sink.latitude = v;
        return true;
    }
    bool onSetSiteLongitude(meade::MeadeLongitude v) override
    {
        _sink.longitude = v;
        return true;
    }

    bool onSetTargetRa(meade::RaCoordinate) override
    {
        return true;
    }
    bool onSetLocalSiderealTime(meade::MeadeLocalTime) override
    {
        return true;
    }
    bool onSetHomePoint() override
    {
        return true;
    }
    bool onSetHourAngle(uint8_t, uint8_t) override
    {
        return true;
    }
    bool onSyncCoordinates(meade::DecCoordinate, meade::RaCoordinate) override
    {
        return true;
    }
    bool onSetUtcOffset(int) override
    {
        return true;
    }
    bool onSetLocalTime(meade::MeadeLocalTime) override
    {
        return true;
    }
    bool onSetLocalDate(meade::MeadeLocalDate) override
    {
        return true;
    }

  private:
    FakeHandlers &_sink;
};

// Runs one `:S...` suffix through the Set dispatcher into `h`, asserting the
// "1" ack, then returns the bytes the matching `:G...` suffix emits.
const char *setThenGet(const char *setSuffix, const char *getSuffix, FakeHandlers &h)
{
    meade::MeadeResponse ack;
    RoundTripSetHandlers sink(h);
    meade::handleMeadeSet(ack, setSuffix, sink);
    EXPECT_STREQ("1", ack.c_str());
    return dispatch(getSuffix, h);
}

}  // namespace

TEST(MeadeGet, firmware_version_two_char_command)
{
    FakeHandlers h;
    EXPECT_STREQ("V1.2.3#", dispatch("VN", h));
    EXPECT_STREQ("fw", h.lastCall);
}

TEST(MeadeGet, product_name_two_char_command)
{
    FakeHandlers h;
    EXPECT_STREQ("OpenAstroTracker#", dispatch("VP", h));
    EXPECT_STREQ("product", h.lastCall);
}

TEST(MeadeGet, current_ra_formats_hh_mm_ss)
{
    FakeHandlers h;
    h.currentRa = {14, 45, 6};
    EXPECT_STREQ("14:45:06#", dispatch("R", h));
    EXPECT_STREQ("currentRa", h.lastCall);
}

TEST(MeadeGet, target_ra_formats_hh_mm_ss)
{
    FakeHandlers h;
    h.targetRa = {0, 0, 0};
    EXPECT_STREQ("00:00:00#", dispatch("r", h));
    EXPECT_STREQ("targetRa", h.lastCall);
}

TEST(MeadeGet, current_dec_signed_dms)
{
    FakeHandlers h;
    h.currentDec = {47, 30, 15, false};
    EXPECT_STREQ("+47*30'15#", dispatch("D", h));
    EXPECT_STREQ("currentDec", h.lastCall);
}

TEST(MeadeGet, target_dec_negative)
{
    FakeHandlers h;
    h.targetDec = {12, 45, 0, true};
    EXPECT_STREQ("-12*45'00#", dispatch("d", h));
    EXPECT_STREQ("targetDec", h.lastCall);
}

TEST(MeadeGet, mount_status_passes_through)
{
    FakeHandlers h;
    h.status = "Idle,---,0,0";
    EXPECT_STREQ("Idle,---,0,0#", dispatch("X", h));
    EXPECT_STREQ("status", h.lastCall);
}

TEST(MeadeGet, is_slewing_emits_zero_one)
{
    FakeHandlers h;
    h.isSlewing = true;
    EXPECT_STREQ("1#", dispatch("IS", h));
    EXPECT_STREQ("slewing", h.lastCall);
    h.isSlewing = false;
    EXPECT_STREQ("0#", dispatch("IS", h));
}

TEST(MeadeGet, is_tracking_emits_zero_one)
{
    FakeHandlers h;
    h.isTracking = false;
    EXPECT_STREQ("0#", dispatch("IT", h));
    EXPECT_STREQ("tracking", h.lastCall);
}

TEST(MeadeGet, is_guiding_emits_zero_one)
{
    FakeHandlers h;
    h.isGuiding = true;
    EXPECT_STREQ("1#", dispatch("IG", h));
    EXPECT_STREQ("guiding", h.lastCall);
}

TEST(MeadeGet, site_latitude_signed_two_digit_deg)
{
    FakeHandlers h;
    h.latitude = {47, 30, false};
    EXPECT_STREQ("+47*30#", dispatch("t", h));
    h.latitude = {12, 45, true};
    EXPECT_STREQ("-12*45#", dispatch("t", h));
}

// :Gg is east-negative and MeadeLongitude is east-positive, so the sign on the
// wire is the opposite of `negative`. This has to stay the exact inverse of
// readLongitude; see the round trips below.
TEST(MeadeGet, site_longitude_signed_three_digit_deg)
{
    FakeHandlers h;
    h.longitude = {12, 30, false};  // 12d30' east
    EXPECT_STREQ("-012*30#", dispatch("g", h));
    h.longitude = {122, 45, true};  // 122d45' west
    EXPECT_STREQ("+122*45#", dispatch("g", h));
}

// ---- Sign of zero -----------------------------------------------------
//
// A coordinate whose degrees component is zero still has a hemisphere. The
// magnitude and the sign are separate struct fields precisely so that these
// four replies do not all collapse onto the '+' form.

TEST(MeadeGet, dec_zero_degrees_keeps_south_sign)
{
    FakeHandlers h;
    h.currentDec = {0, 30, 0, true};
    EXPECT_STREQ("-00*30'00#", dispatch("D", h));
    h.currentDec = {0, 30, 0, false};
    EXPECT_STREQ("+00*30'00#", dispatch("D", h));
}

TEST(MeadeGet, site_latitude_zero_degrees_keeps_south_sign)
{
    FakeHandlers h;
    h.latitude = {0, 30, true};
    EXPECT_STREQ("-00*30#", dispatch("t", h));
    h.latitude = {0, 30, false};
    EXPECT_STREQ("+00*30#", dispatch("t", h));
}

TEST(MeadeGet, site_longitude_zero_degrees_keeps_sign)
{
    FakeHandlers h;
    h.longitude = {0, 5, true};  // 5' west
    EXPECT_STREQ("+000*05#", dispatch("g", h));
    h.longitude = {0, 5, false};  // 5' east
    EXPECT_STREQ("-000*05#", dispatch("g", h));
}

// ---- Set -> Get round trips -------------------------------------------
//
// The sign has to survive the wire -> struct -> wire journey, not just one
// leg of it. `-00*30:00` is the case that used to come back as `+00*30'00`.

TEST(MeadeGet, dec_round_trip_preserves_sign_of_zero)
{
    FakeHandlers h;
    EXPECT_STREQ("-00*30'00#", setThenGet("d-00*30:00", "D", h));
    EXPECT_STREQ("+00*30'00#", setThenGet("d+00*30:00", "D", h));
}

TEST(MeadeGet, dec_round_trip_preserves_nonzero_degrees)
{
    FakeHandlers h;
    EXPECT_STREQ("-12*45'30#", setThenGet("d-12*45:30", "D", h));
    EXPECT_STREQ("+84*03'02#", setThenGet("d+84*03:02", "D", h));
}

TEST(MeadeGet, site_latitude_round_trip_preserves_sign_of_zero)
{
    FakeHandlers h;
    EXPECT_STREQ("-00*30#", setThenGet("t-00*30", "t", h));
    EXPECT_STREQ("+00*30#", setThenGet("t+00*30", "t", h));
}

TEST(MeadeGet, site_latitude_round_trip_preserves_nonzero_degrees)
{
    FakeHandlers h;
    EXPECT_STREQ("-45*15#", setThenGet("t-45:15", "t", h));
    EXPECT_STREQ("+47*30#", setThenGet("t+47*30", "t", h));
}

TEST(MeadeGet, site_longitude_round_trip_preserves_sign_of_zero)
{
    FakeHandlers h;
    EXPECT_STREQ("-000*05#", setThenGet("g-000*05", "g", h));
    EXPECT_STREQ("+000*05#", setThenGet("g+000*05", "g", h));
}

TEST(MeadeGet, site_longitude_round_trip_preserves_nonzero_degrees)
{
    FakeHandlers h;
    EXPECT_STREQ("-122*45#", setThenGet("g-122*45", "g", h));
    EXPECT_STREQ("+097*34#", setThenGet("g+097*34", "g", h));
}

// The reader and the writer both flip the sign, so the wire value is unchanged
// by a round trip -- which is exactly why a flip on one side alone is invisible
// to a client and has to be caught by the struct-level assertions above.
TEST(MeadeGet, site_longitude_round_trip_is_unchanged_at_the_meridians)
{
    FakeHandlers h;
    EXPECT_STREQ("+000*00#", setThenGet("g+000*00", "g", h));
    EXPECT_STREQ("+000*00#", setThenGet("g-000*00", "g", h));
    EXPECT_STREQ("-180*00#", setThenGet("g-180*00", "g", h));
}

// The form INDI actually sends: unsigned, counting westward. It comes back in
// the signed form, on the same meridian.
TEST(MeadeGet, site_longitude_unsigned_round_trips_to_the_same_meridian)
{
    FakeHandlers h;
    EXPECT_STREQ("+121*53#", setThenGet("g121*53", "g", h));
    EXPECT_EQ(static_cast<uint16_t>(121), h.longitude.degrees);
    EXPECT_EQ(static_cast<uint8_t>(53), h.longitude.minutes);
    EXPECT_TRUE(h.longitude.negative);  // west, east-positive internally

    FakeHandlers e;
    EXPECT_STREQ("-058*07#", setThenGet("g301*53", "g", e));
    EXPECT_EQ(static_cast<uint16_t>(58), e.longitude.degrees);
    EXPECT_EQ(static_cast<uint8_t>(7), e.longitude.minutes);
    EXPECT_FALSE(e.longitude.negative);
}

TEST(MeadeGet, utc_offset_signs_and_pads)
{
    FakeHandlers h;
    h.utcOffset = -5;
    EXPECT_STREQ("-05#", dispatch("G", h));
    h.utcOffset = 3;
    EXPECT_STREQ("+03#", dispatch("G", h));
}

TEST(MeadeGet, local_time_24h_format)
{
    FakeHandlers h;
    h.localTime = {14, 45, 6};
    EXPECT_STREQ("14:45:06#", dispatch("L", h));
    EXPECT_STREQ("time", h.lastCall);
}

TEST(MeadeGet, local_time_12h_converts_pm)
{
    FakeHandlers h;
    h.localTime = {14, 45, 6};  // 14:xx -> 02:xx in 12h
    EXPECT_STREQ("02:45:06#", dispatch("a", h));
    h.localTime = {0, 30, 0};  // 00 -> 12
    EXPECT_STREQ("12:30:00#", dispatch("a", h));
    h.localTime = {7, 8, 9};  // morning unchanged
    EXPECT_STREQ("07:08:09#", dispatch("a", h));
}

TEST(MeadeGet, local_date_truncates_year_to_two_digits)
{
    FakeHandlers h;
    h.localDate = {3, 7, 2024};
    EXPECT_STREQ("03/07/24#", dispatch("C", h));
    EXPECT_STREQ("date", h.lastCall);
}

TEST(MeadeGet, clock_format_24h)
{
    FakeHandlers h;
    h.clockFmt = meade::MeadeClockFormat::Hours24;
    EXPECT_STREQ("24#", dispatch("c", h));
    h.clockFmt = meade::MeadeClockFormat::Hours12;
    EXPECT_STREQ("12#", dispatch("c", h));
}

TEST(MeadeGet, tracking_rate_sidereal)
{
    FakeHandlers h;
    h.trackRate = meade::MeadeTrackingRate::Sidereal;
    EXPECT_STREQ("60.0#", dispatch("T", h));
}

TEST(MeadeGet, site_name_slots_invoke_handler_with_index)
{
    FakeHandlers h;
    EXPECT_STREQ("OAT1#", dispatch("M", h));
    EXPECT_STREQ("OAT2#", dispatch("N", h));
    EXPECT_STREQ("OAT3#", dispatch("O", h));
    EXPECT_STREQ("OAT4#", dispatch("P", h));
    EXPECT_STREQ("siteName", h.lastCall);
}

TEST(MeadeGet, unknown_suffix_returns_empty)
{
    FakeHandlers h;
    EXPECT_STREQ("", dispatch("ZZ", h));
    EXPECT_STREQ("", dispatch("Q", h));
    EXPECT_STREQ("", dispatch("", h));
    EXPECT_EQ(nullptr, h.lastCall);
}
