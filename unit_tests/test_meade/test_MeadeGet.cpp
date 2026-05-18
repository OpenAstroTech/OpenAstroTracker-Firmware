// Wire-byte tests for the Meade Get-family dispatcher (`handleMeadeGet`).
//
// Each test exercises a single Meade `:G...` sub-command suffix end-to-end:
// it calls the parser entry point with a stub handler and asserts the exact
// bytes emitted on the wire. The stub records which callback fired so we
// also catch silent regressions where the wrong handler is invoked.

#include <unity.h>

#include <string.h>

#include "core/MeadeParser.hpp"

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
    meade::DecCoordinate currentDec    = {7, 8, 9};
    meade::DecCoordinate targetDec     = {-10, 11, 12};
    bool isSlewing                     = false;
    bool isTracking                    = true;
    bool isGuiding                     = false;
    meade::MeadeLatitude latitude      = {47, 30};
    meade::MeadeLongitude longitude    = {-12, 30};
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
    last = meade::handleMeadeGet(suffix, h);
    return last.c_str();
}

}  // namespace

namespace
{

void test_firmware_version_two_char_command()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("V1.2.3#", dispatch("VN", h));
    TEST_ASSERT_EQUAL_STRING("fw", h.lastCall);
}

void test_product_name_two_char_command()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("OpenAstroTracker#", dispatch("VP", h));
    TEST_ASSERT_EQUAL_STRING("product", h.lastCall);
}

void test_current_ra_formats_hh_mm_ss()
{
    FakeHandlers h;
    h.currentRa = {14, 45, 6};
    TEST_ASSERT_EQUAL_STRING("14:45:06#", dispatch("R", h));
    TEST_ASSERT_EQUAL_STRING("currentRa", h.lastCall);
}

void test_target_ra_formats_hh_mm_ss()
{
    FakeHandlers h;
    h.targetRa = {0, 0, 0};
    TEST_ASSERT_EQUAL_STRING("00:00:00#", dispatch("r", h));
    TEST_ASSERT_EQUAL_STRING("targetRa", h.lastCall);
}

void test_current_dec_signed_dms()
{
    FakeHandlers h;
    h.currentDec = {47, 30, 15};
    TEST_ASSERT_EQUAL_STRING("+47*30'15#", dispatch("D", h));
    TEST_ASSERT_EQUAL_STRING("currentDec", h.lastCall);
}

void test_target_dec_negative()
{
    FakeHandlers h;
    h.targetDec = {-12, 45, 0};
    TEST_ASSERT_EQUAL_STRING("-12*45'00#", dispatch("d", h));
    TEST_ASSERT_EQUAL_STRING("targetDec", h.lastCall);
}

void test_mount_status_passes_through()
{
    FakeHandlers h;
    h.status = "Idle,---,0,0";
    TEST_ASSERT_EQUAL_STRING("Idle,---,0,0#", dispatch("X", h));
    TEST_ASSERT_EQUAL_STRING("status", h.lastCall);
}

void test_is_slewing_emits_zero_one()
{
    FakeHandlers h;
    h.isSlewing = true;
    TEST_ASSERT_EQUAL_STRING("1#", dispatch("IS", h));
    TEST_ASSERT_EQUAL_STRING("slewing", h.lastCall);
    h.isSlewing = false;
    TEST_ASSERT_EQUAL_STRING("0#", dispatch("IS", h));
}

void test_is_tracking_emits_zero_one()
{
    FakeHandlers h;
    h.isTracking = false;
    TEST_ASSERT_EQUAL_STRING("0#", dispatch("IT", h));
    TEST_ASSERT_EQUAL_STRING("tracking", h.lastCall);
}

void test_is_guiding_emits_zero_one()
{
    FakeHandlers h;
    h.isGuiding = true;
    TEST_ASSERT_EQUAL_STRING("1#", dispatch("IG", h));
    TEST_ASSERT_EQUAL_STRING("guiding", h.lastCall);
}

void test_site_latitude_signed_two_digit_deg()
{
    FakeHandlers h;
    h.latitude = {47, 30};
    TEST_ASSERT_EQUAL_STRING("+47*30#", dispatch("t", h));
    h.latitude = {-12, 45};
    TEST_ASSERT_EQUAL_STRING("-12*45#", dispatch("t", h));
}

void test_site_longitude_signed_three_digit_deg()
{
    FakeHandlers h;
    h.longitude = {12, 30};
    TEST_ASSERT_EQUAL_STRING("+012*30#", dispatch("g", h));
    h.longitude = {-122, 45};
    TEST_ASSERT_EQUAL_STRING("-122*45#", dispatch("g", h));
}

void test_utc_offset_signs_and_pads()
{
    FakeHandlers h;
    h.utcOffset = -5;
    TEST_ASSERT_EQUAL_STRING("-05#", dispatch("G", h));
    h.utcOffset = 3;
    TEST_ASSERT_EQUAL_STRING("+03#", dispatch("G", h));
}

void test_local_time_24h_format()
{
    FakeHandlers h;
    h.localTime = {14, 45, 6};
    TEST_ASSERT_EQUAL_STRING("14:45:06#", dispatch("L", h));
    TEST_ASSERT_EQUAL_STRING("time", h.lastCall);
}

void test_local_time_12h_converts_pm()
{
    FakeHandlers h;
    h.localTime = {14, 45, 6};  // 14:xx -> 02:xx in 12h
    TEST_ASSERT_EQUAL_STRING("02:45:06#", dispatch("a", h));
    h.localTime = {0, 30, 0};  // 00 -> 12
    TEST_ASSERT_EQUAL_STRING("12:30:00#", dispatch("a", h));
    h.localTime = {7, 8, 9};  // morning unchanged
    TEST_ASSERT_EQUAL_STRING("07:08:09#", dispatch("a", h));
}

void test_local_date_truncates_year_to_two_digits()
{
    FakeHandlers h;
    h.localDate = {3, 7, 2024};
    TEST_ASSERT_EQUAL_STRING("03/07/24#", dispatch("C", h));
    TEST_ASSERT_EQUAL_STRING("date", h.lastCall);
}

void test_clock_format_24h()
{
    FakeHandlers h;
    h.clockFmt = meade::MeadeClockFormat::Hours24;
    TEST_ASSERT_EQUAL_STRING("24#", dispatch("c", h));
    h.clockFmt = meade::MeadeClockFormat::Hours12;
    TEST_ASSERT_EQUAL_STRING("12#", dispatch("c", h));
}

void test_tracking_rate_sidereal()
{
    FakeHandlers h;
    h.trackRate = meade::MeadeTrackingRate::Sidereal;
    TEST_ASSERT_EQUAL_STRING("60.0#", dispatch("T", h));
}

void test_site_name_slots_invoke_handler_with_index()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("OAT1#", dispatch("M", h));
    TEST_ASSERT_EQUAL_STRING("OAT2#", dispatch("N", h));
    TEST_ASSERT_EQUAL_STRING("OAT3#", dispatch("O", h));
    TEST_ASSERT_EQUAL_STRING("OAT4#", dispatch("P", h));
    TEST_ASSERT_EQUAL_STRING("siteName", h.lastCall);
}

void test_unknown_suffix_returns_empty()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("ZZ", h));
    TEST_ASSERT_EQUAL_STRING("", dispatch("Q", h));
    TEST_ASSERT_EQUAL_STRING("", dispatch("", h));
    TEST_ASSERT_NULL(h.lastCall);
}

}  // namespace

void register_meade_get_tests()
{
    RUN_TEST(test_firmware_version_two_char_command);
    RUN_TEST(test_product_name_two_char_command);
    RUN_TEST(test_current_ra_formats_hh_mm_ss);
    RUN_TEST(test_target_ra_formats_hh_mm_ss);
    RUN_TEST(test_current_dec_signed_dms);
    RUN_TEST(test_target_dec_negative);
    RUN_TEST(test_mount_status_passes_through);
    RUN_TEST(test_is_slewing_emits_zero_one);
    RUN_TEST(test_is_tracking_emits_zero_one);
    RUN_TEST(test_is_guiding_emits_zero_one);
    RUN_TEST(test_site_latitude_signed_two_digit_deg);
    RUN_TEST(test_site_longitude_signed_three_digit_deg);
    RUN_TEST(test_utc_offset_signs_and_pads);
    RUN_TEST(test_local_time_24h_format);
    RUN_TEST(test_local_time_12h_converts_pm);
    RUN_TEST(test_local_date_truncates_year_to_two_digits);
    RUN_TEST(test_clock_format_24h);
    RUN_TEST(test_tracking_rate_sidereal);
    RUN_TEST(test_site_name_slots_invoke_handler_with_index);
    RUN_TEST(test_unknown_suffix_returns_empty);
}
