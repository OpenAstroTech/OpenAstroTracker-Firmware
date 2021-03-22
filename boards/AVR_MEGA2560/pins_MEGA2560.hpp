/**
 * @brief a pins configuration file for an MEGA2560 OAT.
 */

#pragma once

// DRIVER_TYPE_ULN2003 requires 4 digital outputs in Arduino pin numbering
#ifndef RA_IN1_PIN
  #define RA_IN1_PIN  22
#endif
#ifndef RA_IN2_PIN
  #define RA_IN2_PIN  24
#endif
#ifndef RA_IN3_PIN
  #define RA_IN3_PIN  26
#endif
#ifndef RA_IN4_PIN
  #define RA_IN4_PIN  28
#endif
#ifndef DEC_IN1_PIN
  #define DEC_IN1_PIN 30
#endif
#ifndef DEC_IN2_PIN
  #define DEC_IN2_PIN 32
#endif
#ifndef DEC_IN3_PIN
  #define DEC_IN3_PIN 34
#endif
#ifndef DEC_IN4_PIN
  #define DEC_IN4_PIN 36
#endif
// DRIVER_TYPE_TMC2209_UART requires 4 digital pins in Arduino pin numbering
#ifndef RA_STEP_PIN
  #define RA_STEP_PIN 22  // STEP
#endif
#ifndef RA_DIR_PIN
  #define RA_DIR_PIN  24  // DIR
#endif
#ifndef RA_EN_PIN
  #define RA_EN_PIN   26  // Enable
#endif
#ifndef RA_DIAG_PIN
  #define RA_DIAG_PIN 28  // only needed for autohome function
#endif
#ifndef RA_MS0_PIN
  #define RA_MS0_PIN  30
#endif
#ifndef RA_MS1_PIN
  #define RA_MS1_PIN  32
#endif
#ifndef RA_MS2_PIN
  #define RA_MS2_PIN  34
#endif    
// DRIVER_TYPE_TMC2209_UART HardwareSerial port, can be shared across all drivers
#ifndef RA_SERIAL_PORT
  #define RA_SERIAL_PORT Serial2
#endif
#ifndef RA_DRIVER_ADDRESS
  #define RA_DRIVER_ADDRESS 0b00  // Set by MS1/MS2. LOW/LOW in this case
#endif
// DRIVER_TYPE_TMC2209_UART requires 4 digital pins in Arduino pin numbering
#ifndef DEC_STEP_PIN
  #define DEC_STEP_PIN 23  // STEP
#endif
#ifndef DEC_DIR_PIN
  #define DEC_DIR_PIN  25  // DIR
#endif
#ifndef DEC_EN_PIN
  #define DEC_EN_PIN   27  // Enable
#endif
#ifndef DEC_DIAG_PIN
  #define DEC_DIAG_PIN 29  // only needed for autohome function
#endif
#ifndef DEC_MS0_PIN
  #define DEC_MS0_PIN  31
#endif
#ifndef DEC_MS1_PIN
  #define DEC_MS1_PIN  33
#endif
#ifndef DEC_MS2_PIN
  #define DEC_MS2_PIN  35
#endif
// DRIVER_TYPE_TMC2209_UART HardwareSerial port, can be shared across all drivers
#ifndef DEC_SERIAL_PORT
  #define DEC_SERIAL_PORT Serial2
#endif
#ifndef DEC_DRIVER_ADDRESS
  #define DEC_DRIVER_ADDRESS 0b01  // Set by MS1/MS2 (MS1 HIGH, MS2 LOW)
#endif

#define SW_SERIAL_UART 0

// DRIVER_TYPE_ULN2003 requires 4 digital outputs in Arduino pin numbering
#ifndef AZ_IN1_PIN
  #define AZ_IN1_PIN 47
#endif
#ifndef AZ_IN2_PIN
  #define AZ_IN2_PIN 49
#endif
#ifndef AZ_IN3_PIN
  #define AZ_IN3_PIN 51
#endif
#ifndef AZ_IN4_PIN
  #define AZ_IN4_PIN 53
#endif
#ifndef ALT_IN1_PIN
  #define ALT_IN1_PIN 46
#endif
#ifndef ALT_IN2_PIN
  #define ALT_IN2_PIN 48
#endif
#ifndef ALT_IN3_PIN
  #define ALT_IN3_PIN 50
#endif
#ifndef ALT_IN4_PIN
  #define ALT_IN4_PIN 52
#endif

//GPS pin configuration
#ifndef GPS_SERIAL_PORT
  #define GPS_SERIAL_PORT Serial1
#endif

// DISPLAY_TYPE_LCD_KEYPAD requires 6 digital & 1 analog output in Arduino pin numbering
#ifndef LCD_BRIGHTNESS_PIN
  #define LCD_BRIGHTNESS_PIN 10   // Brightness PWM control (#undef to disable)
#endif
#ifndef LCD_PIN4
  #define LCD_PIN4 4              // LCD DB4 pin
#endif
#ifndef LCD_PIN5
  #define LCD_PIN5 5              // LCD DB5 pin
#endif
#ifndef LCD_PIN6
  #define LCD_PIN6 6              // LCD DB6 pin
#endif
#ifndef LCD_PIN7
  #define LCD_PIN7 7              // LCD DB7 pin
#endif
#ifndef LCD_PIN8
  #define LCD_PIN8 8              // LCD RS pin
#endif
#ifndef LCD_PIN9
  #define LCD_PIN9 9              // LCD ENABLE pin
#endif

// DISPLAY_TYPE_LCD_KEYPAD requires 1 analog input in Arduino pin numbering
#ifndef LCD_KEY_SENSE_PIN
  #define LCD_KEY_SENSE_PIN 0     // Analog input for shield keys (#undef to disable)
#endif
