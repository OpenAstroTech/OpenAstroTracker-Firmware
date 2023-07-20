/**
 * @brief a pins configuration file for an ESP32-based OAT.
 */

#pragma once

// DRIVER_TYPE_TMC2209_UART requires 4 digital pins in Arduino pin numbering
#ifndef RA_STEP_PIN
    #define RA_STEP_PIN 27  // STEP
#endif
#ifndef RA_DIR_PIN
    #define RA_DIR_PIN 26  // DIR
#endif
#ifndef RA_EN_PIN
    #define RA_EN_PIN 25  // Enable
#endif

// DRIVER_TYPE_TMC2209_UART HardwareSerial port, can be shared across all drivers
#ifndef RA_SERIAL_PORT
    #define RA_SERIAL_PORT Serial1
#endif
#ifndef RA_DRIVER_ADDRESS
    #define RA_DRIVER_ADDRESS 0b00  // Set by MS1/MS2. LOW/LOW in this case
#endif
// DRIVER_TYPE_TMC2209_UART requires 4 digital pins in Arduino pin numbering
#ifndef DEC_STEP_PIN
    #define DEC_STEP_PIN 33  // STEP
#endif
#ifndef DEC_DIR_PIN
    #define DEC_DIR_PIN 32  // DIR
#endif
#ifndef DEC_EN_PIN
    #define DEC_EN_PIN 25  // Enable
#endif

// DRIVER_TYPE_TMC2209_UART HardwareSerial port, can be shared across all drivers
#ifndef DEC_SERIAL_PORT
    #define DEC_SERIAL_PORT Serial1
#endif
#ifndef DEC_DRIVER_ADDRESS
    #define DEC_DRIVER_ADDRESS 0b01  // Set by MS1/MS2 (MS1 HIGH, MS2 LOW)
#endif

// DRIVER_TYPE_TMC2209_UART requires 4 digital pins in Arduino pin numbering
#ifndef AZ_STEP_PIN
    #define AZ_STEP_PIN 14  // STEP
#endif
#ifndef AZ_DIR_PIN
    #define AZ_DIR_PIN 12  // DIR
#endif
#ifndef AZ_EN_PIN
    #define AZ_EN_PIN 25  // Enable
#endif

// DRIVER_TYPE_TMC2209_UART requires 2 additional digital pins for SoftwareSerial, can be shared across all drivers
#ifndef AZ_SERIAL_PORT
    #define AZ_SERIAL_PORT Serial1
#endif
#ifndef AZ_DRIVER_ADDRESS
    #define AZ_DRIVER_ADDRESS 0b10
#endif
// DRIVER_TYPE_TMC2209_UART requires 4 digital pins in Arduino pin numbering
#ifndef ALT_STEP_PIN
    #define ALT_STEP_PIN 16  // STEP
#endif
#ifndef ALT_DIR_PIN
    #define ALT_DIR_PIN 17  // DIR
#endif
#ifndef ALT_EN_PIN
    #define ALT_EN_PIN 25  // Enable
#endif

// DRIVER_TYPE_TMC2209_UART requires 2 additional digital pins for SoftwareSerial, can be shared across all drivers
#ifndef ALT_SERIAL_PORT
    #define ALT_SERIAL_PORT Serial1
#endif
#ifndef ALT_DRIVER_ADDRESS
    #define ALT_DRIVER_ADDRESS 0b11
#endif

#define SW_SERIAL_UART 0

//Serial port for external debugging
#if DEBUG_SEPARATE_SERIAL == 1
    #ifndef DEBUG_SERIAL_PORT
        #error "There is no default separate serial port for ESP32, please define DEBUG_SERIAL_PORT"
    #endif
#else
    #ifndef DEBUG_SERIAL_PORT
        #define DEBUG_SERIAL_PORT Serial
    #endif
#endif
