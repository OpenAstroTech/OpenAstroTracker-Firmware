/**
 * @brief Pin configuration for a Raspberry Pi Pico (RP2040/RP2350) mounted on
 *        the JackW01 pi-pico-printer-board carrier (https://github.com/jackw01/pi-pico-printer-board).
 *
 * Stepper slot mapping:
 *   X slot  -> RA  (TMC2209 UART address 0)
 *   Y slot  -> DEC (TMC2209 UART address 1)
 *   Z slot  -> AZ  (TMC2209 UART address 2)
 *   E slot  -> ALT (TMC2209 UART address 3)
 *
 * All four stepper enable lines are tied together to a single GPIO on this
 * board (GPIO5). Define every axis EN pin to the same value so the firmware
 * compiles cleanly; in practice enabling/disabling one axis affects all.
 * Per-axis power management should be handled via the TMC2209 IHOLD register
 * over UART rather than toggling EN.
 *
 * TMC2209 UART: all four drivers share a single-wire bus on UART1.
 *   TX -> GPIO8  (UART1 TX)
 *   RX -> GPIO9  (UART1 RX)
 * Pin remapping must be applied before Serial2.begin() in platform setup:
 *   Serial2.setTX(RP2040_UART1_TX_PIN);
 *   Serial2.setRX(RP2040_UART1_RX_PIN);
 */

#pragma once

// ── RA axis (X slot, TMC2209 address 0) ──────────────────────────────────────
#ifndef RA_STEP_PIN
    #define RA_STEP_PIN 7
#endif
#ifndef RA_DIR_PIN
    #define RA_DIR_PIN 6
#endif
#ifndef RA_EN_PIN
    #define RA_EN_PIN 5  // Shared with all axes on this board
#endif
#ifndef RA_DIAG_PIN
    #define RA_DIAG_PIN 4  // X endstop input - used for Hall sensor RA homing
#endif

// RA TMC2209 UART
#ifndef RA_SERIAL_PORT
    #define RA_SERIAL_PORT Serial2  // UART1 via Philhower Serial2
#endif
#ifndef RA_DRIVER_ADDRESS
    #define RA_DRIVER_ADDRESS 0b00  // MS1=LOW, MS2=LOW
#endif

// ── DEC axis (Y slot, TMC2209 address 2) ─────────────────────────────────────
#ifndef DEC_STEP_PIN
    #define DEC_STEP_PIN 11
#endif
#ifndef DEC_DIR_PIN
    #define DEC_DIR_PIN 10
#endif
#ifndef DEC_EN_PIN
    #define DEC_EN_PIN 5  // Shared with all axes on this board
#endif
#ifndef DEC_DIAG_PIN
    #define DEC_DIAG_PIN 3  // Y endstop input
#endif

// DEC TMC2209 UART
#ifndef DEC_SERIAL_PORT
    #define DEC_SERIAL_PORT Serial2  // Shared UART1 bus
#endif
#ifndef DEC_DRIVER_ADDRESS
    #define DEC_DRIVER_ADDRESS 0b01  // MS1=HIGH, MS2=LOW -> address 1
#endif

// ── AZ axis (Z slot, TMC2209 address 1) ──────────────────────────────────────
#ifndef AZ_STEP_PIN
    #define AZ_STEP_PIN 13
#endif
#ifndef AZ_DIR_PIN
    #define AZ_DIR_PIN 12
#endif
#ifndef AZ_EN_PIN
    #define AZ_EN_PIN 5  // Shared with all axes on this board
#endif

// AZ TMC2209 UART
#ifndef AZ_SERIAL_PORT
    #define AZ_SERIAL_PORT Serial2  // Shared UART1 bus
#endif
#ifndef AZ_DRIVER_ADDRESS
    #define AZ_DRIVER_ADDRESS 0b10  // MS1=LOW, MS2=HIGH -> address 2
#endif

// ── ALT axis (Extruder slot, TMC2209 address 3) ───────────────────────────────
#ifndef ALT_STEP_PIN
    #define ALT_STEP_PIN 15
#endif
#ifndef ALT_DIR_PIN
    #define ALT_DIR_PIN 14
#endif
#ifndef ALT_EN_PIN
    #define ALT_EN_PIN 5  // Shared with all axes on this board
#endif

// ALT TMC2209 UART
#ifndef ALT_SERIAL_PORT
    #define ALT_SERIAL_PORT Serial2  // Shared UART1 bus
#endif
#ifndef ALT_DRIVER_ADDRESS
    #define ALT_DRIVER_ADDRESS 0b11  // MS1=HIGH, MS2=HIGH -> address 3
#endif

// ── UART1 physical pin assignments ───────────────────────────────────────────
// The JackW01 board routes UART to the TMC2209 drivers as follows:
//   GPIO8 → R202 (1kΩ) → RESET/TX pad  (UART1 TX)
//   GPIO9 → R201 (100Ω) → MS3/RX pad   (UART1 RX)
//
// BigTreeTech TMC2209 modules have an onboard resistor bridging their
// RESET/TX and MS3/RX pads to PDN_UART, so both JackW01 lines reach
// the TMC2209 PDN_UART pin.  Standard two-wire hardware UART1 is used:
//   TX: GPIO8 (native UART1 TX mux function)
//   RX: GPIO9 (native UART1 RX mux function)
//
// b_setup.hpp calls Serial2.setTX()/setRX() before Serial2.begin() to
// remap UART1 to these pins.
#ifndef RP2040_UART1_TX_PIN
    #define RP2040_UART1_TX_PIN 8
#endif
#ifndef RP2040_UART1_RX_PIN
    #define RP2040_UART1_RX_PIN 9
#endif

// All drivers use hardware UART (Serial2).
#define SW_SERIAL_UART 0

// ── Shared EN pin override ────────────────────────────────────────────────────
// All four TMC2209 EN lines are wired together to GPIO5 on this board.
// After per-axis init leaves GPIO5 in an indeterminate state, b_setup.hpp will
// drive RP2040_SHARED_EN_PIN LOW once to enable all drivers together.
#define RP2040_SHARED_EN_PIN 5

// ── Debug serial port ─────────────────────────────────────────────────────────
#ifndef DEBUG_SERIAL_PORT
    #define DEBUG_SERIAL_PORT Serial  // USB CDC serial via Philhower core
#endif
