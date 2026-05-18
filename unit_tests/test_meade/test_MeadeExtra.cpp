// Wire-byte tests for the Meade Extra dispatcher (`handleMeadeExtra`).
//
// Covers representative `:X...` sub-commands across the five family branches:
//   - FactoryReset    (`:XFR`)             → "1#"
//   - DriftAlignment  (`:XD<duration>`)    → "" (no-op on the dispatcher level)
//   - Get-leaves      (`:XG...`)           → diverse tag shapes
//   - Set-leaves      (`:XS...`)           → "" with side-effects on handler
//   - Level-leaves    (`:XL...`)           → SetSuccess / pair / fallback
//
// Compile-time hardware guards live in the overrides; the dispatcher itself
// is hardware-agnostic and exercised directly here via a fake handler.

#include <unity.h>

#include "core/MeadeParser.hpp"

namespace meade = oat::core::meade;

namespace
{

class FakeExtra : public meade::IMeadeExtraHandlers
{
  public:
    // Captured calls
    bool factoryResetCalled = false;
    bool driftCalled        = false;
    int driftDuration       = 0;
    bool getDecLimitsCalled = false;
    bool levelAvailable     = true;
    bool startupCalled      = false;
    bool shutdownCalled     = false;

    // Returned values
    float raSpd  = 100.5f;
    float decSpd = 200.25f;
    float altSpd = 12.5f;
    float azSpd  = 13.5f;
    meade::ExtraDecLimits decLimits {-30.5f, 45.5f};
    float speedCalibration     = 1.00125f;
    float remainingSafeTime    = 3.5f;
    float trackingSpeed        = 0.0041667f;
    int backlash               = 7;
    const char *autoHomeStates = "RA:Idle,DEC:Idle";
    meade::ExtraAzAltPositions azalt {123L, 456L};
    meade::ExtraStepperCoords targetCoords {1000L, 2000L};
    const char *stepperInfo  = "RA:TMC,DEC:TMC";
    const char *hardwareInfo = "MEGA,RAMPS";
    const char *logBuffer    = "abc";
    long raHomeOff           = -10L;
    long decHomeOff          = 20L;
    bool northern            = true;
    meade::ExtraHms hourAngle {3, 4, 5};
    meade::ExtraHms lst {6, 7, 8};
    const char *networkStatus = "WL,OAT,192.168.1.10";
    meade::ExtraPitchRoll refAngles {1.1f, 2.2f};
    meade::ExtraPitchRoll curAngles {3.3f, 4.4f};
    float gyroTemperature = 22.5f;

    // Captured Set arguments
    float setRaSpd             = 0.0f;
    float setDecSpd            = 0.0f;
    bool decLimitLowerPayload  = false;
    float decLimitLowerVal     = 0.0f;
    bool clearLowerCalled      = false;
    bool clearUpperCalled      = false;
    long setTrackingStepperPos = 0;
    bool setManualSlew         = false;
    int setBacklash            = 0;
    long setRaHomingOffset     = 0L;

    void onFactoryReset() override
    {
        factoryResetCalled = true;
    }
    void onDriftAlignment(int duration) override
    {
        driftCalled   = true;
        driftDuration = duration;
    }
    float onGetRaStepsPerDegree() override
    {
        return raSpd;
    }
    float onGetDecStepsPerDegree() override
    {
        return decSpd;
    }
    float onGetAltStepsPerDegree() override
    {
        return altSpd;
    }
    float onGetAzStepsPerDegree() override
    {
        return azSpd;
    }
    meade::ExtraDecLimits onGetDecLimits() override
    {
        getDecLimitsCalled = true;
        return decLimits;
    }
    float onGetTrackingSpeedCalibration() override
    {
        return speedCalibration;
    }
    float onGetRemainingSafeTime() override
    {
        return remainingSafeTime;
    }
    float onGetTrackingSpeed() override
    {
        return trackingSpeed;
    }
    int onGetBacklashSteps() override
    {
        return backlash;
    }
    const char *onGetAutoHomingStates() override
    {
        return autoHomeStates;
    }
    meade::ExtraAzAltPositions onGetAzAltPositions() override
    {
        return azalt;
    }
    meade::ExtraStepperCoords onGetTargetCoordinatePositions(float, float) override
    {
        return targetCoords;
    }
    const char *onGetStepperInfo() override
    {
        return stepperInfo;
    }
    const char *onGetMountHardwareInfo() override
    {
        return hardwareInfo;
    }
    const char *onGetLogBuffer() override
    {
        return logBuffer;
    }
    long onGetRaHomingOffset() override
    {
        return raHomeOff;
    }
    long onGetDecHomingOffset() override
    {
        return decHomeOff;
    }
    bool onGetHemisphere() override
    {
        return northern;
    }
    meade::ExtraHms onGetHourAngle() override
    {
        return hourAngle;
    }
    meade::ExtraHms onGetLocalSiderealTime() override
    {
        return lst;
    }
    const char *onGetNetworkStatus() override
    {
        return networkStatus;
    }

    void onSetRaStepsPerDegree(float v) override
    {
        setRaSpd = v;
    }
    void onSetDecStepsPerDegree(float v) override
    {
        setDecSpd = v;
    }
    void onSetAzStepsPerDegree(float) override
    {
    }
    void onSetAltStepsPerDegree(float) override
    {
    }
    void onSetDecLimitLower(bool havePayload, float value) override
    {
        decLimitLowerPayload = havePayload;
        decLimitLowerVal     = value;
    }
    void onSetDecLimitUpper(bool, float) override
    {
    }
    void onClearDecLimitLower() override
    {
        clearLowerCalled = true;
    }
    void onClearDecLimitUpper() override
    {
        clearUpperCalled = true;
    }
    void onSetTrackingSpeedCalibration(float) override
    {
    }
    void onSetTrackingStepperPosition(long v) override
    {
        setTrackingStepperPos = v;
    }
    void onSetManualSlewMode(bool enable) override
    {
        setManualSlew = enable;
    }
    void onSetRaManualSpeed(float) override
    {
    }
    void onSetDecManualSpeed(float) override
    {
    }
    void onSetBacklashCorrection(int v) override
    {
        setBacklash = v;
    }
    void onSetRaHomingOffset(long v) override
    {
        setRaHomingOffset = v;
    }
    void onSetDecHomingOffset(long) override
    {
    }

    bool onLevelIsAvailable() override
    {
        return levelAvailable;
    }
    meade::ExtraPitchRoll onLevelGetReferenceAngles() override
    {
        return refAngles;
    }
    meade::ExtraPitchRoll onLevelGetCurrentAngles() override
    {
        return curAngles;
    }
    float onLevelGetTemperature() override
    {
        return gyroTemperature;
    }
    void onLevelSetReferencePitch(float) override
    {
    }
    void onLevelSetReferenceRoll(float) override
    {
    }
    void onLevelStartup() override
    {
        startupCalled = true;
    }
    void onLevelShutdown() override
    {
        shutdownCalled = true;
    }
};

const char *dispatch(const char *suffix, FakeExtra &h)
{
    static meade::MeadeResponse last;
    last = meade::handleMeadeExtra(suffix, h);
    return last.c_str();
}

}  // namespace

namespace
{

// ---- Family-level -----------------------------------------------------------

void test_extra_factory_reset_emits_one_terminator_and_calls_handler()
{
    FakeExtra h;
    TEST_ASSERT_EQUAL_STRING("1#", dispatch("FR", h));
    TEST_ASSERT_TRUE(h.factoryResetCalled);
}

void test_extra_drift_alignment_emits_empty_and_passes_duration_minus_three()
{
    FakeExtra h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("D5", h));
    TEST_ASSERT_TRUE(h.driftCalled);
    TEST_ASSERT_EQUAL_INT(2, h.driftDuration);
}

void test_extra_unknown_family_emits_empty()
{
    FakeExtra h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("Z", h));
}

void test_extra_empty_suffix_emits_empty()
{
    FakeExtra h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("", h));
}

// ---- Get leaves -------------------------------------------------------------

void test_extra_get_ra_steps_per_degree_numeric_float_one_decimal()
{
    FakeExtra h;
    TEST_ASSERT_EQUAL_STRING("100.5#", dispatch("GR", h));
}

void test_extra_get_dec_limit_both_pipe_pair()
{
    FakeExtra h;
    TEST_ASSERT_EQUAL_STRING("-30.5|45.5#", dispatch("GDL", h));
    TEST_ASSERT_TRUE(h.getDecLimitsCalled);
}

void test_extra_get_dec_limit_lower_only_uses_lo_value()
{
    FakeExtra h;
    TEST_ASSERT_EQUAL_STRING("-30.5#", dispatch("GDLL", h));
}

void test_extra_get_dec_limit_upper_only_uses_hi_value()
{
    FakeExtra h;
    TEST_ASSERT_EQUAL_STRING("45.5#", dispatch("GDLU", h));
}

void test_extra_get_dec_limit_invalid_variant_fixed_false()
{
    FakeExtra h;
    TEST_ASSERT_EQUAL_STRING("0#", dispatch("GDLQ", h));
}

void test_extra_get_dec_parking_fixed_false()
{
    FakeExtra h;
    TEST_ASSERT_EQUAL_STRING("0#", dispatch("GDP", h));
}

void test_extra_get_backlash_steps_int()
{
    FakeExtra h;
    TEST_ASSERT_EQUAL_STRING("7#", dispatch("GB", h));
}

void test_extra_get_az_alt_positions_long_pair_pipe()
{
    FakeExtra h;
    TEST_ASSERT_EQUAL_STRING("123|456#", dispatch("GAA", h));
}

void test_extra_get_target_coordinate_positions_parses_payload()
{
    FakeExtra h;
    TEST_ASSERT_EQUAL_STRING("1000|2000#", dispatch("GC10.5*-20.0", h));
}

void test_extra_get_target_coordinate_positions_malformed_emits_empty()
{
    FakeExtra h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("GC", h));
}

void test_extra_get_auto_homing_states_text()
{
    FakeExtra h;
    TEST_ASSERT_EQUAL_STRING("RA:Idle,DEC:Idle#", dispatch("GAH", h));
}

void test_extra_get_log_buffer_literal_no_terminator()
{
    FakeExtra h;
    TEST_ASSERT_EQUAL_STRING("abc", dispatch("GO", h));
}

void test_extra_get_ra_homing_offset_long()
{
    FakeExtra h;
    TEST_ASSERT_EQUAL_STRING("-10#", dispatch("GHR", h));
}

void test_extra_get_hemisphere_returns_hemisphere_format()
{
    FakeExtra h;
    h.northern = true;
    TEST_ASSERT_EQUAL_STRING("N#", dispatch("GHS", h));
}

void test_extra_get_hour_angle_compact_hms()
{
    FakeExtra h;
    TEST_ASSERT_EQUAL_STRING("030405#", dispatch("GH", h));
}

void test_extra_get_local_sidereal_time_compact_hms()
{
    FakeExtra h;
    TEST_ASSERT_EQUAL_STRING("060708#", dispatch("GL", h));
}

void test_extra_get_network_status_text()
{
    FakeExtra h;
    TEST_ASSERT_EQUAL_STRING("WL,OAT,192.168.1.10#", dispatch("GN", h));
}

// ---- Set leaves -------------------------------------------------------------

void test_extra_set_ra_steps_per_degree_emits_empty_and_captures_value()
{
    FakeExtra h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("SR123.5", h));
    TEST_ASSERT_EQUAL_FLOAT(123.5f, h.setRaSpd);
}

void test_extra_set_dec_steps_per_degree_zero_value_skipped()
{
    FakeExtra h;
    h.setDecSpd = -1.0f;
    TEST_ASSERT_EQUAL_STRING("", dispatch("SD0", h));
    // Legacy: only positive values are committed; sentinel stays unchanged.
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, h.setDecSpd);
}

void test_extra_set_dec_limit_lower_with_payload()
{
    FakeExtra h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("SDLL-25.5", h));
    TEST_ASSERT_TRUE(h.decLimitLowerPayload);
    TEST_ASSERT_EQUAL_FLOAT(-25.5f, h.decLimitLowerVal);
}

void test_extra_set_dec_limit_lower_without_payload_uses_current_position()
{
    FakeExtra h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("SDLL", h));
    TEST_ASSERT_FALSE(h.decLimitLowerPayload);
}

void test_extra_set_dec_limit_lower_clear_invokes_clear()
{
    FakeExtra h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("SDLl", h));
    TEST_ASSERT_TRUE(h.clearLowerCalled);
}

void test_extra_set_manual_slew_mode_one_enables()
{
    FakeExtra h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("SM1", h));
    TEST_ASSERT_TRUE(h.setManualSlew);
}

void test_extra_set_manual_slew_mode_zero_disables()
{
    FakeExtra h;
    h.setManualSlew = true;
    TEST_ASSERT_EQUAL_STRING("", dispatch("SM0", h));
    TEST_ASSERT_FALSE(h.setManualSlew);
}

void test_extra_set_backlash_correction_captures_int()
{
    FakeExtra h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("SB42", h));
    TEST_ASSERT_EQUAL_INT(42, h.setBacklash);
}

void test_extra_set_dec_parking_is_no_op_but_emits_empty()
{
    FakeExtra h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("SDP", h));
}

// ---- Level leaves -----------------------------------------------------------

void test_extra_level_unavailable_returns_zero_terminator()
{
    FakeExtra h;
    h.levelAvailable = false;
    TEST_ASSERT_EQUAL_STRING("0#", dispatch("LGC", h));
    TEST_ASSERT_FALSE(h.startupCalled);
}

void test_extra_level_startup_returns_one_terminator_when_available()
{
    FakeExtra h;
    h.levelAvailable = true;
    TEST_ASSERT_EQUAL_STRING("1#", dispatch("L1", h));
    TEST_ASSERT_TRUE(h.startupCalled);
}

void test_extra_level_shutdown_returns_one_terminator_when_available()
{
    FakeExtra h;
    h.levelAvailable = true;
    TEST_ASSERT_EQUAL_STRING("1#", dispatch("L0", h));
    TEST_ASSERT_TRUE(h.shutdownCalled);
}

void test_extra_level_get_temperature_numeric_float_when_available()
{
    FakeExtra h;
    h.levelAvailable  = true;
    h.gyroTemperature = 23.5f;
    TEST_ASSERT_EQUAL_STRING("23.5#", dispatch("LGT", h));
}

}  // namespace

void register_meade_extra_tests()
{
    RUN_TEST(test_extra_factory_reset_emits_one_terminator_and_calls_handler);
    RUN_TEST(test_extra_drift_alignment_emits_empty_and_passes_duration_minus_three);
    RUN_TEST(test_extra_unknown_family_emits_empty);
    RUN_TEST(test_extra_empty_suffix_emits_empty);
    RUN_TEST(test_extra_get_ra_steps_per_degree_numeric_float_one_decimal);
    RUN_TEST(test_extra_get_dec_limit_both_pipe_pair);
    RUN_TEST(test_extra_get_dec_limit_lower_only_uses_lo_value);
    RUN_TEST(test_extra_get_dec_limit_upper_only_uses_hi_value);
    RUN_TEST(test_extra_get_dec_limit_invalid_variant_fixed_false);
    RUN_TEST(test_extra_get_dec_parking_fixed_false);
    RUN_TEST(test_extra_get_backlash_steps_int);
    RUN_TEST(test_extra_get_az_alt_positions_long_pair_pipe);
    RUN_TEST(test_extra_get_target_coordinate_positions_parses_payload);
    RUN_TEST(test_extra_get_target_coordinate_positions_malformed_emits_empty);
    RUN_TEST(test_extra_get_auto_homing_states_text);
    RUN_TEST(test_extra_get_log_buffer_literal_no_terminator);
    RUN_TEST(test_extra_get_ra_homing_offset_long);
    RUN_TEST(test_extra_get_hemisphere_returns_hemisphere_format);
    RUN_TEST(test_extra_get_hour_angle_compact_hms);
    RUN_TEST(test_extra_get_local_sidereal_time_compact_hms);
    RUN_TEST(test_extra_get_network_status_text);
    RUN_TEST(test_extra_set_ra_steps_per_degree_emits_empty_and_captures_value);
    RUN_TEST(test_extra_set_dec_steps_per_degree_zero_value_skipped);
    RUN_TEST(test_extra_set_dec_limit_lower_with_payload);
    RUN_TEST(test_extra_set_dec_limit_lower_without_payload_uses_current_position);
    RUN_TEST(test_extra_set_dec_limit_lower_clear_invokes_clear);
    RUN_TEST(test_extra_set_manual_slew_mode_one_enables);
    RUN_TEST(test_extra_set_manual_slew_mode_zero_disables);
    RUN_TEST(test_extra_set_backlash_correction_captures_int);
    RUN_TEST(test_extra_set_dec_parking_is_no_op_but_emits_empty);
    RUN_TEST(test_extra_level_unavailable_returns_zero_terminator);
    RUN_TEST(test_extra_level_startup_returns_one_terminator_when_available);
    RUN_TEST(test_extra_level_shutdown_returns_one_terminator_when_available);
    RUN_TEST(test_extra_level_get_temperature_numeric_float_when_available);
}
