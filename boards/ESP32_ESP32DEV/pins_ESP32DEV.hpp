/**
 * @brief a pins configuration file for an ESP32-based OAT.
 */

#pragma once

#include "Constants.hpp"

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
