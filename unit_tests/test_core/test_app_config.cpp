#include <gtest/gtest.h>

#include "core/AxisMechanicsConfig.hpp"
#include "core/CommsConfig.hpp"
#include "core/DriverConfig.hpp"
#include "core/GuideConfig.hpp"
#include "core/LimitConfig.hpp"
#include "core/SensorConfig.hpp"
#include "core/SiteConfig.hpp"

// The configuration structs are plain data: `app::build*Config()` fills them from the board's
// `Configuration*.hpp`, everything below it reads fields. These tests pin the two things the rest
// of the firmware relies on and a board builder could otherwise break silently: the defaults a
// host build sees, and the units the fields are in.

TEST(SiteConfigTest, DefaultsMatchTheFirmwareAssumptions)
{
    const core::SiteConfig site;

    EXPECT_FLOAT_EQ(86164.0905f, site.siderealSecondsPerDay);
    EXPECT_FLOAT_EQ(0.0f, site.raZeroPointShiftHours);
    EXPECT_TRUE(site.northernHemisphere);
    EXPECT_EQ(3, site.polarisRaHour);
    EXPECT_EQ(0, site.polarisRaMinute);
    EXPECT_EQ(8, site.polarisRaSecond);
}

// RA travel limits are hours from home; the DEC pair is degrees, where 0 means "no limit".
TEST(LimitConfigTest, DefaultsAreTheAdvancedConfigurationDefaults)
{
    const core::LimitConfig limits;

    EXPECT_FLOAT_EQ(5.0f, limits.raLimitLeftHours);
    EXPECT_FLOAT_EQ(7.0f, limits.raLimitRightHours);
    EXPECT_FLOAT_EQ(7.0f, limits.raTrackingLimitHours);
    EXPECT_FLOAT_EQ(7.0f, limits.raPhysicalLimitDegrees);
    EXPECT_FLOAT_EQ(0.0f, limits.decLimitUpDegrees);
    EXPECT_FLOAT_EQ(0.0f, limits.decLimitDownDegrees);
}

TEST(GuideConfigTest, DefaultsAreTheAdvancedConfigurationDefaults)
{
    const core::GuideConfig guide;

    EXPECT_FLOAT_EQ(1.5f, guide.raPulseMultiplier);
    EXPECT_FLOAT_EQ(0.5f, guide.decPulseMultiplier);
}

// The active state is stored as the raw 0/1 the Arduino cores give `LOW`/`HIGH`, so `core/` can
// hold it without `<Arduino.h>`.
TEST(SensorConfigTest, ActiveStatesDefaultToLowAndBackOffIsHalfADegree)
{
    const core::SensorConfig sensors;

    EXPECT_EQ(0, sensors.raEndSwitchActiveState);
    EXPECT_EQ(0, sensors.decEndSwitchActiveState);
    EXPECT_EQ(0, sensors.raHomingSensorActiveState);
    EXPECT_EQ(0, sensors.decHomingSensorActiveState);
    EXPECT_FLOAT_EQ(0.5f, sensors.raEndSwitchBackslewDegrees);
    EXPECT_FLOAT_EQ(0.5f, sensors.decEndSwitchBackslewDegrees);
}

// A board without an optional axis leaves the axis' speed at zero and the axis unflagged, which is
// what makes `app::BoardPresence` the only thing a consumer needs to ask.
TEST(AxisMechanicsConfigTest, AbsentAxesStartEmpty)
{
    const core::AxisMechanicsConfig mechanics;

    EXPECT_EQ(0ul, mechanics.az.speed);
    EXPECT_EQ(0ul, mechanics.alt.speed);
    EXPECT_EQ(0ul, mechanics.focus.speed);
    EXPECT_FALSE(mechanics.az.invertDirection);
    EXPECT_FALSE(mechanics.alt.invertDirection);

    // The RA/DEC defaults are the ones the geometry log has always printed.
    EXPECT_EQ(16, mechanics.raPulleyTeeth);
    EXPECT_EQ(16, mechanics.decPulleyTeeth);
    EXPECT_EQ(400, mechanics.raStepperSpr);
    EXPECT_EQ(400, mechanics.decStepperSpr);
}

// The driver policy is per-board, not per-axis: a board that wants a coil energised while idle
// keeps its enable pin asserted, and a blank build energises nothing early.
TEST(DriverConfigTest, DefaultsLeaveEveryAxisPoweredDownAndTheSweepAtFiveDegrees)
{
    const core::DriverConfig drivers;

    EXPECT_FALSE(drivers.azAlwaysOn);
    EXPECT_FALSE(drivers.altAlwaysOn);
    EXPECT_FALSE(drivers.focusAlwaysOn);
    EXPECT_FLOAT_EQ(5.0f, drivers.uartConnectionTestTxDeg);
}

TEST(CommsConfigTest, DefaultsAreTheAscomRateAndNoLogging)
{
    const core::CommsConfig comms;

    EXPECT_EQ(19200u, comms.serialBaudrate);
    EXPECT_EQ(0, comms.debugLevel);
    EXPECT_FALSE(comms.debugSeparateSerial);
    EXPECT_EQ(115200u, comms.debugSerialBaudrate);
    EXPECT_EQ(9600u, comms.gpsBaudrate);
}
