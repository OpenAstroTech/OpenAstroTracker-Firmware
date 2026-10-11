/**
 * @brief Pin configuration for a Raspberry Pi Pico (RP2040) on the
 *        BigTreeTech SKR Pico v1.0 printer controller board.
 *
 * Hardware reference: https://github.com/bigtreetech/SKR-Pico
 * Authoritative pin source: Klipper/SKR Pico klipper.cfg
 *
 * Stepper slot mapping (OAT axis -> board slot -> TMC2209 UART address):
 *   X slot  -> RA  (TMC2209 UART address 0)
 *   Y slot  -> DEC (TMC2209 UART address 2)
 *   Z slot  -> AZ  (TMC2209 UART address 1)
 *   E0 slot -> ALT (TMC2209 UART address 3)
 *
 * Note: DEC uses address 2 and AZ uses address 1 — the opposite of the
 *       JackW01 board.  This reflects the MS1/MS2 resistor strapping on
 *       the SKR Pico PCB.
 *
 * Each stepper driver has its own dedicated EN pin (unlike the JackW01
 * board where all EN lines share a single GPIO).  No RP2040_SHARED_EN_PIN
 * is needed; AZ_ALWAYS_ON / ALT_ALWAYS_ON are not required.
 *
 * TMC2209 UART: all four drivers share a two-wire bus on UART1.
 *   TX -> GPIO8  (UART1 TX, same physical assignment as JackW01)
 *   RX -> GPIO9  (UART1 RX, same physical assignment as JackW01)
 * Pin remapping is applied in b_setup.hpp via:
 *   Serial2.setTX(RP2040_UART1_TX_PIN);
 *   Serial2.setRX(RP2040_UART1_RX_PIN);
 *
 * The SKR Pico has on-board TMC2209 ICs with proper PDN_UART wiring
 * (both TX and RX reach each driver).  No jumper is required.
 *
 * Endstop inputs (also usable as homing sensors):
 *   X-STOP -> GPIO4  (RA Hall sensor / diag input)
 *   Y-STOP -> GPIO3
 *   Z-STOP -> GPIO25
 *   E-STOP -> GPIO16
 *
 * Additional I/O:
 *   BLTouch sensor  -> GPIO22
 *   BLTouch control -> GPIO29
 *   NeoPixel data   -> GPIO24
 */

#pragma once

// ── RA axis (X slot, TMC2209 address 0) ──────────────────────────────────────
#ifndef RA_STEP_PIN
    #define RA_STEP_PIN 11
#endif
#ifndef RA_DIR_PIN
    #define RA_DIR_PIN 10
#endif
#ifndef RA_EN_PIN
    #define RA_EN_PIN 12  // Individual EN pin (active LOW)
#endif
#ifndef RA_DIAG_PIN
    #define RA_DIAG_PIN 4  // X-STOP input - used for Hall sensor RA homing
#endif

// RA TMC2209 UART
#ifndef RA_SERIAL_PORT
    #define RA_SERIAL_PORT Serial2  // UART1 via Philhower Serial2
#endif
#ifndef RA_DRIVER_ADDRESS
    #define RA_DRIVER_ADDRESS 0b00  // MS1=LOW, MS2=LOW -> address 0
#endif

// ── DEC axis (Y slot, TMC2209 address 2) ─────────────────────────────────────
#ifndef DEC_STEP_PIN
    #define DEC_STEP_PIN 6
#endif
#ifndef DEC_DIR_PIN
    #define DEC_DIR_PIN 5
#endif
#ifndef DEC_EN_PIN
    #define DEC_EN_PIN 7  // Individual EN pin (active LOW)
#endif
#ifndef DEC_DIAG_PIN
    #define DEC_DIAG_PIN 3  // Y-STOP input
#endif

// DEC TMC2209 UART
#ifndef DEC_SERIAL_PORT
    #define DEC_SERIAL_PORT Serial2  // Shared UART1 bus
#endif
#ifndef DEC_DRIVER_ADDRESS
    #define DEC_DRIVER_ADDRESS 0b10  // MS1=LOW, MS2=HIGH -> address 2
#endif

// ── AZ axis (Z slot, TMC2209 address 1) ──────────────────────────────────────
#ifndef AZ_STEP_PIN
    #define AZ_STEP_PIN 19
#endif
#ifndef AZ_DIR_PIN
    #define AZ_DIR_PIN 28
#endif
#ifndef AZ_EN_PIN
    #define AZ_EN_PIN 2  // Individual EN pin (active LOW)
#endif

// AZ TMC2209 UART
#ifndef AZ_SERIAL_PORT
    #define AZ_SERIAL_PORT Serial2  // Shared UART1 bus
#endif
#ifndef AZ_DRIVER_ADDRESS
    #define AZ_DRIVER_ADDRESS 0b01  // MS1=HIGH, MS2=LOW -> address 1
#endif

// ── ALT axis (E0 slot, TMC2209 address 3) ────────────────────────────────────
#ifndef ALT_STEP_PIN
    #define ALT_STEP_PIN 14
#endif
#ifndef ALT_DIR_PIN
    #define ALT_DIR_PIN 13
#endif
#ifndef ALT_EN_PIN
    #define ALT_EN_PIN 15  // Individual EN pin (active LOW)
#endif

// ALT TMC2209 UART
#ifndef ALT_SERIAL_PORT
    #define ALT_SERIAL_PORT Serial2  // Shared UART1 bus
#endif
#ifndef ALT_DRIVER_ADDRESS
    #define ALT_DRIVER_ADDRESS 0b11  // MS1=HIGH, MS2=HIGH -> address 3
#endif

// ── UART1 physical pin assignments ───────────────────────────────────────────
// The SKR Pico routes UART1 to all four on-board TMC2209 ICs via a shared
// two-wire bus.  GPIO8 and GPIO9 are the native UART1 TX/RX mux functions.
// b_setup.hpp calls Serial2.setTX()/setRX() before Serial2.begin().
#ifndef RP2040_UART1_TX_PIN
    #define RP2040_UART1_TX_PIN 8
#endif
#ifndef RP2040_UART1_RX_PIN
    #define RP2040_UART1_RX_PIN 9
#endif

// All drivers use hardware UART (Serial2).
#define SW_SERIAL_UART 0

// ── Debug serial port ─────────────────────────────────────────────────────────
#ifndef DEBUG_SERIAL_PORT
    #define DEBUG_SERIAL_PORT Serial  // USB CDC serial via Philhower core
#endif

// ── GPS (UART0 on left-side header) ──────────────────────────────────────────
// Left-side 5-pin header: IO0=GPIO0, IO1=GPIO1, GND, 5V.
// RP2040 pin mux is fixed: GPIO0 = UART0 TX only, GPIO1 = UART0 RX only.
//   GPS RX → GPIO0 / IO0  (RP2040 UART0 TX)
//   GPS TX → GPIO1 / IO1  (RP2040 UART0 RX)
#define GPS_SERIAL_PORT   Serial1
#define RP2040_GPS_TX_PIN 0  // GPIO0 — UART0 TX → GPS RX
#define RP2040_GPS_RX_PIN 1  // GPIO1 — UART0 RX ← GPS TX

// ── FAN outputs ───────────────────────────────────────────────────────────────
// FAN1 connector: switched 12/24V via N-channel MOSFET gated by GPIO17.
// Power is set via analogWrite (0-100 %). Override RP2040_FAN1_POWER_PERCENT
// in your local config file to reduce fan speed.
#define RP2040_FAN1_PIN 17
#ifndef RP2040_FAN1_POWER_PERCENT
    #define RP2040_FAN1_POWER_PERCENT 100
#endif
