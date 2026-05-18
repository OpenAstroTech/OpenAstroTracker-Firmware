// Wire-byte tests for the Meade Movement dispatcher (`handleMeadeMovement`).
//
// Covers all `:M...` sub-commands: SlewToTarget, TrackingToggle, GuidePulse,
// MoveAzAltHome, axis-nudge (AZ/AL), continuous slews (e/w/n/s), MoveStepper
// (X<axis><steps>), and Hall-sensor auto-home (HR/HD).

#include <unity.h>

#include "core/MeadeParser.hpp"
#include "core/MeadeParser.hpp"

namespace meade = oat::core::meade;

namespace
{

class FakeHandlers : public meade::IMeadeMovementHandlers
{
  public:
    int slewToTargetCalls = 0;
    int trackingOnCalls   = 0;
    int trackingOffCalls  = 0;

    int guidePulseCalls          = 0;
    meade::MoveDirection lastDir = meade::MoveDirection::East;
    int lastDurationMs           = 0;

    int azAltHomeCalls = 0;
    int azCalls        = 0;
    float lastAzArc    = 0.0f;
    int alCalls        = 0;
    float lastAlArc    = 0.0f;

    int slewE = 0, slewW = 0, slewN = 0, slewS = 0;

    int moveStepperCalls         = 0;
    meade::MovementAxis lastAxis = meade::MovementAxis::Ra;
    long lastSteps               = 0;

    int homeRaCalls            = 0;
    int homeDecCalls           = 0;
    int lastHomeRaDirection    = 0;
    int lastHomeDecDirection   = 0;
    const char *lastRaPayload  = nullptr;
    const char *lastDecPayload = nullptr;
    bool homeRaResult          = true;
    bool homeDecResult         = true;

    void onStartSlewToTarget() override
    {
        ++slewToTargetCalls;
    }
    void onTrackingOn() override
    {
        ++trackingOnCalls;
    }
    void onTrackingOff() override
    {
        ++trackingOffCalls;
    }
    void onGuidePulse(meade::MoveDirection dir, int durationMs) override
    {
        ++guidePulseCalls;
        lastDir        = dir;
        lastDurationMs = durationMs;
    }
    void onMoveAzAltHome() override
    {
        ++azAltHomeCalls;
    }
    void onMoveAzimuth(float arcMinutes) override
    {
        ++azCalls;
        lastAzArc = arcMinutes;
    }
    void onMoveAltitude(float arcMinutes) override
    {
        ++alCalls;
        lastAlArc = arcMinutes;
    }
    void onSlewEast() override
    {
        ++slewE;
    }
    void onSlewWest() override
    {
        ++slewW;
    }
    void onSlewNorth() override
    {
        ++slewN;
    }
    void onSlewSouth() override
    {
        ++slewS;
    }
    void onMoveStepper(meade::MovementAxis axis, long steps) override
    {
        ++moveStepperCalls;
        lastAxis  = axis;
        lastSteps = steps;
    }
    bool onHomeRa(int direction, const char *distancePayload) override
    {
        ++homeRaCalls;
        lastHomeRaDirection = direction;
        lastRaPayload       = distancePayload;
        return homeRaResult;
    }
    bool onHomeDec(int direction, const char *distancePayload) override
    {
        ++homeDecCalls;
        lastHomeDecDirection = direction;
        lastDecPayload       = distancePayload;
        return homeDecResult;
    }
};

meade::MeadeResponse run(const char *suffix, FakeHandlers &h)
{
    return meade::handleMeadeMovement(suffix, h);
}

}  // namespace

void setUp(void)
{
}
void tearDown(void)
{
}

void test_empty_or_null_suffix_returns_empty(void)
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", run("", h).c_str());
    TEST_ASSERT_EQUAL_STRING("", run(nullptr, h).c_str());
    TEST_ASSERT_EQUAL_INT(0, h.slewToTargetCalls);
}

void test_slew_to_target_emits_zero_and_calls_handler(void)
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("0", run("S", h).c_str());
    TEST_ASSERT_EQUAL_INT(1, h.slewToTargetCalls);
}

void test_slew_with_trailing_bytes_is_unknown(void)
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", run("S123", h).c_str());
    TEST_ASSERT_EQUAL_INT(0, h.slewToTargetCalls);
}

void test_tracking_on(void)
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("1", run("T1", h).c_str());
    TEST_ASSERT_EQUAL_INT(1, h.trackingOnCalls);
}

void test_tracking_off(void)
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("1", run("T0", h).c_str());
    TEST_ASSERT_EQUAL_INT(1, h.trackingOffCalls);
}

void test_tracking_bare_or_bad_byte_emits_zero(void)
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("0", run("T", h).c_str());
    TEST_ASSERT_EQUAL_STRING("0", run("T2", h).c_str());
    TEST_ASSERT_EQUAL_INT(0, h.trackingOnCalls);
    TEST_ASSERT_EQUAL_INT(0, h.trackingOffCalls);
}

void test_guide_pulse_lowercase_directions(void)
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", run("Gn0403", h).c_str());
    TEST_ASSERT_EQUAL_INT(1, h.guidePulseCalls);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(meade::MoveDirection::North), static_cast<int>(h.lastDir));
    TEST_ASSERT_EQUAL_INT(403, h.lastDurationMs);

    TEST_ASSERT_EQUAL_STRING("", run("gs0100", h).c_str());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(meade::MoveDirection::South), static_cast<int>(h.lastDir));
    TEST_ASSERT_EQUAL_INT(100, h.lastDurationMs);

    TEST_ASSERT_EQUAL_STRING("", run("Ge0001", h).c_str());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(meade::MoveDirection::East), static_cast<int>(h.lastDir));
    TEST_ASSERT_EQUAL_INT(1, h.lastDurationMs);

    TEST_ASSERT_EQUAL_STRING("", run("GW9999", h).c_str());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(meade::MoveDirection::West), static_cast<int>(h.lastDir));
    TEST_ASSERT_EQUAL_INT(9999, h.lastDurationMs);
}

void test_guide_pulse_uppercase_direction_accepted(void)
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", run("GN0500", h).c_str());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(meade::MoveDirection::North), static_cast<int>(h.lastDir));
    TEST_ASSERT_EQUAL_INT(500, h.lastDurationMs);
}

void test_guide_pulse_unknown_direction_defaults_to_east(void)
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", run("Gx0123", h).c_str());
    TEST_ASSERT_EQUAL_INT(1, h.guidePulseCalls);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(meade::MoveDirection::East), static_cast<int>(h.lastDir));
}

void test_guide_pulse_malformed_emits_zero(void)
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("0", run("Gn040", h).c_str());    // too short
    TEST_ASSERT_EQUAL_STRING("0", run("Gn04030", h).c_str());  // too long
    TEST_ASSERT_EQUAL_STRING("0", run("Gn04A3", h).c_str());   // non-digit
    TEST_ASSERT_EQUAL_INT(0, h.guidePulseCalls);
}

void test_move_az_alt_home(void)
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("1", run("AA", h).c_str());
    TEST_ASSERT_EQUAL_INT(1, h.azAltHomeCalls);
}

void test_move_azimuth(void)
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", run("AZ+32.5", h).c_str());
    TEST_ASSERT_EQUAL_INT(1, h.azCalls);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 32.5f, h.lastAzArc);
}

void test_move_altitude(void)
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", run("AL-12.25", h).c_str());
    TEST_ASSERT_EQUAL_INT(1, h.alCalls);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -12.25f, h.lastAlArc);
}

void test_continuous_slew_shortcuts(void)
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", run("e", h).c_str());
    TEST_ASSERT_EQUAL_STRING("", run("w", h).c_str());
    TEST_ASSERT_EQUAL_STRING("", run("n", h).c_str());
    TEST_ASSERT_EQUAL_STRING("", run("s", h).c_str());
    TEST_ASSERT_EQUAL_INT(1, h.slewE);
    TEST_ASSERT_EQUAL_INT(1, h.slewW);
    TEST_ASSERT_EQUAL_INT(1, h.slewN);
    TEST_ASSERT_EQUAL_INT(1, h.slewS);
}

void test_move_stepper_each_axis(void)
{
    struct Case {
        const char *suffix;
        meade::MovementAxis axis;
        long steps;
    };
    static const Case cases[] = {
        {"Xr1000", meade::MovementAxis::Ra, 1000},
        {"Xd-250", meade::MovementAxis::Dec, -250},
        {"Xz42", meade::MovementAxis::Azimuth, 42},
        {"Xl0", meade::MovementAxis::Altitude, 0},
        {"Xf-7", meade::MovementAxis::Focus, -7},
    };
    for (auto &c : cases)
    {
        FakeHandlers h;
        TEST_ASSERT_EQUAL_STRING("1", run(c.suffix, h).c_str());
        TEST_ASSERT_EQUAL_INT(1, h.moveStepperCalls);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(c.axis), static_cast<int>(h.lastAxis));
        TEST_ASSERT_EQUAL_INT(c.steps, h.lastSteps);
    }
}

void test_move_stepper_invalid_axis_returns_zero(void)
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("0", run("Xq500", h).c_str());
    TEST_ASSERT_EQUAL_STRING("0", run("X", h).c_str());
    TEST_ASSERT_EQUAL_INT(0, h.moveStepperCalls);
}

void test_home_ra_directions(void)
{
    FakeHandlers h;
    h.homeRaResult = true;
    TEST_ASSERT_EQUAL_STRING("1", run("HRR30", h).c_str());
    TEST_ASSERT_EQUAL_INT(1, h.homeRaCalls);
    TEST_ASSERT_EQUAL_INT(-1, h.lastHomeRaDirection);
    TEST_ASSERT_EQUAL_STRING("30", h.lastRaPayload);

    TEST_ASSERT_EQUAL_STRING("1", run("HRL", h).c_str());
    TEST_ASSERT_EQUAL_INT(2, h.homeRaCalls);
    TEST_ASSERT_EQUAL_INT(1, h.lastHomeRaDirection);
    TEST_ASSERT_EQUAL_STRING("", h.lastRaPayload);
}

void test_home_ra_bad_direction_emits_zero_without_calling(void)
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("0", run("HR", h).c_str());
    TEST_ASSERT_EQUAL_STRING("0", run("HRX", h).c_str());
    TEST_ASSERT_EQUAL_INT(0, h.homeRaCalls);
}

void test_home_dec_directions(void)
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("1", run("HDU45", h).c_str());
    TEST_ASSERT_EQUAL_INT(1, h.lastHomeDecDirection);
    TEST_ASSERT_EQUAL_STRING("45", h.lastDecPayload);

    TEST_ASSERT_EQUAL_STRING("1", run("HDD", h).c_str());
    TEST_ASSERT_EQUAL_INT(-1, h.lastHomeDecDirection);
}

void test_home_dec_handler_failure_propagates(void)
{
    FakeHandlers h;
    h.homeDecResult = false;
    TEST_ASSERT_EQUAL_STRING("0", run("HDU10", h).c_str());
    TEST_ASSERT_EQUAL_INT(1, h.homeDecCalls);
}

void test_unknown_suffix_returns_empty(void)
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", run("Q", h).c_str());
    TEST_ASSERT_EQUAL_STRING("", run("Z123", h).c_str());
}

void process(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_empty_or_null_suffix_returns_empty);
    RUN_TEST(test_slew_to_target_emits_zero_and_calls_handler);
    RUN_TEST(test_slew_with_trailing_bytes_is_unknown);
    RUN_TEST(test_tracking_on);
    RUN_TEST(test_tracking_off);
    RUN_TEST(test_tracking_bare_or_bad_byte_emits_zero);
    RUN_TEST(test_guide_pulse_lowercase_directions);
    RUN_TEST(test_guide_pulse_uppercase_direction_accepted);
    RUN_TEST(test_guide_pulse_unknown_direction_defaults_to_east);
    RUN_TEST(test_guide_pulse_malformed_emits_zero);
    RUN_TEST(test_move_az_alt_home);
    RUN_TEST(test_move_azimuth);
    RUN_TEST(test_move_altitude);
    RUN_TEST(test_continuous_slew_shortcuts);
    RUN_TEST(test_move_stepper_each_axis);
    RUN_TEST(test_move_stepper_invalid_axis_returns_zero);
    RUN_TEST(test_home_ra_directions);
    RUN_TEST(test_home_ra_bad_direction_emits_zero_without_calling);
    RUN_TEST(test_home_dec_directions);
    RUN_TEST(test_home_dec_handler_failure_propagates);
    RUN_TEST(test_unknown_suffix_returns_empty);
    UNITY_END();
}

#ifdef ARDUINO
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
