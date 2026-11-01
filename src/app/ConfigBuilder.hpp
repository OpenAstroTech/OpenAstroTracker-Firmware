#pragma once

// `<Arduino.h>` is needed because `Configuration_adv.hpp` expresses the switch polarity as
// `LOW`/`HIGH`. The RP2040 core's `Interrupts.h` trips `-Werror=shadow`, so the include is pushed
// out of the warning set the way `main.cpp` does it.
PUSH_NO_WARNINGS
#include <Arduino.h>
POP_NO_WARNINGS

#include "../../Configuration.hpp"
#include "../../Configuration_adv.hpp"

#include "core/AxisMechanicsConfig.hpp"
#include "core/CommsConfig.hpp"
#include "core/DriverConfig.hpp"
#include "core/GuideConfig.hpp"
#include "core/LimitConfig.hpp"
#include "core/SensorConfig.hpp"
#include "core/SiteConfig.hpp"

namespace app
{
/// The one place `src/app/` reads the board's `Configuration*.hpp` values into structs.
///
/// Every function below is `inline` and default-constructs its struct, assigns only literals or
/// products of literals, and returns it. At `-O2` the whole struct folds away and each field
/// reaches its consumer as a constant, so a call site pays nothing for reading
/// `app::buildLimitConfig().decLimitUpDegrees` instead of `DEC_LIMIT_UP` — but the macro is now
/// named once, here, and the rest of the firmware speaks struct fields.
///
/// A builder is *not* the place to decide whether a board has hardware; that is
/// `app::BoardPresence`. It only translates the numbers.
///
/// Deliberately absent, because their only consumer cannot see `core/`:
/// * the Wi-Fi settings — `adapters/WifiControl.cpp`;
/// * `RA/DEC_HOMING_SENSOR_SEARCH_DEGREES` — `adapters/MeadeCommandAdapter.cpp`;
/// * the `MEADE`/`GPS` pins and the driver UART addresses — `boards/*/pins_*.h`.

/// Where the mount is and how it reads the sky.
inline core::SiteConfig buildSiteConfig()
{
    core::SiteConfig config;

    config.siderealSecondsPerDay = SIDEREAL_SECONDS_PER_DAY;
    config.raZeroPointShiftHours = RA_ZERO_POINT_SHIFT_HOURS;
    config.northernHemisphere    = (NORTHERN_HEMISPHERE == 1);
    config.polarisRaHour         = POLARIS_RA_HOUR;
    config.polarisRaMinute       = POLARIS_RA_MINUTE;
    config.polarisRaSecond       = POLARIS_RA_SECOND;

    return config;
}

/// The travel limits the mount enforces.
inline core::LimitConfig buildLimitConfig()
{
    core::LimitConfig config;

    config.raLimitLeftHours       = RA_LIMIT_LEFT;
    config.raLimitRightHours      = RA_LIMIT_RIGHT;
    config.raTrackingLimitHours   = RA_TRACKING_LIMIT;
    config.raPhysicalLimitDegrees = RA_PHYSICAL_LIMIT;
    config.decLimitUpDegrees      = DEC_LIMIT_UP;
    config.decLimitDownDegrees    = DEC_LIMIT_DOWN;

    return config;
}

/// How much faster than tracking a guiding pulse moves each axis.
inline core::GuideConfig buildGuideConfig()
{
    core::GuideConfig config;

    config.raPulseMultiplier  = RA_PULSE_MULTIPLIER;
    config.decPulseMultiplier = DEC_PULSE_MULTIPLIER;

    return config;
}

/// The switch/sensor polarity and the end-switch back-off.
inline core::SensorConfig buildSensorConfig()
{
    core::SensorConfig config;

    config.raEndSwitchActiveState      = RA_END_SWITCH_ACTIVE_STATE;
    config.decEndSwitchActiveState     = DEC_END_SWITCH_ACTIVE_STATE;
    config.raEndSwitchBackslewDegrees  = RA_ENDSWITCH_BACKSLEW_DEG;
    config.decEndSwitchBackslewDegrees = DEC_ENDSWITCH_BACKSLEW_DEG;

#if defined(RA_HOMING_SENSOR_ACTIVE_STATE)
    config.raHomingSensorActiveState = RA_HOMING_SENSOR_ACTIVE_STATE;
#endif
#if defined(DEC_HOMING_SENSOR_ACTIVE_STATE)
    config.decHomingSensorActiveState = DEC_HOMING_SENSOR_ACTIVE_STATE;
#endif

    return config;
}

/// Gearing, microstep modes and per-axis motion tuning.
///
/// A board without an optional axis reads the inert defaults `Configuration_adv.hpp` supplies for
/// that axis' derived numbers, and the fields its own `#if` block never defines keep the struct's
/// zero value — which no consumer reads, because `app::BoardPresence` says the axis is absent.
inline core::AxisMechanicsConfig buildAxisMechanicsConfig()
{
    core::AxisMechanicsConfig config;

    config.ra.speed            = RA_STEPPER_SPEED;
    config.ra.acceleration     = RA_STEPPER_ACCELERATION;
    config.dec.speed           = DEC_STEPPER_SPEED;
    config.dec.acceleration    = DEC_STEPPER_ACCELERATION;
    config.dec.invertDirection = (DEC_INVERT_DIR == 1);

    config.raPulleyTeeth   = RA_PULLEY_TEETH;
    config.decPulleyTeeth  = DEC_PULLEY_TEETH;
    config.raStepperSpr    = RA_STEPPER_SPR;
    config.decStepperSpr   = DEC_STEPPER_SPR;
    config.raTransmission  = RA_TRANSMISSION;
    config.decTransmission = DEC_TRANSMISSION;

    config.stepperServiceIntervalMs = STEPPER_CONTROL_INTERVAL_MS;

#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    config.az.speed                 = AZ_STEPPER_SPEED;
    config.az.acceleration          = AZ_STEPPER_ACCELERATION;
    config.az.invertDirection       = (AZ_INVERT_DIR == 1);
    config.azMicrosteps             = AZ_MICROSTEPPING;
    config.azStepperSpr             = AZ_STEPPER_SPR;
    config.azCircumference          = AZ_CIRCUMFERENCE;
    config.azimuthStepsPerRev       = AZIMUTH_STEPS_PER_REV;
    config.azimuthStepsPerArcMinute = AZIMUTH_STEPS_PER_ARC_MINUTE;
#endif

#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    config.alt.speed                 = ALT_STEPPER_SPEED;
    config.alt.acceleration          = ALT_STEPPER_ACCELERATION;
    config.alt.invertDirection       = (ALT_INVERT_DIR == 1);
    config.altMicrosteps             = ALT_MICROSTEPPING;
    config.altStepperSpr             = ALT_STEPPER_SPR;
    config.altCircumference          = ALT_CIRCUMFERENCE;
    config.altitudeStepsPerRev       = ALTITUDE_STEPS_PER_REV;
    config.altitudeStepsPerArcMinute = ALTITUDE_STEPS_PER_ARC_MINUTE;
#endif

#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    config.focus.speed        = FOCUS_STEPPER_SPEED;
    config.focus.acceleration = FOCUS_STEPPER_ACCELERATION;
#endif

    return config;
}

/// The driver power policy and the UART self-test sweep.
inline core::DriverConfig buildDriverConfig()
{
    core::DriverConfig config;

    config.azAlwaysOn    = (AZ_ALWAYS_ON == 1);
    config.altAlwaysOn   = (ALT_ALWAYS_ON == 1);
    config.focusAlwaysOn = (FOCUSER_ALWAYS_ON == 1);

#if defined(UART_CONNECTION_TEST_TX_DEG)
    config.uartConnectionTestTxDeg = UART_CONNECTION_TEST_TX_DEG;
#endif

    return config;
}

/// Baud rates and the debug mask.
inline core::CommsConfig buildCommsConfig()
{
    core::CommsConfig config;

    config.serialBaudrate      = SERIAL_BAUDRATE;
    config.debugLevel          = DEBUG_LEVEL;
    config.debugSeparateSerial = (DEBUG_SEPARATE_SERIAL == 1);
    config.debugSerialBaudrate = DEBUG_SERIAL_BAUDRATE;
    config.gpsBaudrate         = GPS_BAUD_RATE;

    return config;
}

}  // namespace app
