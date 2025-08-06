/**
 * @brief a pins configuration file for an OAE board v1.0
 */

#pragma once

/**
 * @brief a pins configuration file for an ESP32-based OAT.
 */

#pragma once

// DRIVER_TYPE_TMC2209_UART requires 4 digital pins in Arduino pin numbering
#ifndef RA_STEP_PIN
    #define RA_STEP_PIN 14  // STEP
#endif
#ifndef RA_DIR_PIN
    #define RA_DIR_PIN 26  // DIR
#endif
#ifndef RA_EN_PIN
    #define RA_EN_PIN 27  // Enable
#endif

// DRIVER_TYPE_TMC2209_UART HardwareSerial port, can be shared across all drivers
#ifndef RA_SERIAL_PORT
    #define RA_SERIAL_PORT Serial1
#endif
#ifndef RA_TX_PIN
    #define RA_TX_PIN 17
#endif
#ifndef RA_RX_PIN
    #define RA_RX_PIN 16
#endif
#ifndef RA_DRIVER_ADDRESS
    #define RA_DRIVER_ADDRESS 0b00  // Set by MS1/MS2. LOW/LOW in this case
#endif
// DRIVER_TYPE_TMC2209_UART requires 4 digital pins in Arduino pin numbering
#ifndef DEC_STEP_PIN
    #define DEC_STEP_PIN 25  // STEP
#endif
#ifndef DEC_DIR_PIN
    #define DEC_DIR_PIN 5  // DIR
#endif
#ifndef DEC_EN_PIN
    #define DEC_EN_PIN 33  // Enable
#endif

// DRIVER_TYPE_TMC2209_UART HardwareSerial port, can be shared across all drivers
#ifndef DEC_SERIAL_PORT
    #define DEC_SERIAL_PORT Serial1  // SoftwareSerial TX port
#endif
#ifndef DEC_TX_PIN
    #define DEC_TX_PIN 17
#endif
#ifndef DEC_RX_PIN
    #define DEC_RX_PIN 16
#endif
#ifndef DEC_DRIVER_ADDRESS
    #define DEC_DRIVER_ADDRESS 0b01  // Set by MS1/MS2 (MS1 HIGH, MS2 LOW)
#endif

#define SW_SERIAL_UART 0

#ifndef ALT_STEP_PIN
    #define ALT_STEP_PIN 13  // STEP
#endif
#ifndef ALT_DIR_PIN
    #define ALT_DIR_PIN 23  // DIR
#endif
#ifndef ALT_EN_PIN
    #define ALT_EN_PIN 4  // Enable
#endif

#ifndef AZ_STEP_PIN
    #define AZ_STEP_PIN 18  // STEP
#endif
#ifndef AZ_DIR_PIN
    #define AZ_DIR_PIN 19  // DIR
#endif
#ifndef AZ_EN_PIN
    #define AZ_EN_PIN 32  // Enable
#endif

// DISPLAY_TYPE_LCD_JOY_I2C_SSD1306 requires 3 analog inputs in Arduino pin numbering
#ifndef LCD_KEY_SENSE_X_PIN
    //#define LCD_KEY_SENSE_X_PIN 34
#endif
#ifndef LCD_KEY_SENSE_Y_PIN
    //#define LCD_KEY_SENSE_Y_PIN 39
#endif
#ifndef LCD_KEY_SENSE_PUSH_PIN
    //#define LCD_KEY_SENSE_PUSH_PIN 36
#endif

//Serial port for external debugging
#if DEBUG_SEPARATE_SERIAL == 1
    #ifndef DEBUG_SERIAL_PORT
        #error "There is no default separate serial port for ESP32, please define DEBUG_SERIAL_PORT"
    #endif
#else
    #ifndef DEBUG_SERIAL_PORT
        #define DEBUG_SERIAL_PORT Serial2
    #endif
#endif


// Defines for OAE ///////////////////////

#ifndef RA_WHEEL_CIRCUMFERENCE
    #define RA_WHEEL_CIRCUMFERENCE 704.97f
#endif
#ifndef DEC_TRANSMISSION
    #define DEC_TRANSMISSION (DEC_WHEEL_CIRCUMFERENCE / (DEC_PULLEY_TEETH * 1.0))
#endif
#ifndef RA_LIMIT_LEFT
    #define RA_LIMIT_LEFT 5.0f
#endif
#ifndef RA_LIMIT_RIGHT
    #define RA_LIMIT_RIGHT 7.0f
#endif
#ifndef RA_TRACKING_LIMIT
    #define RA_TRACKING_LIMIT 6.75f
#endif
#ifndef DEC_WHEEL_CIRCUMFERENCE
    #define DEC_WHEEL_CIRCUMFERENCE 1.0f
#endif
#ifndef RA_STEPPER_SPR
    #define RA_STEPPER_SPR (400 * 9)   // change to (200 * 9) for 1.8° stepper
#endif
#ifndef DEC_STEPPER_SPR
    #define DEC_STEPPER_SPR (200 * 50 * 4.5f)  // change to (200 * 9) for 1.8° stepper
#endif
#ifndef DEC_PULLEY_TEETH
    #define DEC_PULLEY_TEETH 1
#endif