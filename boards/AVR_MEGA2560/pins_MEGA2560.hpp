/**
 * @brief a pins configuration file for an MEGA2560 OAT.
 */

#pragma once

// DRIVER_TYPE_TMC2209_UART requires 4 digital pins in Arduino pin numbering
#ifndef RA_STEP_PIN
    #define RA_STEP_PIN 22  // STEP
#endif
#ifndef RA_DIR_PIN
    #define RA_DIR_PIN 24  // DIR
#endif
#ifndef RA_EN_PIN
    #define RA_EN_PIN 26  // Enable
#endif
#ifndef RA_DIAG_PIN
    #define RA_DIAG_PIN 28  // only needed for autohome function
#endif
#ifndef RA_MS0_PIN
    #define RA_MS0_PIN 30
#endif
#ifndef RA_MS1_PIN
    #define RA_MS1_PIN 32
#endif
#ifndef RA_MS2_PIN
    #define RA_MS2_PIN 34
#endif
// DRIVER_TYPE_TMC2209_UART HardwareSerial port, can be shared across all drivers
#ifndef RA_SERIAL_PORT
    #define RA_SERIAL_PORT Serial3
#endif
#ifndef RA_DRIVER_ADDRESS
    #define RA_DRIVER_ADDRESS 0b00  // Set by MS1/MS2. LOW/LOW in this case
#endif
// DRIVER_TYPE_TMC2209_UART requires 4 digital pins in Arduino pin numbering
#ifndef DEC_STEP_PIN
    #define DEC_STEP_PIN 23  // STEP
#endif
#ifndef DEC_DIR_PIN
    #define DEC_DIR_PIN 25  // DIR
#endif
#ifndef DEC_EN_PIN
    #define DEC_EN_PIN 27  // Enable
#endif
#ifndef DEC_DIAG_PIN
    #define DEC_DIAG_PIN 29  // only needed for autohome function
#endif
#ifndef DEC_MS0_PIN
    #define DEC_MS0_PIN 31
#endif
#ifndef DEC_MS1_PIN
    #define DEC_MS1_PIN 33
#endif
#ifndef DEC_MS2_PIN
    #define DEC_MS2_PIN 35
#endif
// DRIVER_TYPE_TMC2209_UART HardwareSerial port, can be shared across all drivers
#ifndef DEC_SERIAL_PORT
    #define DEC_SERIAL_PORT Serial3
#endif
#ifndef DEC_DRIVER_ADDRESS
    #define DEC_DRIVER_ADDRESS 0b01  // Set by MS1/MS2 (MS1 HIGH, MS2 LOW)
#endif

#define SW_SERIAL_UART 0

// DRIVER_TYPE_TMC2209_UART requires 2 additional digital pins for SoftwareSerial, can be shared across all drivers
#ifndef FOCUS_STEP_PIN
    #define FOCUS_STEP_PIN 32
#endif
#ifndef FOCUS_DIR_PIN
    #define FOCUS_DIR_PIN 33
#endif
#ifndef FOCUS_EN_PIN
    #define FOCUS_EN_PIN 34
#endif
#ifndef FOCUS_SERIAL_PORT
    #define FOCUS_SERIAL_PORT Serial3  // SoftwareSerial TX port
#endif

#ifndef FOCUS_DRIVER_ADDRESS
    #define FOCUS_DRIVER_ADDRESS 0b10  // Set by MS1/MS2 (MS1 HIGH, MS2 LOW)
#endif

// RA Homing pin for Hall sensor
#ifndef RA_HOMING_SENSOR_PIN
    #define RA_HOMING_SENSOR_PIN 40
#endif

//GPS pin configuration
#ifndef GPS_SERIAL_PORT
    #define GPS_SERIAL_PORT Serial1
#endif

// DISPLAY_TYPE_LCD_KEYPAD requires 6 digital & 1 analog output in Arduino pin numbering
#ifndef LCD_BRIGHTNESS_PIN
    #define LCD_BRIGHTNESS_PIN 10  // Brightness PWM control (#undef to disable)
#endif
#ifndef LCD_PIN4
    #define LCD_PIN4 4  // LCD DB4 pin
#endif
#ifndef LCD_PIN5
    #define LCD_PIN5 5  // LCD DB5 pin
#endif
#ifndef LCD_PIN6
    #define LCD_PIN6 6  // LCD DB6 pin
#endif
#ifndef LCD_PIN7
    #define LCD_PIN7 7  // LCD DB7 pin
#endif
#ifndef LCD_PIN8
    #define LCD_PIN8 8  // LCD RS pin
#endif
#ifndef LCD_PIN9
    #define LCD_PIN9 9  // LCD ENABLE pin
#endif

// DISPLAY_TYPE_LCD_KEYPAD requires 1 analog input in Arduino pin numbering
#ifndef LCD_KEY_SENSE_PIN
    #define LCD_KEY_SENSE_PIN 0  // Analog input for shield keys (#undef to disable)
#endif
