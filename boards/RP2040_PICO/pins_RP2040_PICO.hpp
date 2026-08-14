/**
 * @brief Pin configuration for a bare Raspberry Pi Pico (RP2040/RP2350)
 *        with user-defined wiring.
 *
 * This file provides no default pin assignments because there is no standard
 * wiring for a bare Pico build. All pin definitions MUST be supplied in your
 * local configuration file (e.g. Configuration_local_rp2040_pico.hpp).
 *
 * Required definitions (set in your local config):
 *
 *   RA_STEP_PIN, RA_DIR_PIN, RA_EN_PIN
 *   DEC_STEP_PIN, DEC_DIR_PIN, DEC_EN_PIN
 *
 * If using TMC2209 UART:
 *   RA_SERIAL_PORT, RA_DRIVER_ADDRESS
 *   DEC_SERIAL_PORT, DEC_DRIVER_ADDRESS
 *   RP2040_UART1_TX_PIN, RP2040_UART1_RX_PIN  (if using Serial2/UART1)
 *
 * If using Hall sensor homing:
 *   RA_DIAG_PIN
 *
 * Optional axes (AutoPA):
 *   AZ_STEP_PIN, AZ_DIR_PIN, AZ_EN_PIN, AZ_SERIAL_PORT, AZ_DRIVER_ADDRESS
 *   ALT_STEP_PIN, ALT_DIR_PIN, ALT_EN_PIN, ALT_SERIAL_PORT, ALT_DRIVER_ADDRESS
 *
 * Debug serial port:
 *   DEBUG_SERIAL_PORT  (defaults to Serial / USB CDC if not defined)
 */

#pragma once

// ── Validate that the user has provided the minimum required pins ─────────────
#if !defined(RA_STEP_PIN) || !defined(RA_DIR_PIN) || !defined(RA_EN_PIN)
    #error "BOARD_RP2040_PICO / BOARD_RP2350_PICO2: RA step/dir/en pins must be defined in your local configuration file."
#endif
#if !defined(DEC_STEP_PIN) || !defined(DEC_DIR_PIN) || !defined(DEC_EN_PIN)
    #error "BOARD_RP2040_PICO / BOARD_RP2350_PICO2: DEC step/dir/en pins must be defined in your local configuration file."
#endif

// ── Fallback debug serial ─────────────────────────────────────────────────────
#ifndef DEBUG_SERIAL_PORT
    #define DEBUG_SERIAL_PORT Serial  // USB CDC serial via Philhower core
#endif
