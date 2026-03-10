/**
 * @brief Local hardware configuration for a Raspberry Pi Pico (RP2040)
 *        on the BigTreeTech SKR Pico v1.0 printer controller board.
 *
 * This file is NOT tracked by Git and will survive branch changes.
 * It is automatically included by LocalConfiguration.hpp when
 * BOARD == BOARD_RP2040_SKR_PICO.
 *
 * Hardware summary
 * ----------------
 *  MCU          : RP2040 (Raspberry Pi Pico)
 *  Board        : BTT SKR Pico v1.0
 *  Stepper slot : X  -> RA    (TMC2209, UART addr 0)
 *  Stepper slot : Y  -> DEC   (TMC2209, UART addr 2)
 *  Stepper slot : Z  -> AZ    (TMC2209, UART addr 1)
 *  Stepper slot : E0 -> ALT   (TMC2209, UART addr 3)
 *  Motor rating : 0.9 deg / step (400 SPR), 470 mA
 *  Pulley       : 16T aluminium GT2
 *  RA homing    : Hall sensor on GPIO4 (X-STOP / RA_DIAG_PIN)
 *
 * All pin assignments are in boards/RP2040_SKR_PICO/pins_RP2040_SKR_PICO.hpp.
 * R_SENSE (0.11 ohm) is set globally in src/a_inits.hpp for all TMC2209_UART
 * builds and does not need to be repeated here.
 *
 * Key difference from JackW01: each stepper driver has its own EN pin.
 * AZ_ALWAYS_ON and ALT_ALWAYS_ON are NOT required on this board.
 */
#pragma once

// ── Board ─────────────────────────────────────────────────────────────────────
#define BOARD BOARD_RP2040_SKR_PICO

// ── Hemisphere ────────────────────────────────────────────────────────────────
#define NORTHERN_HEMISPHERE 1

// ── Display (none) ────────────────────────────────────────────────────────────
#define DISPLAY_TYPE      DISPLAY_TYPE_NONE
#define INFO_DISPLAY_TYPE INFO_DISPLAY_TYPE_NONE

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

// ── Steps per revolution (0.9 deg motor = 400 full steps/rev) ─────────────────
#define RA_STEPPER_SPR  400
#define DEC_STEPPER_SPR 400
#define AZ_STEPPER_SPR  400
#define ALT_STEPPER_SPR 400

// ── Pulley tooth count (16T aluminium GT2) ────────────────────────────────────
#define RA_PULLEY_TEETH  16
#define DEC_PULLEY_TEETH 16
#define AZ_PULLEY_TEETH  16
#define ALT_PULLEY_TEETH 16

// ── Microstepping ─────────────────────────────────────────────────────────────
// TMC2209 UART mode can switch microstepping dynamically at runtime.
// RA switches between slew (8×) and fine tracking (256×).
// DEC switches between slew (16×) and fine guiding (256×).
// AZ and ALT use a fixed microstepping mode (no dynamic switching).
#define RA_SLEW_MICROSTEPPING     8    // µsteps used while slewing RA
#define RA_TRACKING_MICROSTEPPING 256  // µsteps used for sidereal tracking
#define DEC_SLEW_MICROSTEPPING    16   // µsteps used while slewing DEC
#define DEC_GUIDE_MICROSTEPPING   256  // µsteps used for guide pulses
#define AZ_MICROSTEPPING          64
#define ALT_MICROSTEPPING         4

// ── Motor direction ───────────────────────────────────────────────────────────
// Set to 1 to reverse the direction of an axis if it moves the wrong way.
// Verify with manual move commands: :Mw#/:Me# (RA), :Mn#/:Ms# (DEC),
// :MAL#/:MAR# (AZ), :MAU#/:MAD# (ALT).  Then set the appropriate flag to 1.
#define RA_INVERT_DIR  0
#define DEC_INVERT_DIR 1
#define AZ_INVERT_DIR  0
#define ALT_INVERT_DIR 0

// ── StealthChop (silent mode) ─────────────────────────────────────────────────
// Configuration_adv.hpp defaults both to 0 (SpreadCycle = noisy).
// Set to 1 to enable StealthChop (silent) for RA tracking and DEC moves.
#define RA_UART_STEALTH_MODE  1
#define DEC_UART_STEALTH_MODE 1

// ── Motor current ─────────────────────────────────────────────────────────────
// Motors are rated at 470 mA.  Run at 75 % of rated current to reduce heat
// while maintaining adequate torque.  RMS current is computed automatically:
//   RMSCURRENT = RATING * (SETTING / 100) / sqrt(2)  => ~249 mA RMS
// Do NOT define XXX_RMSCURRENT directly; Configuration_adv.hpp will error.
#define RA_MOTOR_CURRENT_RATING      470  // mA
#define RA_OPERATING_CURRENT_SETTING  75  // %

#define DEC_MOTOR_CURRENT_RATING     470  // mA
#define DEC_OPERATING_CURRENT_SETTING 75  // %

#define AZ_MOTOR_CURRENT_RATING      470  // mA
#define AZ_OPERATING_CURRENT_SETTING  75  // %

#define ALT_MOTOR_CURRENT_RATING     470  // mA
#define ALT_OPERATING_CURRENT_SETTING 75  // %

// ── AZ/ALT hold current ───────────────────────────────────────────────────────
// AZ must hold position against backlash forces; use 40 % of run current.
// ALT is held by friction and gearing; bleed hold current to zero.
// These are percentages (0-100) converted to TMC2209 IHOLD by the firmware.
#define AZ_MOTOR_HOLD_SETTING  40  // %
#define ALT_MOTOR_HOLD_SETTING  0  // %

// ── DEC axis travel limits ────────────────────────────────────────────────────
// Physical limits measured from the home (level) position.
// These must be defined here; Configuration_adv.hpp defaults to 0.0f for
// non-OAM builds, which makes :Mn#/:Ms# target position 0 and do nothing.
#ifndef DEC_LIMIT_UP
    #define DEC_LIMIT_UP   135.0f  // 135° north of home
#endif
#ifndef DEC_LIMIT_DOWN
    #define DEC_LIMIT_DOWN  35.0f  // 35° south of home
#endif

// ── RA Hall sensor auto-home ──────────────────────────────────────────────────
// Hall sensor is wired to GPIO4 (X-STOP input / RA_DIAG_PIN on SKR Pico).
// Active state LOW: sensor pulls GPIO4 LOW when the magnet is present.
// Search distance: 30° default (RA_HOMING_SENSOR_SEARCH_DEGREES in Configuration_adv.hpp).
// GPIO4 is configured INPUT (no pull-up); add external 10kΩ to 3.3V if sensor
// is open-drain, or change to INPUT_PULLUP in b_setup.hpp.
#define USE_HALL_SENSOR_RA_AUTOHOME    1
#define RA_HOMING_SENSOR_PIN           4    // GPIO4 = RA_DIAG_PIN (X-STOP)
#define RA_HOMING_SENSOR_ACTIVE_STATE  LOW  // sensor pulls LOW when active

// ── GPS ───────────────────────────────────────────────────────────────────────
// ATGM336H-5N wired to left-side header: GPS TX→IO1, GPS RX→IO0, VCC→5V, GND.
// GPS_SERIAL_PORT, RP2040_GPS_TX_PIN, RP2040_GPS_RX_PIN defined in pins file.
// GPS_BAUD_RATE defaults to 9600 in Configuration_adv.hpp (matches module default).
#define USE_GPS 1

// ── FAN ───────────────────────────────────────────────────────────────────────
// FAN1 power level as a percentage (0 = off, 100 = full speed).
// The pin and PWM are defined in pins_RP2040_SKR_PICO.hpp.
#define RP2040_FAN1_POWER_PERCENT 60

// Track immediately after boot
#define TRACK_ON_BOOT 0

// Uncomment to override defaults for stepper speeds and accelerations
//#define RA_STEPPER_SPEED 1200  // You can change the speed and acceleration of the steppers here. Max. Speed = 3000.
//#define RA_STEPPER_ACCELERATION 6000
//#define DEC_STEPPER_SPEED 1300  // You can change the speed and acceleration of the steppers here. Max. Speed = 3000.
//#define DEC_STEPPER_ACCELERATION 6000
