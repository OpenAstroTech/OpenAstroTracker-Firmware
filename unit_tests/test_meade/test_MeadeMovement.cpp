// Wire-byte tests for the Meade Movement dispatcher (`handleMeadeMovement`).
//
// Covers all `:M...` sub-commands: SlewToTarget, TrackingToggle, GuidePulse,
// MoveAzAltHome, axis-nudge (AZ/AL), continuous slews (e/w/n/s), MoveStepper
// (X<axis><steps>), and Hall-sensor auto-home (HR/HD).

#include <gtest/gtest.h>

#include "core/meade/MeadeParser.hpp"

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

const char *run(const char *suffix, FakeHandlers &h)
{
    static meade::MeadeResponse last;
    last.clear();
    meade::handleMeadeMovement(last, suffix, h);
    return last.c_str();
}

}  // namespace

TEST(MeadeMovement, empty_or_null_suffix_returns_empty)
{
    FakeHandlers h;
    EXPECT_STREQ("", run("", h).c_str());
    EXPECT_STREQ("", run(nullptr, h).c_str());
    EXPECT_EQ(0, h.slewToTargetCalls);
}

TEST(MeadeMovement, slew_to_target_emits_zero_and_calls_handler)
{
    FakeHandlers h;
    EXPECT_STREQ("0", run("S", h).c_str());
    EXPECT_EQ(1, h.slewToTargetCalls);
}

TEST(MeadeMovement, slew_with_trailing_bytes_is_unknown)
{
    FakeHandlers h;
    EXPECT_STREQ("", run("S123", h).c_str());
    EXPECT_EQ(0, h.slewToTargetCalls);
}

TEST(MeadeMovement, tracking_on)
{
    FakeHandlers h;
    EXPECT_STREQ("1", run("T1", h).c_str());
    EXPECT_EQ(1, h.trackingOnCalls);
}

TEST(MeadeMovement, tracking_off)
{
    FakeHandlers h;
    EXPECT_STREQ("1", run("T0", h).c_str());
    EXPECT_EQ(1, h.trackingOffCalls);
}

TEST(MeadeMovement, tracking_bare_or_bad_byte_emits_zero)
{
    FakeHandlers h;
    EXPECT_STREQ("0", run("T", h).c_str());
    EXPECT_STREQ("0", run("T2", h).c_str());
    EXPECT_EQ(0, h.trackingOnCalls);
    EXPECT_EQ(0, h.trackingOffCalls);
}

TEST(MeadeMovement, guide_pulse_lowercase_directions)
{
    FakeHandlers h;
    EXPECT_STREQ("", run("Gn0403", h).c_str());
    EXPECT_EQ(1, h.guidePulseCalls);
    EXPECT_EQ(static_cast<int>(meade::MoveDirection::North), static_cast<int>(h.lastDir));
    EXPECT_EQ(403, h.lastDurationMs);

    EXPECT_STREQ("", run("gs0100", h).c_str());
    EXPECT_EQ(static_cast<int>(meade::MoveDirection::South), static_cast<int>(h.lastDir));
    EXPECT_EQ(100, h.lastDurationMs);

    EXPECT_STREQ("", run("Ge0001", h).c_str());
    EXPECT_EQ(static_cast<int>(meade::MoveDirection::East), static_cast<int>(h.lastDir));
    EXPECT_EQ(1, h.lastDurationMs);

    EXPECT_STREQ("", run("GW9999", h).c_str());
    EXPECT_EQ(static_cast<int>(meade::MoveDirection::West), static_cast<int>(h.lastDir));
    EXPECT_EQ(9999, h.lastDurationMs);
}

TEST(MeadeMovement, guide_pulse_uppercase_direction_accepted)
{
    FakeHandlers h;
    EXPECT_STREQ("", run("GN0500", h).c_str());
    EXPECT_EQ(static_cast<int>(meade::MoveDirection::North), static_cast<int>(h.lastDir));
    EXPECT_EQ(500, h.lastDurationMs);
}

TEST(MeadeMovement, guide_pulse_unknown_direction_defaults_to_east)
{
    FakeHandlers h;
    EXPECT_STREQ("", run("Gx0123", h).c_str());
    EXPECT_EQ(1, h.guidePulseCalls);
    EXPECT_EQ(static_cast<int>(meade::MoveDirection::East), static_cast<int>(h.lastDir));
}

TEST(MeadeMovement, guide_pulse_malformed_emits_zero)
{
    FakeHandlers h;
    EXPECT_STREQ("0", run("Gn040", h).c_str());    // too short
    EXPECT_STREQ("0", run("Gn04030", h).c_str());  // too long
    EXPECT_STREQ("0", run("Gn04A3", h).c_str());   // non-digit
    EXPECT_EQ(0, h.guidePulseCalls);
}

TEST(MeadeMovement, move_az_alt_home)
{
    FakeHandlers h;
    EXPECT_STREQ("1", run("AA", h).c_str());
    EXPECT_EQ(1, h.azAltHomeCalls);
}

TEST(MeadeMovement, move_azimuth)
{
    FakeHandlers h;
    EXPECT_STREQ("", run("AZ+32.5", h).c_str());
    EXPECT_EQ(1, h.azCalls);
    EXPECT_NEAR(32.5, static_cast<double>(h.lastAzArc), 0.001);
}

TEST(MeadeMovement, move_altitude)
{
    FakeHandlers h;
    EXPECT_STREQ("", run("AL-12.25", h).c_str());
    EXPECT_EQ(1, h.alCalls);
    EXPECT_NEAR(-12.25, static_cast<double>(h.lastAlArc), 0.001);
}

TEST(MeadeMovement, continuous_slew_shortcuts)
{
    FakeHandlers h;
    EXPECT_STREQ("", run("e", h).c_str());
    EXPECT_STREQ("", run("w", h).c_str());
    EXPECT_STREQ("", run("n", h).c_str());
    EXPECT_STREQ("", run("s", h).c_str());
    EXPECT_EQ(1, h.slewE);
    EXPECT_EQ(1, h.slewW);
    EXPECT_EQ(1, h.slewN);
    EXPECT_EQ(1, h.slewS);
}

TEST(MeadeMovement, move_stepper_each_axis)
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
        EXPECT_STREQ("1", run(c.suffix, h).c_str());
        EXPECT_EQ(1, h.moveStepperCalls);
        EXPECT_EQ(static_cast<int>(c.axis), static_cast<int>(h.lastAxis));
        EXPECT_EQ(c.steps, h.lastSteps);
    }
}

TEST(MeadeMovement, move_stepper_invalid_axis_returns_zero)
{
    FakeHandlers h;
    EXPECT_STREQ("0", run("Xq500", h).c_str());
    EXPECT_STREQ("0", run("X", h).c_str());
    EXPECT_EQ(0, h.moveStepperCalls);
}

TEST(MeadeMovement, home_ra_directions)
{
    FakeHandlers h;
    h.homeRaResult = true;
    EXPECT_STREQ("1", run("HRR30", h).c_str());
    EXPECT_EQ(1, h.homeRaCalls);
    EXPECT_EQ(-1, h.lastHomeRaDirection);
    EXPECT_STREQ("30", h.lastRaPayload);

    EXPECT_STREQ("1", run("HRL", h).c_str());
    EXPECT_EQ(2, h.homeRaCalls);
    EXPECT_EQ(1, h.lastHomeRaDirection);
    EXPECT_STREQ("", h.lastRaPayload);
}

TEST(MeadeMovement, home_ra_bad_direction_emits_zero_without_calling)
{
    FakeHandlers h;
    EXPECT_STREQ("0", run("HR", h).c_str());
    EXPECT_STREQ("0", run("HRX", h).c_str());
    EXPECT_EQ(0, h.homeRaCalls);
}

TEST(MeadeMovement, home_dec_directions)
{
    FakeHandlers h;
    EXPECT_STREQ("1", run("HDU45", h).c_str());
    EXPECT_EQ(1, h.lastHomeDecDirection);
    EXPECT_STREQ("45", h.lastDecPayload);

    EXPECT_STREQ("1", run("HDD", h).c_str());
    EXPECT_EQ(-1, h.lastHomeDecDirection);
}

TEST(MeadeMovement, home_dec_handler_failure_propagates)
{
    FakeHandlers h;
    h.homeDecResult = false;
    EXPECT_STREQ("0", run("HDU10", h).c_str());
    EXPECT_EQ(1, h.homeDecCalls);
}

TEST(MeadeMovement, unknown_suffix_returns_empty)
{
    FakeHandlers h;
    EXPECT_STREQ("", run("Q", h).c_str());
    EXPECT_STREQ("", run("Z123", h).c_str());
}
