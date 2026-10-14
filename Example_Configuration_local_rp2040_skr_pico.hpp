/**
 * @brief Bench configuration for the BTT SKR Pico v1.0 RP2040 board.
 *
 * Personal local config (gitignored). Based on
 * Configuration_local_examples/Configuration_local_rp2040_skr_pico.hpp,
 * adapted for the bench hardware:
 *   - Hall homing sensors on RA (X-STOP GPIO4) AND DEC (Y-STOP GPIO3)
 *   - DEC endstops on Z-STOP (GPIO25, down) and E-STOP (GPIO16, up)
 *   - SSD1306 info display on I2C0, GP0/GP1 on the left-side header
 *   - GPS disabled for the bench
 *
 * All other pin assignments are in boards/RP2040_SKR_PICO/pins_RP2040_SKR_PICO.hpp.
 */
#pragma once

// ── Board ─────────────────────────────────────────────────────────────────────
#define BOARD BOARD_RP2040_SKR_PICO
#define OAM
#define TEST_VERIFY_MODE 0

// ── Hemisphere ────────────────────────────────────────────────────────────────
#define NORTHERN_HEMISPHERE 1

// ── Display ────────────────────────────────────────────────────────────────────
// No LCD menu display (RP2040 phase 1); SSD1306 128x64 info display on I2C0.
// GP0/GP1 on the left-side header (IO0/IO1/GND/5V) - the only header-accessible
// I2C0 pair on the SKR Pico. Shares pins with the default GPS wiring, so
// USE_GPS must stay 0 while the display is enabled.
// (GP20/GP21 are MOSFET-gate-only nets on this board - HB/FAN/Laser outputs,
// never exposed as raw GPIO; wiring I2C there hangs the bus and the firmware.)
#define DISPLAY_TYPE             DISPLAY_TYPE_NONE
#define INFO_DISPLAY_TYPE        INFO_DISPLAY_TYPE_I2C_SSD1306_128x64
#define INFO_DISPLAY_I2C_ADDRESS 0x3C
#define INFO_DISPLAY_I2C_SDA_PIN 0  // GP0 (I2C0 SDA, left header IO0)
#define INFO_DISPLAY_I2C_SCL_PIN 1  // GP1 (I2C0 SCL, left header IO1)
#define INFO_DISPLAY_UPSIDE_DOWN 1
#define INFO_DISPLAY_MIRRORED    0

// ── Stepper control interval ─────────────────────────────────────────────────
// The RP2040 can handle a 10kHz stepper timer
#define STEPPER_CONTROL_INTERVAL_MS 0.1f  // 10 kHz

// ── Stepper motor types ───────────────────────────────────────────────────────
#define RA_STEPPER_TYPE  STEPPER_TYPE_ENABLED
#define DEC_STEPPER_TYPE STEPPER_TYPE_ENABLED
#define AZ_STEPPER_TYPE  STEPPER_TYPE_ENABLED
#define ALT_STEPPER_TYPE STEPPER_TYPE_ENABLED

// ── Stepper driver types ──────────────────────────────────────────────────────
#define RA_DRIVER_TYPE  DRIVER_TYPE_TMC2209_UART
#define DEC_DRIVER_TYPE DRIVER_TYPE_TMC2209_UART
#define AZ_DRIVER_TYPE  DRIVER_TYPE_TMC2209_UART
#define ALT_DRIVER_TYPE DRIVER_TYPE_TMC2209_UART

// ── RA Settings ───────────────────────────────────────────────────────────────
#define RA_SLEW_MICROSTEPPING        16   // µsteps used while slewing RA
#define RA_TRACKING_MICROSTEPPING    256  // µsteps used for sidereal tracking
#define RA_STEPPER_SPEED             8000  // steps/s. If SPEED and ACCELERATION numbers are the same, the spool up to full speed takes one second.
#define RA_STEPPER_ACCELERATION      8000      // steps/s/s
#define RA_MOTOR_CURRENT_RATING      1000      // mA
#define RA_OPERATING_CURRENT_SETTING 75        // %
#define RA_WHEEL_CIRCUMFERENCE       1130.97f  // 360mm diam
#define RA_PULLEY_TEETH              16
#define RA_STEPPER_SPR               (400 * 9)  // change to (200 * 9) for 1.8° stepper
#define RA_LIMIT_LEFT                6.0f
#define RA_LIMIT_RIGHT               6.0f
#define RA_INVERT_DIR                1

// ── DEC settings ────────────────────────────────────────────────────────────
#define DEC_SLEW_MICROSTEPPING        16   // µsteps used while slewing DEC
#define DEC_GUIDE_MICROSTEPPING       256  // µsteps used for guide pulses
#define DEC_STEPPER_SPEED             8000  // steps/s. If SPEED and ACCELERATION numbers are the same, the spool up to full speed takes one second.
#define DEC_STEPPER_ACCELERATION      8000  // steps/s/s
#define DEC_MOTOR_CURRENT_RATING      1000  // mA
#define DEC_OPERATING_CURRENT_SETTING 75    // %
#define DEC_WHEEL_CIRCUMFERENCE       816.814f
#define DEC_PULLEY_TEETH              16
#define DEC_STEPPER_SPR               (400 * 9)  // change to (200 * 9) for 1.8° stepper
#define DEC_LIMIT_UP                  130
#define DEC_LIMIT_DOWN                130
#define DEC_INVERT_DIR                1

// ── StealthChop (silent mode) ─────────────────────────────────────────────────
#define RA_UART_STEALTH_MODE  1
#define DEC_UART_STEALTH_MODE 1

// ── AZIMUTH definitions ──────────────────────────────────────────────────────
#define AZ_MICROSTEPPING             16
#define AZ_STEPPER_SPEED             2500
#define AZ_STEPPER_ACCELERATION      1250
#define AZ_MOTOR_CURRENT_RATING      2000  // mA
#define AZ_OPERATING_CURRENT_SETTING 80    // %
#define AZ_STEPPER_SPR               400
#define AZ_ALWAYS_ON                 1
#define AZ_MOTOR_HOLD_SETTING        15
#define AZ_PULLEY_TEETH              20
#define AZ_ROD_PITCH                 1.0f  // mm per full rev of stepper
#define AZ_CIRCUMFERENCE             (115 * 2 * 3.1515927)
#define AZIMUTH_STEPS_PER_REV        (AZ_CIRCUMFERENCE / AZ_ROD_PITCH * AZ_STEPPER_SPR * AZ_MICROSTEPPING)  // Steps needed to turn AZ 360deg
#define AZ_INVERT_DIR                1

// ── ALTITUDE definitions ─────────────────────────────────────────────────────
#define ALT_MICROSTEPPING             32
#define ALT_STEPPER_SPEED             2500
#define ALT_STEPPER_ACCELERATION      1250
#define ALT_MOTOR_CURRENT_RATING      1000  // mA
#define ALT_OPERATING_CURRENT_SETTING 70    // %
#define ALT_STEPPER_SPR               400
#define ALT_INVERT_DIR                0
#define ALT_ALWAYS_ON                 0

// ── Hall sensor auto-homing (RA and DEC) ──────────────────────────────────────
// RA sensor on X-STOP (GPIO4 = RA_DIAG_PIN), DEC sensor on Y-STOP (GPIO3 = DEC_DIAG_PIN).
// Active state LOW: sensor pulls the GPIO LOW when the magnet is present.
#define USE_HALL_SENSOR_RA_AUTOHOME   0
#define RA_HOMING_SENSOR_PIN          4  // GPIO4 = X-STOP
#define RA_HOMING_SENSOR_ACTIVE_STATE LOW

#define USE_HALL_SENSOR_DEC_AUTOHOME   0
#define DEC_HOMING_SENSOR_PIN          3  // GPIO3 = Y-STOP
#define DEC_HOMING_SENSOR_ACTIVE_STATE LOW

// ── DEC endstops ──────────────────────────────────────────────────────────────
// Wired to the two remaining endstop inputs on the SKR Pico.
#define USE_DEC_END_SWITCH            0
#define DEC_ENDSWITCH_DOWN_SENSOR_PIN 25  // GPIO25 = Z-STOP
#define DEC_ENDSWITCH_UP_SENSOR_PIN   16  // GPIO16 = E-STOP
#define DEC_END_SWITCH_ACTIVE_STATE   LOW

// ── GPS ───────────────────────────────────────────────────────────────────────
// Disabled for the bench (no module connected). NOTE: GPS shares GP0/GP1 with
// the info display on this board - only one of the two can be enabled.
#define USE_GPS 0

// ── FAN (disabled for now)───────────────────────────────────────────────────────────────────────
// #define RP2040_FAN1_POWER_PERCENT 60

// Track immediately after boot
#define TRACK_ON_BOOT 0

#define DEBUG_LEVEL (DEBUG_NONE)
