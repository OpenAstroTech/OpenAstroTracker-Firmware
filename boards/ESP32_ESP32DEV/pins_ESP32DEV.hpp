/**
 * @brief a pins configuration file for an ESP32-based OAT.
 */

#pragma once

// DRIVER_TYPE_ULN2003 requires 4 digital outputs in Arduino pin numbering
#ifndef RA_IN1_PIN
  #define RA_IN1_PIN  13  
#endif
#ifndef RA_IN2_PIN
  #define RA_IN2_PIN  12  
#endif
#ifndef RA_IN3_PIN
  #define RA_IN3_PIN  14  
#endif
#ifndef RA_IN4_PIN
  #define RA_IN4_PIN  27  
#endif
#ifndef DEC_IN1_PIN
  #define DEC_IN1_PIN  26  
#endif
#ifndef DEC_IN2_PIN
  #define DEC_IN2_PIN  25  
#endif
#ifndef DEC_IN3_PIN
  #define DEC_IN3_PIN  33   
#endif
#ifndef DEC_IN4_PIN
  #define DEC_IN4_PIN  32  
#endif
// DRIVER_TYPE_TMC2209_UART requires 4 digital pins in Arduino pin numbering
#ifndef RA_STEP_PIN
  #define RA_STEP_PIN 19  // STEP
#endif
#ifndef RA_DIR_PIN
  #define RA_DIR_PIN  21  // DIR
#endif
#ifndef RA_EN_PIN
  #define RA_EN_PIN   22  // Enable
#endif
#ifndef RA_DIAG_PIN
  #define RA_DIAG_PIN 23  // only needed for autohome function
#endif
#ifndef RA_MS0_PIN
  #define RA_MS0_PIN  4
#endif
#ifndef RA_MS1_PIN
  #define RA_MS1_PIN  0
#endif
#ifndef RA_MS2_PIN
  #define RA_MS2_PIN  2
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
  #define DEC_STEP_PIN 16  // STEP
#endif
#ifndef DEC_DIR_PIN
  #define DEC_DIR_PIN  17  // DIR
#endif
#ifndef DEC_EN_PIN
  #define DEC_EN_PIN   5  // Enable
#endif
#ifndef DEC_DIAG_PIN
  #define DEC_DIAG_PIN 18  // only needed for autohome function
#endif
#ifndef DEC_MS0_PIN
  #define DEC_MS0_PIN  15
#endif
#ifndef DEC_MS1_PIN
  #define DEC_MS1_PIN  8
#endif
#ifndef DEC_MS2_PIN
  #define DEC_MS2_PIN  7
#endif
// DRIVER_TYPE_TMC2209_UART HardwareSerial port, can be shared across all drivers
#ifndef DEC_SERIAL_PORT
  #define DEC_SERIAL_PORT Serial2 // SoftwareSerial TX port
#endif
#ifndef DEC_DRIVER_ADDRESS
  #define DEC_DRIVER_ADDRESS 0b01  // Set by MS1/MS2 (MS1 HIGH, MS2 LOW)
#endif

#define SW_SERIAL_UART 0

// DISPLAY_TYPE_LCD_JOY_I2C_SSD1306 requires 3 analog inputs in Arduino pin numbering
#ifndef LCD_KEY_SENSE_X_PIN
  #define LCD_KEY_SENSE_X_PIN 34
#endif
#ifndef LCD_KEY_SENSE_Y_PIN
  #define LCD_KEY_SENSE_Y_PIN 39
#endif
#ifndef LCD_KEY_SENSE_PUSH_PIN
  #define LCD_KEY_SENSE_PUSH_PIN 36
#endif
