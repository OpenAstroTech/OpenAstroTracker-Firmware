// Golden wire-byte tests for the Meade response API. Each shape and each
// kind->shape binding gets at least one assertion against the exact bytes
// the firmware will put on the wire.

#include <unity.h>

#include "core/MeadeResponse.hpp"

namespace meade = oat::core::meade;

using meade::MeadeGetCommandKind;
using meade::MeadeResponse;
using meade::MeadeSetCommandKind;

namespace tag = meade::response::tag;
using meade::response::makeResponse;
using meade::response::respond;
using meade::response::respond;

void setUp(void)
{
}
void tearDown(void)
{
}

// ---- Direct shape (tag) tests ------------------------------------------

void test_empty_response_is_empty_string()
{
    MeadeResponse r = makeResponse(tag::Empty {});
    TEST_ASSERT_TRUE(r.empty());
    TEST_ASSERT_EQUAL_STRING("", r.c_str());
}

void test_literal_passes_through_verbatim()
{
    MeadeResponse r = makeResponse(tag::Literal {}, "OAT1#");
    TEST_ASSERT_EQUAL_STRING("OAT1#", r.c_str());
    TEST_ASSERT_EQUAL_UINT(5, r.length());
}

void test_text_appends_terminator()
{
    MeadeResponse r = makeResponse(tag::Text {}, "V1.2.3");
    TEST_ASSERT_EQUAL_STRING("V1.2.3#", r.c_str());
}

void test_boolean_emits_zero_or_one()
{
    TEST_ASSERT_EQUAL_STRING("1#", makeResponse(tag::Boolean {}, true).c_str());
    TEST_ASSERT_EQUAL_STRING("0#", makeResponse(tag::Boolean {}, false).c_str());
}

void test_set_success_emits_zero_or_one_without_hash()
{
    TEST_ASSERT_EQUAL_STRING("1", makeResponse(tag::SetSuccess {}, true).c_str());
    TEST_ASSERT_EQUAL_STRING("0", makeResponse(tag::SetSuccess {}, false).c_str());
}

void test_numeric_float_honors_precision()
{
    TEST_ASSERT_EQUAL_STRING("12.30#", makeResponse(tag::NumericFloat {}, 12.3f, 2).c_str());
    TEST_ASSERT_EQUAL_STRING("12#", makeResponse(tag::NumericFloat {}, 12.0f, 0).c_str());
}

void test_clock_format_is_24_hash()
{
    TEST_ASSERT_EQUAL_STRING("24#", makeResponse(tag::ClockFormat24 {}).c_str());
}

void test_tracking_rate_is_60_dot_0_hash()
{
    TEST_ASSERT_EQUAL_STRING("60.0#", makeResponse(tag::TrackingRate {}).c_str());
}

void test_utc_offset_signs_and_pads()
{
    TEST_ASSERT_EQUAL_STRING("+05#", makeResponse(tag::UtcOffset {}, 5).c_str());
    TEST_ASSERT_EQUAL_STRING("-08#", makeResponse(tag::UtcOffset {}, -8).c_str());
    TEST_ASSERT_EQUAL_STRING("+00#", makeResponse(tag::UtcOffset {}, 0).c_str());
}

void test_local_date_pads_and_truncates_year()
{
    TEST_ASSERT_EQUAL_STRING("03/07/24#", makeResponse(tag::LocalDate {}, 3, 7, 2024).c_str());
    TEST_ASSERT_EQUAL_STRING("12/31/99#", makeResponse(tag::LocalDate {}, 12, 31, 1999).c_str());
}

void test_site_name_slot_includes_slot_number()
{
    TEST_ASSERT_EQUAL_STRING("OAT1#", makeResponse(tag::SiteNameSlot {}, 1).c_str());
    TEST_ASSERT_EQUAL_STRING("OAT4#", makeResponse(tag::SiteNameSlot {}, 4).c_str());
}

void test_ra_coordinate_is_hh_mm_ss()
{
    TEST_ASSERT_EQUAL_STRING("14:45:06#", makeResponse(tag::RaCoordinate {}, 14, 45, 6).c_str());
    TEST_ASSERT_EQUAL_STRING("00:00:00#", makeResponse(tag::RaCoordinate {}, 0, 0, 0).c_str());
}

void test_dec_coordinate_is_signed_dms()
{
    TEST_ASSERT_EQUAL_STRING("+47*30'15#", makeResponse(tag::DecCoordinate {}, '+', 47, 30, 15).c_str());
    TEST_ASSERT_EQUAL_STRING("-12*45'00#", makeResponse(tag::DecCoordinate {}, '-', 12, 45, 0).c_str());
}

void test_site_latitude_signed_two_digit_degrees()
{
    TEST_ASSERT_EQUAL_STRING("+47*30#", makeResponse(tag::SiteLatitude {}, '+', 47, 30).c_str());
    TEST_ASSERT_EQUAL_STRING("-12*45#", makeResponse(tag::SiteLatitude {}, '-', 12, 45).c_str());
}

void test_site_longitude_signed_three_digit_degrees()
{
    TEST_ASSERT_EQUAL_STRING("+012*30#", makeResponse(tag::SiteLongitude {}, '+', 12, 30).c_str());
    TEST_ASSERT_EQUAL_STRING("-122*45#", makeResponse(tag::SiteLongitude {}, '-', 122, 45).c_str());
}

void test_local_time_is_hh_mm_ss()
{
    TEST_ASSERT_EQUAL_STRING("14:45:06#", makeResponse(tag::LocalTime {}, 14, 45, 6).c_str());
}

void test_dec_limits_pair_uses_pipe_separator()
{
    TEST_ASSERT_EQUAL_STRING("-30.5|45.2#", makeResponse(tag::DecLimitsPair {}, -30.5f, 45.2f).c_str());
}

void test_angle_pair_uses_comma_separator()
{
    TEST_ASSERT_EQUAL_STRING("1.25,-0.50#", makeResponse(tag::AnglePair {}, 1.25f, -0.5f).c_str());
}

void test_hemisphere_emits_n_or_s()
{
    TEST_ASSERT_EQUAL_STRING("N#", makeResponse(tag::Hemisphere {}, true).c_str());
    TEST_ASSERT_EQUAL_STRING("S#", makeResponse(tag::Hemisphere {}, false).c_str());
}

void test_set_local_date_ack_wire_bytes()
{
    MeadeResponse r = makeResponse(tag::SetLocalDateAck {}, true);
    TEST_ASSERT_EQUAL_STRING("1Updating Planetary Data#                              #", r.c_str());
    TEST_ASSERT_EQUAL_STRING("0", makeResponse(tag::SetLocalDateAck {}, false).c_str());
}

void test_level_unknown_echoes_command_letter()
{
    TEST_ASSERT_EQUAL_STRING("Unknown Level command: XB", makeResponse(tag::LevelUnknown {}, "B").c_str());
}

// ---- Kind -> tag binding tests -----------------------------------------

void test_get_firmware_version_binds_to_text()
{
    MeadeResponse r = respond<MeadeGetCommandKind::FirmwareVersion>("V1.2.3");
    TEST_ASSERT_EQUAL_STRING("V1.2.3#", r.c_str());
}

void test_get_product_name_binds_to_text()
{
    MeadeResponse r = respond<MeadeGetCommandKind::ProductName>("OpenAstroTracker");
    TEST_ASSERT_EQUAL_STRING("OpenAstroTracker#", r.c_str());
}

void test_get_is_slewing_binds_to_boolean()
{
    TEST_ASSERT_EQUAL_STRING("1#", respond<MeadeGetCommandKind::IsSlewing>(true).c_str());
    TEST_ASSERT_EQUAL_STRING("0#", respond<MeadeGetCommandKind::IsTracking>(false).c_str());
    TEST_ASSERT_EQUAL_STRING("1#", respond<MeadeGetCommandKind::IsGuiding>(true).c_str());
}

void test_get_clock_format_takes_no_args()
{
    TEST_ASSERT_EQUAL_STRING("24#", respond<MeadeGetCommandKind::ClockFormat>().c_str());
}

void test_get_tracking_rate_takes_no_args()
{
    TEST_ASSERT_EQUAL_STRING("60.0#", respond<MeadeGetCommandKind::TrackingRate>().c_str());
}

void test_get_site_name_slots_carry_fixed_arg()
{
    // The slot number is fixed at the trait layer; the caller passes no args.
    TEST_ASSERT_EQUAL_STRING("OAT1#", respond<MeadeGetCommandKind::SiteName1>().c_str());
    TEST_ASSERT_EQUAL_STRING("OAT2#", respond<MeadeGetCommandKind::SiteName2>().c_str());
    TEST_ASSERT_EQUAL_STRING("OAT3#", respond<MeadeGetCommandKind::SiteName3>().c_str());
    TEST_ASSERT_EQUAL_STRING("OAT4#", respond<MeadeGetCommandKind::SiteName4>().c_str());
}

void test_get_current_ra_binds_to_ra_coordinate()
{
    TEST_ASSERT_EQUAL_STRING("14:45:06#", respond<MeadeGetCommandKind::CurrentRa>(14, 45, 6).c_str());
}

void test_get_current_dec_binds_to_dec_coordinate()
{
    TEST_ASSERT_EQUAL_STRING("+47*30'15#", respond<MeadeGetCommandKind::CurrentDec>('+', 47, 30, 15).c_str());
}

void test_get_utc_offset_binds()
{
    TEST_ASSERT_EQUAL_STRING("-05#", respond<MeadeGetCommandKind::UtcOffset>(-5).c_str());
}

void test_get_local_date_binds()
{
    TEST_ASSERT_EQUAL_STRING("03/07/24#", respond<MeadeGetCommandKind::LocalDate>(3, 7, 2024).c_str());
}

void test_set_target_ra_binds_to_set_success()
{
    TEST_ASSERT_EQUAL_STRING("1", respond<MeadeSetCommandKind::TargetRa>(true).c_str());
    TEST_ASSERT_EQUAL_STRING("0", respond<MeadeSetCommandKind::TargetDec>(false).c_str());
}

void test_set_local_date_uses_dedicated_ack()
{
    MeadeResponse r = respond<MeadeSetCommandKind::LocalDate>(true);
    TEST_ASSERT_EQUAL_STRING("1Updating Planetary Data#                              #", r.c_str());
}

// ---- Behavioural tests --------------------------------------------------

void test_meade_response_is_implicitly_convertible_to_c_string()
{
    // Drop-in compatibility: code that returns `const char *` can return a
    // `MeadeResponse` directly via implicit conversion.
    MeadeResponse r        = makeResponse(tag::Text {}, "hi");
    const char *underlying = r;
    TEST_ASSERT_EQUAL_STRING("hi#", underlying);
}

void test_truncates_at_capacity_minus_one_for_nul()
{
    // Build a very long source to ensure clamping. We rely on `Literal`'s
    // behaviour: anything past Capacity-1 is dropped.
    char src[MeadeResponse::Capacity + 50];
    for (std::size_t i = 0; i < sizeof(src) - 1; ++i)
    {
        src[i] = 'a';
    }
    src[sizeof(src) - 1] = '\0';
    MeadeResponse r      = makeResponse(tag::Literal {}, src);
    TEST_ASSERT_EQUAL_UINT(MeadeResponse::Capacity - 1, r.length());
    TEST_ASSERT_EQUAL('\0', r.c_str()[MeadeResponse::Capacity - 1]);
}

// ---- Tests for shapes added with the Extra/Level family migration ------

void test_int_formats_decimal()
{
    TEST_ASSERT_EQUAL_STRING("42#", makeResponse(tag::Int {}, 42).c_str());
    TEST_ASSERT_EQUAL_STRING("-7#", makeResponse(tag::Int {}, -7).c_str());
    TEST_ASSERT_EQUAL_STRING("0#", makeResponse(tag::Int {}, 0).c_str());
}

void test_long_formats_signed()
{
    TEST_ASSERT_EQUAL_STRING("123456#", makeResponse(tag::Long {}, 123456L).c_str());
    TEST_ASSERT_EQUAL_STRING("-987654#", makeResponse(tag::Long {}, -987654L).c_str());
}

void test_long_pair_pipe_uses_pipe_separator()
{
    MeadeResponse r = makeResponse(tag::LongPairPipe {}, 100L, 200L);
    TEST_ASSERT_EQUAL_STRING("100|200#", r.c_str());
}

void test_compact_hms_zero_pads()
{
    MeadeResponse r = makeResponse(tag::CompactHms {}, 5, 7, 9);
    TEST_ASSERT_EQUAL_STRING("050709#", r.c_str());
}

void test_compact_hms_handles_two_digit()
{
    MeadeResponse r = makeResponse(tag::CompactHms {}, 23, 59, 58);
    TEST_ASSERT_EQUAL_STRING("235958#", r.c_str());
}

void test_angle_pair4_uses_four_decimal_precision()
{
    MeadeResponse r = makeResponse(tag::AnglePair4 {}, 1.23456f, -2.71828f);
    TEST_ASSERT_EQUAL_STRING("1.2346,-2.7183#", r.c_str());
}

void process()
{
    UNITY_BEGIN();
    RUN_TEST(test_empty_response_is_empty_string);
    RUN_TEST(test_literal_passes_through_verbatim);
    RUN_TEST(test_text_appends_terminator);
    RUN_TEST(test_boolean_emits_zero_or_one);
    RUN_TEST(test_set_success_emits_zero_or_one_without_hash);
    RUN_TEST(test_numeric_float_honors_precision);
    RUN_TEST(test_clock_format_is_24_hash);
    RUN_TEST(test_tracking_rate_is_60_dot_0_hash);
    RUN_TEST(test_utc_offset_signs_and_pads);
    RUN_TEST(test_local_date_pads_and_truncates_year);
    RUN_TEST(test_site_name_slot_includes_slot_number);
    RUN_TEST(test_ra_coordinate_is_hh_mm_ss);
    RUN_TEST(test_dec_coordinate_is_signed_dms);
    RUN_TEST(test_site_latitude_signed_two_digit_degrees);
    RUN_TEST(test_site_longitude_signed_three_digit_degrees);
    RUN_TEST(test_local_time_is_hh_mm_ss);
    RUN_TEST(test_dec_limits_pair_uses_pipe_separator);
    RUN_TEST(test_angle_pair_uses_comma_separator);
    RUN_TEST(test_hemisphere_emits_n_or_s);
    RUN_TEST(test_set_local_date_ack_wire_bytes);
    RUN_TEST(test_level_unknown_echoes_command_letter);

    RUN_TEST(test_int_formats_decimal);
    RUN_TEST(test_long_formats_signed);
    RUN_TEST(test_long_pair_pipe_uses_pipe_separator);
    RUN_TEST(test_compact_hms_zero_pads);
    RUN_TEST(test_compact_hms_handles_two_digit);
    RUN_TEST(test_angle_pair4_uses_four_decimal_precision);

    RUN_TEST(test_get_firmware_version_binds_to_text);
    RUN_TEST(test_get_product_name_binds_to_text);
    RUN_TEST(test_get_is_slewing_binds_to_boolean);
    RUN_TEST(test_get_clock_format_takes_no_args);
    RUN_TEST(test_get_tracking_rate_takes_no_args);
    RUN_TEST(test_get_site_name_slots_carry_fixed_arg);
    RUN_TEST(test_get_current_ra_binds_to_ra_coordinate);
    RUN_TEST(test_get_current_dec_binds_to_dec_coordinate);
    RUN_TEST(test_get_utc_offset_binds);
    RUN_TEST(test_get_local_date_binds);
    RUN_TEST(test_set_target_ra_binds_to_set_success);
    RUN_TEST(test_set_local_date_uses_dedicated_ack);

    RUN_TEST(test_meade_response_is_implicitly_convertible_to_c_string);
    RUN_TEST(test_truncates_at_capacity_minus_one_for_nul);
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
