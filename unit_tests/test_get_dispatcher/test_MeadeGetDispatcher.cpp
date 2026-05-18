// Routing + wire-byte coverage for `dispatchGet`. Verifies that each Get kind
// hits its matching IMeadeGetHandlers callback and that the callback's value
// is funneled into the typed response formatter as expected.

#include <unity.h>

#include <cstring>

#include "core/MeadeParser.hpp"
#include "core/MeadeResponse.hpp"

namespace meade = oat::core::meade;

using meade::dispatchGet;
using meade::IMeadeGetHandlers;
using meade::LocalDate;
using meade::MeadeGetCommandKind;
using meade::MeadePayload;
using meade::MeadeResponse;

namespace
{
// Records which callback fired and returns scripted values.
class FakeHandlers : public IMeadeGetHandlers
{
  public:
    const char *lastCall = nullptr;

    // Scripted return values.
    const char *firmwareVersion = "V1.2.3";
    const char *productName     = "OpenAstroTracker";
    const char *targetRa        = "01:02:03";
    const char *targetDec       = "+10*20:30";
    const char *currentRa       = "04:05:06";
    const char *currentDec      = "-20*30:40";
    const char *mountStatus     = "Idle,---,0,0";
    bool isSlewing              = true;
    bool isTracking             = false;
    bool isGuiding              = true;
    const char *siteLatitude    = "+47*36#";
    const char *siteLongitude   = "-122*19#";
    int utcOffset               = -8;
    const char *localTime12h    = "11:22:33#";
    const char *localTime24h    = "23:22:33#";
    LocalDate localDate         = {3, 14, 2025};

    const char *onFirmwareVersion() override
    {
        lastCall = "FirmwareVersion";
        return firmwareVersion;
    }
    const char *onProductName() override
    {
        lastCall = "ProductName";
        return productName;
    }
    const char *onTargetRa() override
    {
        lastCall = "TargetRa";
        return targetRa;
    }
    const char *onTargetDec() override
    {
        lastCall = "TargetDec";
        return targetDec;
    }
    const char *onCurrentRa() override
    {
        lastCall = "CurrentRa";
        return currentRa;
    }
    const char *onCurrentDec() override
    {
        lastCall = "CurrentDec";
        return currentDec;
    }
    const char *onMountStatus() override
    {
        lastCall = "MountStatus";
        return mountStatus;
    }
    bool onIsSlewing() override
    {
        lastCall = "IsSlewing";
        return isSlewing;
    }
    bool onIsTracking() override
    {
        lastCall = "IsTracking";
        return isTracking;
    }
    bool onIsGuiding() override
    {
        lastCall = "IsGuiding";
        return isGuiding;
    }
    const char *onSiteLatitude() override
    {
        lastCall = "SiteLatitude";
        return siteLatitude;
    }
    const char *onSiteLongitude() override
    {
        lastCall = "SiteLongitude";
        return siteLongitude;
    }
    int onUtcOffset() override
    {
        lastCall = "UtcOffset";
        return utcOffset;
    }
    const char *onLocalTime12h() override
    {
        lastCall = "LocalTime12h";
        return localTime12h;
    }
    const char *onLocalTime24h() override
    {
        lastCall = "LocalTime24h";
        return localTime24h;
    }
    LocalDate onLocalDate() override
    {
        lastCall = "LocalDate";
        return localDate;
    }
};

MeadePayload kEmptyPayload {};
}  // namespace

void setUp(void)
{
}
void tearDown(void)
{
}

// ---- Routing -----------------------------------------------------------

void test_unknown_kind_returns_empty_response()
{
    FakeHandlers h;
    MeadeResponse r = dispatchGet(MeadeGetCommandKind::Unknown, kEmptyPayload, h);
    TEST_ASSERT_TRUE(r.empty());
    TEST_ASSERT_NULL(h.lastCall);
}

void test_firmware_version_routes_and_formats_as_text()
{
    FakeHandlers h;
    MeadeResponse r = dispatchGet(MeadeGetCommandKind::FirmwareVersion, kEmptyPayload, h);
    TEST_ASSERT_EQUAL_STRING("FirmwareVersion", h.lastCall);
    TEST_ASSERT_EQUAL_STRING("V1.2.3#", r.c_str());
}

void test_product_name_routes_and_formats_as_text()
{
    FakeHandlers h;
    MeadeResponse r = dispatchGet(MeadeGetCommandKind::ProductName, kEmptyPayload, h);
    TEST_ASSERT_EQUAL_STRING("ProductName", h.lastCall);
    TEST_ASSERT_EQUAL_STRING("OpenAstroTracker#", r.c_str());
}

void test_is_slewing_routes_and_formats_as_boolean_true()
{
    FakeHandlers h;
    h.isSlewing     = true;
    MeadeResponse r = dispatchGet(MeadeGetCommandKind::IsSlewing, kEmptyPayload, h);
    TEST_ASSERT_EQUAL_STRING("IsSlewing", h.lastCall);
    TEST_ASSERT_EQUAL_STRING("1#", r.c_str());
}

void test_is_tracking_routes_and_formats_as_boolean_false()
{
    FakeHandlers h;
    h.isTracking    = false;
    MeadeResponse r = dispatchGet(MeadeGetCommandKind::IsTracking, kEmptyPayload, h);
    TEST_ASSERT_EQUAL_STRING("IsTracking", h.lastCall);
    TEST_ASSERT_EQUAL_STRING("0#", r.c_str());
}

void test_is_guiding_routes_and_formats_as_boolean_true()
{
    FakeHandlers h;
    h.isGuiding     = true;
    MeadeResponse r = dispatchGet(MeadeGetCommandKind::IsGuiding, kEmptyPayload, h);
    TEST_ASSERT_EQUAL_STRING("IsGuiding", h.lastCall);
    TEST_ASSERT_EQUAL_STRING("1#", r.c_str());
}

void test_current_ra_routes_and_passes_through_preformatted()
{
    FakeHandlers h;
    MeadeResponse r = dispatchGet(MeadeGetCommandKind::CurrentRa, kEmptyPayload, h);
    TEST_ASSERT_EQUAL_STRING("CurrentRa", h.lastCall);
    TEST_ASSERT_EQUAL_STRING("04:05:06#", r.c_str());
}

void test_current_dec_routes_and_passes_through_preformatted()
{
    FakeHandlers h;
    MeadeResponse r = dispatchGet(MeadeGetCommandKind::CurrentDec, kEmptyPayload, h);
    TEST_ASSERT_EQUAL_STRING("CurrentDec", h.lastCall);
    TEST_ASSERT_EQUAL_STRING("-20*30:40#", r.c_str());
}

void test_target_ra_routes_and_passes_through_preformatted()
{
    FakeHandlers h;
    MeadeResponse r = dispatchGet(MeadeGetCommandKind::TargetRa, kEmptyPayload, h);
    TEST_ASSERT_EQUAL_STRING("TargetRa", h.lastCall);
    TEST_ASSERT_EQUAL_STRING("01:02:03#", r.c_str());
}

void test_target_dec_routes_and_passes_through_preformatted()
{
    FakeHandlers h;
    MeadeResponse r = dispatchGet(MeadeGetCommandKind::TargetDec, kEmptyPayload, h);
    TEST_ASSERT_EQUAL_STRING("TargetDec", h.lastCall);
    TEST_ASSERT_EQUAL_STRING("+10*20:30#", r.c_str());
}

void test_mount_status_routes_and_formats_as_text()
{
    FakeHandlers h;
    MeadeResponse r = dispatchGet(MeadeGetCommandKind::MountStatus, kEmptyPayload, h);
    TEST_ASSERT_EQUAL_STRING("MountStatus", h.lastCall);
    TEST_ASSERT_EQUAL_STRING("Idle,---,0,0#", r.c_str());
}

void test_site_latitude_routes_and_passes_through_preformatted()
{
    // The SiteLatitude `preformatted` overload appends `#` unconditionally,
    // and the existing Mount-side formatter already includes `#`. The double
    // terminator is pre-existing behavior preserved by this refactor.
    FakeHandlers h;
    MeadeResponse r = dispatchGet(MeadeGetCommandKind::SiteLatitude, kEmptyPayload, h);
    TEST_ASSERT_EQUAL_STRING("SiteLatitude", h.lastCall);
    TEST_ASSERT_EQUAL_STRING("+47*36##", r.c_str());
}

void test_site_longitude_routes_and_passes_through_preformatted()
{
    // See SiteLatitude note: pre-existing double-terminator preserved.
    FakeHandlers h;
    MeadeResponse r = dispatchGet(MeadeGetCommandKind::SiteLongitude, kEmptyPayload, h);
    TEST_ASSERT_EQUAL_STRING("SiteLongitude", h.lastCall);
    TEST_ASSERT_EQUAL_STRING("-122*19##", r.c_str());
}

void test_utc_offset_routes_and_formats_signed_two_digits()
{
    FakeHandlers h;
    h.utcOffset     = -8;
    MeadeResponse r = dispatchGet(MeadeGetCommandKind::UtcOffset, kEmptyPayload, h);
    TEST_ASSERT_EQUAL_STRING("UtcOffset", h.lastCall);
    TEST_ASSERT_EQUAL_STRING("-08#", r.c_str());
}

void test_local_time_12h_routes_and_passes_through_preformatted()
{
    // See SiteLatitude note: pre-existing double-terminator preserved.
    FakeHandlers h;
    MeadeResponse r = dispatchGet(MeadeGetCommandKind::LocalTime12h, kEmptyPayload, h);
    TEST_ASSERT_EQUAL_STRING("LocalTime12h", h.lastCall);
    TEST_ASSERT_EQUAL_STRING("11:22:33##", r.c_str());
}

void test_local_time_24h_routes_and_passes_through_preformatted()
{
    // See SiteLatitude note: pre-existing double-terminator preserved.
    FakeHandlers h;
    MeadeResponse r = dispatchGet(MeadeGetCommandKind::LocalTime24h, kEmptyPayload, h);
    TEST_ASSERT_EQUAL_STRING("LocalTime24h", h.lastCall);
    TEST_ASSERT_EQUAL_STRING("23:22:33##", r.c_str());
}

void test_local_date_unpacks_struct_to_month_day_year_args()
{
    FakeHandlers h;
    h.localDate     = {3, 14, 2025};
    MeadeResponse r = dispatchGet(MeadeGetCommandKind::LocalDate, kEmptyPayload, h);
    TEST_ASSERT_EQUAL_STRING("LocalDate", h.lastCall);
    TEST_ASSERT_EQUAL_STRING("03/14/25#", r.c_str());
}

// ---- Constant-shaped kinds: never call a handler ------------------------

void test_clock_format_emits_constant_without_callback()
{
    FakeHandlers h;
    MeadeResponse r = dispatchGet(MeadeGetCommandKind::ClockFormat, kEmptyPayload, h);
    TEST_ASSERT_NULL(h.lastCall);
    TEST_ASSERT_EQUAL_STRING("24#", r.c_str());
}

void test_tracking_rate_emits_constant_without_callback()
{
    FakeHandlers h;
    MeadeResponse r = dispatchGet(MeadeGetCommandKind::TrackingRate, kEmptyPayload, h);
    TEST_ASSERT_NULL(h.lastCall);
    TEST_ASSERT_EQUAL_STRING("60.0#", r.c_str());
}

void test_site_name_slot_1_uses_fixed_arg_without_callback()
{
    FakeHandlers h;
    MeadeResponse r = dispatchGet(MeadeGetCommandKind::SiteName1, kEmptyPayload, h);
    TEST_ASSERT_NULL(h.lastCall);
    TEST_ASSERT_EQUAL_STRING("OAT1#", r.c_str());
}

void test_site_name_slot_2_uses_fixed_arg_without_callback()
{
    FakeHandlers h;
    MeadeResponse r = dispatchGet(MeadeGetCommandKind::SiteName2, kEmptyPayload, h);
    TEST_ASSERT_NULL(h.lastCall);
    TEST_ASSERT_EQUAL_STRING("OAT2#", r.c_str());
}

void test_site_name_slot_3_uses_fixed_arg_without_callback()
{
    FakeHandlers h;
    MeadeResponse r = dispatchGet(MeadeGetCommandKind::SiteName3, kEmptyPayload, h);
    TEST_ASSERT_NULL(h.lastCall);
    TEST_ASSERT_EQUAL_STRING("OAT3#", r.c_str());
}

void test_site_name_slot_4_uses_fixed_arg_without_callback()
{
    FakeHandlers h;
    MeadeResponse r = dispatchGet(MeadeGetCommandKind::SiteName4, kEmptyPayload, h);
    TEST_ASSERT_NULL(h.lastCall);
    TEST_ASSERT_EQUAL_STRING("OAT4#", r.c_str());
}

void process()
{
    UNITY_BEGIN();
    RUN_TEST(test_unknown_kind_returns_empty_response);
    RUN_TEST(test_firmware_version_routes_and_formats_as_text);
    RUN_TEST(test_product_name_routes_and_formats_as_text);
    RUN_TEST(test_is_slewing_routes_and_formats_as_boolean_true);
    RUN_TEST(test_is_tracking_routes_and_formats_as_boolean_false);
    RUN_TEST(test_is_guiding_routes_and_formats_as_boolean_true);
    RUN_TEST(test_current_ra_routes_and_passes_through_preformatted);
    RUN_TEST(test_current_dec_routes_and_passes_through_preformatted);
    RUN_TEST(test_target_ra_routes_and_passes_through_preformatted);
    RUN_TEST(test_target_dec_routes_and_passes_through_preformatted);
    RUN_TEST(test_mount_status_routes_and_formats_as_text);
    RUN_TEST(test_site_latitude_routes_and_passes_through_preformatted);
    RUN_TEST(test_site_longitude_routes_and_passes_through_preformatted);
    RUN_TEST(test_utc_offset_routes_and_formats_signed_two_digits);
    RUN_TEST(test_local_time_12h_routes_and_passes_through_preformatted);
    RUN_TEST(test_local_time_24h_routes_and_passes_through_preformatted);
    RUN_TEST(test_local_date_unpacks_struct_to_month_day_year_args);
    RUN_TEST(test_clock_format_emits_constant_without_callback);
    RUN_TEST(test_tracking_rate_emits_constant_without_callback);
    RUN_TEST(test_site_name_slot_1_uses_fixed_arg_without_callback);
    RUN_TEST(test_site_name_slot_2_uses_fixed_arg_without_callback);
    RUN_TEST(test_site_name_slot_3_uses_fixed_arg_without_callback);
    RUN_TEST(test_site_name_slot_4_uses_fixed_arg_without_callback);
    UNITY_END();
}

#if defined(ARDUINO)
    #include <Arduino.h>
void setup()
{
    delay(2000);
    process();
}

void loop()
{
}
#else
int main()
{
    process();
    return 0;
}
#endif
