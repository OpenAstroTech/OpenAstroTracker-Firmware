/**
* Configuration accumulates in order across 3 files: Configuration_local.hpp, Configuration.hpp, and Configuration_adv.hpp.
*  - Configuration_local.hpp (see below) captures the local (primarily hardware) configuration.
*  - Configuration.hpp (this file) adds reasonable default values for any data missing in Configuration_local.hpp.
*  - Configuration_adv.hpp holds advanced configuration data, and is not usually required to be changed.
* 
* There should be no user-specified parameters in this file - everything should be specified in Configuration_local.hpp
*
* We support local configurations so that you can setup it up once with your hardware and pinout
* and not need to worry about a new version from Git overwriting your setup. 
* There are multiple ways to define a local config file:
*  - For all boards/hardware configs:
*    Create a file called Configuration_local.hpp (best to copy configuration_sample_local.hpp and 
*    change it as needed)
*  - Specific to a board:
*    Create a file called Configuration_local_<board>.hpp, where <board> is either 'mega' or 
*    'esp32' or (here, too, best to copy Configuration_sample_local.hpp and change it as needed). 
*    The code automatically picks the right one at compile time. This is useful if you are 
*    developer or just have multiple OATs. 
*  - Custom configurations or advanced builds:
*    Use Configuration_local.hpp to include an appropriate local configuration file e.g. according to 
*    preprocessor directives specified on the command line. This is useful if you have multiple OATs, or wish 
*    to build the software across many software configurations on the same platform.
* 
* These files won't be tracked by Git and thus will remain after branch changes or code updates. 
*/
#pragma once

// Include named constants for various features
#include "Constants.hpp"

// Include the current software version.
#include "Version.h"

// Include the user-specific local configuration
#if __has_include("Configuration_local.hpp")
    #include "Configuration_local.hpp"
#elif __has_include("Configuration_local_CI.hpp")                                   // CI environment on GitHub
    #include "Configuration_local_CI.hpp"
#endif

/**
 * Use default values for any parameters the user didn't provide.
 */

// If board is not defined in local configuration, assume BOARD_UNKNOWN
// Configuration checks below will capture any missing pin assignments.
#ifndef BOARD
  #define BOARD BOARD_UNKNOWN
#endif

// Set to 1 for the northern hemisphere, 0 otherwise
#ifndef NORTHERN_HEMISPHERE
#define NORTHERN_HEMISPHERE 1
#endif

/**
 * @brief Display & keypad configuration.
 * See Constants.hpp for supported DISPLAY_TYPE options.
 * Pin assignments vary based on display & keypad selection.
 */
#ifndef DISPLAY_TYPE
#define DISPLAY_TYPE DISPLAY_TYPE_NONE
#endif

// Used RA wheel version. Unless you printed your OAT before March 2020, you're using 
// a version 2 or higher (software only differentiates between 1 and more than 1)
#ifndef RA_WHEEL_VERSION
#define RA_WHEEL_VERSION 4
#endif

/**
 * @brief Stepper motor type in use on each axis.
 * See Constants.hpp for supported options.
 */
#ifndef RA_STEPPER_TYPE
#define RA_STEPPER_TYPE     STEPPER_TYPE_28BYJ48
#endif
#ifndef DEC_STEPPER_TYPE
#define DEC_STEPPER_TYPE    STEPPER_TYPE_28BYJ48
#endif
#ifndef AZ_STEPPER_TYPE
#define AZ_STEPPER_TYPE    STEPPER_TYPE_28BYJ48
#endif
#ifndef ALT_STEPPER_TYPE
#define ALT_STEPPER_TYPE    STEPPER_TYPE_28BYJ48
#endif

/**
 * @brief Stepper driver type in use on each axis.
 * See Constants.hpp for supported DRIVER_TYPE options.
 */
#ifndef RA_DRIVER_TYPE
#define RA_DRIVER_TYPE      DRIVER_TYPE_ULN2003
#endif
#ifndef DEC_DRIVER_TYPE
#define DEC_DRIVER_TYPE     DRIVER_TYPE_ULN2003
#endif
#ifndef AZ_DRIVER_TYPE
#define AZ_DRIVER_TYPE     DRIVER_TYPE_ULN2003
#endif
#ifndef ALT_DRIVER_TYPE
#define ALT_DRIVER_TYPE     DRIVER_TYPE_ULN2003
#endif

// Your pulley tooth count. 16 for the bought (aluminium) one, 20 for the printed one.
#ifndef RA_PULLEY_TEETH
#define RA_PULLEY_TEETH     16
#endif
#ifndef DEC_PULLEY_TEETH
#define DEC_PULLEY_TEETH    16
#endif
#ifndef AZ_PULLEY_TEETH
#define AZ_PULLEY_TEETH    16
#endif
#ifndef ALT_PULLEY_TEETH
#define ALT_PULLEY_TEETH    16
#endif

// Set these factors to correct Alt/Az arcsecond/step values
#ifndef AZ_CORRECTION_FACTOR
#define AZ_CORRECTION_FACTOR    1.0000
#endif
#ifndef ALT_CORRECTION_FACTOR
#define ALT_CORRECTION_FACTOR    1.0000
#endif

/**
 * @brief GPS receiver configuration.
 * Set USE_GPS to 1 to enable, 0 or #undef to exclude GPS from configuration.
 * On ATmega GPS uses hardware Serial1. No additional pins required. Change in configuration_adv.hpp
 * On ESP32 GPS uses hardware Serial2. No additional pins required. Change in configuration_adv.hpp
 * Note the potential serial port assignment conflict if stepper driver DRIVER_TYPE_TMC2209_UART is used.
 */
#ifndef USE_GPS
#define USE_GPS 0
#endif
#ifndef GPS_BAUD_RATE
#define GPS_BAUD_RATE 9600
#endif

/**
 * @brief External (USB) serial port configuration.
 * See Constants.hpp for predefined SERIAL_BAUDRATE options, or customize as required.
 */
#ifndef SERIAL_BAUDRATE
#define SERIAL_BAUDRATE SERIAL_BAUDRATE_ASCOM
#endif

/**
 * @brief Wifi configuration.
 * Wifi is only supported on esp32.
 * Set WIFI_ENABLED to 1 to enable, 0 or #undef to exclude Wifi from configuration.
 * If Wifi is enabled then the WIFI_MODE and WIFI_HOSTNAME must be set.
 * Requirements for WIFI_MODE:
 *  WIFI_MODE_DISABLED (i.e. Wifi transceiver disabled)
 *      No additional requirements.
 *  WIFI_MODE_INFRASTRUCTURE (i.e. connect OAT to existing Wifi network):
 *      WIFI_INFRASTRUCTURE_MODE_SSID & WIFI_INFRASTRUCTURE_MODE_WPAKEY must be set.
 *  WIFI_MODE_AP_ONLY (i.e. set OAT as Wifi hotspot): 
 *      WIFI_AP_MODE_WPAKEY must be set.
 *  WIFI_MODE_ATTEMPT_INFRASTRUCTURE_FAIL_TO_AP (i.e. try WIFI_MODE_INFRASTRUCTURE, fall back to WIFI_MODE_AP_ONLY):
 *      Requirements for both WIFI_MODE_INFRASTRUCTURE and WIFI_MODE_AP_ONLY must be satisfied.
 * WIFI_INFRASTRUCTURE_MODE_WPAKEY & WIFI_AP_MODE_WPAKEY must not be shorter than 8 characters and not 
 * longer than 32 characters. Do not use special characters or white spaces in the password (esp32 limitation).
 * Note that enabling Wifi increases flash usage by about 420 kB.
 */
#ifndef WIFI_ENABLED
  #define WIFI_ENABLED 0
#endif
#ifndef WIFI_MODE
  #define WIFI_MODE WIFI_MODE_DISABLED
#endif
#if !defined(WIFI_HOSTNAME)
  #define WIFI_HOSTNAME "OAT"
#endif
#if !defined(WIFI_INFRASTRUCTURE_MODE_SSID)
  #define WIFI_INFRASTRUCTURE_MODE_SSID ""
#endif
#if !defined(WIFI_INFRASTRUCTURE_MODE_WPAKEY)
  #define WIFI_INFRASTRUCTURE_MODE_WPAKEY ""
#endif
#if !defined(WIFI_AP_MODE_WPAKEY)
  #define WIFI_AP_MODE_WPAKEY ""
#endif

/**
 * @brief Bluetooth configuration.
 * Bluetooth is only supported on esp32.
 * Set BLUETOOTH_ENABLED to 1 to enable, 0 or #undef to exclude Bluetooth from configuration.
 * If Bluetooth is enabled then the BLUETOOTH_DEVICE_NAME must be set.
 * Note that enabling Bluetooth increases flash usage by about 627 kB.
 */
#ifndef BLUETOOTH_ENABLED
  #define BLUETOOTH_ENABLED 0
#endif

/**
 * @brief Gyro-based tilt/roll levelling configuration.
 * Set USE_GYRO_LEVEL to 1 to enable, 0 or #undef to exclude gyro from configuration.
 * On ATmega & ESP32 gyro uses hardware I2C. No additional pins required. 
 */
#ifndef USE_GYRO_LEVEL
#define USE_GYRO_LEVEL 0
#endif

// Set this to 1 if your gyro is mounted such that roll and pitch are in the wrong direction
#ifndef GYRO_AXIS_SWAP
#define GYRO_AXIS_SWAP 1
#endif

/**
 * @brief Automated azimuth/altitude adjustment configuration.
 * Set AZIMUTH_ALTITUDE_MOTORS to 1 to enable, 0 or #undef to exclude AZ/ALT from configuration.
 */
#ifndef AZIMUTH_ALTITUDE_MOTORS
#define AZIMUTH_ALTITUDE_MOTORS  0
#endif

// Enable dew heater output (for boards that have MOSFETs)
#ifndef DEW_HEATER
#define DEW_HEATER 0
#endif

// These values are needed to calculate the current position during initial alignment.
// Use something like Stellarium to look up the RA of Polaris in JNow (on date) variant.
// This changes slightly over weeks, so adjust every couple of months.
// This value is from 13.Aug.2020, next adjustment suggested at end 2020
// The same could be done for the DEC coordinates but they dont change significantly for the next 5 years
#ifndef POLARIS_RA_HOUR
#define POLARIS_RA_HOUR     2
#endif
#ifndef POLARIS_RA_MINUTE
#define POLARIS_RA_MINUTE   58
#endif
#ifndef POLARIS_RA_SECOND
#define POLARIS_RA_SECOND   34
#endif

// Set this to specify the amount of debug output OAT should send to the serial port.
// Note that if you use an app to control OAT, ANY debug output will likely confuse that app.
// Debug output is useful if you are using Wifi to control the OAT or if you are issuing
// manual commands via a terminal.
#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL (DEBUG_NONE)
#endif

// Append board specific pins data.
#if (BOARD == BOARD_UNKNOWN)
  // Assume custom user board - no predefined pin mapping
#elif (BOARD == BOARD_AVR_MEGA2560)
  #include "boards/AVR_MEGA2560/pins_MEGA2560.hpp"
#elif (BOARD == BOARD_ESP32_ESP32DEV)
  #include "boards/ESP32_ESP32DEV/pins_ESP32DEV.hpp"
#elif (BOARD == BOARD_AVR_MKS_GEN_L_V1)
  #include "boards/AVR_MKS_GEN_L_V1/pins_MKS_GEN_L_V1.h"
#elif (BOARD == BOARD_AVR_MKS_GEN_L_V2)
  #include "boards/AVR_MKS_GEN_L_V2/pins_MKS_GEN_L_V2.h"
#elif (BOARD == BOARD_AVR_MKS_GEN_L_V21)
  #include "boards/AVR_MKS_GEN_L_V21/pins_MKS_GEN_L_V21.h"
#endif

// Append the advanced configuration data.
#include "Configuration_adv.hpp"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                            ////////
// VALIDATE CONFIGURATION     ////////
//                            ////////
//////////////////////////////////////

// Platform
#if defined(ESP32) || defined(__AVR_ATmega2560__)
  // Valid platform
#else
  #error Unsupported platform configuration. Use at own risk.
#endif

// Display & keypad configurations
#if defined(ESP32) && ((DISPLAY_TYPE == DISPLAY_TYPE_NONE) || (DISPLAY_TYPE == DISPLAY_TYPE_LCD_JOY_I2C_SSD1306))
  // Valid display for ESP32
#elif defined(__AVR_ATmega2560__) && ((DISPLAY_TYPE == DISPLAY_TYPE_NONE) || (DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD) \
  || (DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23008) || (DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23017))
  // Valid display for ATmega
#else
  #error Unsupported display configuration. Use at own risk.
#endif

// Validate motor & driver configurations
#if (RA_STEPPER_TYPE == STEPPER_TYPE_28BYJ48) && (RA_DRIVER_TYPE == DRIVER_TYPE_ULN2003)
  // Valid RA stepper and driver combination
#elif (RA_STEPPER_TYPE == STEPPER_TYPE_NEMA17) && ((RA_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC) \
  || (RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE) || (RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART))
  // Valid RA stepper and driver combination
#else
  #error Unsupported RA stepper & driver combination. Use at own risk.
#endif

#if (DEC_STEPPER_TYPE == STEPPER_TYPE_28BYJ48) && (DEC_DRIVER_TYPE == DRIVER_TYPE_ULN2003)
  // Valid DEC stepper and driver combination
#elif (DEC_STEPPER_TYPE == STEPPER_TYPE_NEMA17) && ((DEC_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC) \
  || (DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE) || (DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART))
  // Valid DEC stepper and driver combination
#else
  #error Unsupported DEC stepper & driver combination. Use at own risk.
#endif

#if (RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
  #ifndef RA_DRIVER_ADDRESS
    // Serial bus address must be specified for TMC2209 in UART mode
    #error RA driver address for DRIVER_TYPE_TMC2209_UART not specified.
  #endif
#endif

#if (DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
  #ifndef DEC_DRIVER_ADDRESS
    // Serial bus address must be specified for TMC2209 in UART mode
    #error DEC driver address for DRIVER_TYPE_TMC2209_UART not specified.
  #endif
#endif

#if (AZIMUTH_ALTITUDE_MOTORS == 0)
  // Baseline configuration without azimuth & altitude control is valid
#elif defined(__AVR_ATmega2560__)
  // Azimuth configuration
  #if (AZ_STEPPER_TYPE == STEPPER_TYPE_28BYJ48) && (AZ_DRIVER_TYPE == DRIVER_TYPE_ULN2003)
    // Valid AZ stepper and driver combination
  #elif (AZ_STEPPER_TYPE == STEPPER_TYPE_28BYJ48) && (AZ_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC)
    // Valid AZ stepper and driver combination
  #elif (AZ_STEPPER_TYPE == STEPPER_TYPE_28BYJ48) && (AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE)
    // Valid AZ stepper and driver combination
  #elif (AZ_STEPPER_TYPE == STEPPER_TYPE_28BYJ48) && (AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
    // Valid AZ stepper and driver combination
  #elif (AZ_STEPPER_TYPE == STEPPER_TYPE_NEMA17) && (AZ_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC)
    // Valid AZ stepper and driver combination
  #elif (AZ_STEPPER_TYPE == STEPPER_TYPE_NEMA17) && (AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE)
    // Valid AZ stepper and driver combination
  #elif (AZ_STEPPER_TYPE == STEPPER_TYPE_NEMA17) && (AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
    // Valid AZ stepper and driver combination
  #else
    #error Unsupported AZ stepper & driver combination. Use at own risk.
  #endif

  // Altitude configuration
  #if (ALT_STEPPER_TYPE == STEPPER_TYPE_28BYJ48) && (ALT_DRIVER_TYPE == DRIVER_TYPE_ULN2003)
    // Valid ALT stepper and driver combination
  #elif (ALT_STEPPER_TYPE == STEPPER_TYPE_28BYJ48) && (ALT_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC)
    // Valid ALT stepper and driver combination
  #elif (ALT_STEPPER_TYPE == STEPPER_TYPE_28BYJ48) && (ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE)
    // Valid ALT stepper and driver combination
  #elif (ALT_STEPPER_TYPE == STEPPER_TYPE_28BYJ48) && (ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
    // Valid ALT stepper and driver combination
  #elif (ALT_STEPPER_TYPE == STEPPER_TYPE_NEMA17) && (ALT_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC)
    // Valid ALT stepper and driver combination
  #elif (ALT_STEPPER_TYPE == STEPPER_TYPE_NEMA17) && (ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE)
    // Valid ALT stepper and driver combination
  #elif (ALT_STEPPER_TYPE == STEPPER_TYPE_NEMA17) && (ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
    // Valid ALT stepper and driver combination
  #else
    #error Unsupported ALT stepper & driver combination. Use at own risk.
  #endif

  #if (AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
    #ifndef AZ_DRIVER_ADDRESS
      // Serial bus address must be specified for TMC2209 in UART mode
      #error AZ driver address for DRIVER_TYPE_TMC2209_UART not specified.
    #endif
  #endif

  #if (ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
    #ifndef ALT_DRIVER_ADDRESS
      // Serial bus address must be specified for TMC2209 in UART mode
      #error ALT driver address for DRIVER_TYPE_TMC2209_UART not specified.
    #endif
  #endif

#else
  #error Configuration does not support AZ/ALT. Use at own risk.
#endif 

// Interfaces
#if (BLUETOOTH_ENABLED == 0)
  // Baseline configuration without Bluetooth is valid
#elif defined(ESP32)
  // Bluetooth is only supported on ESP32
  #if !defined(BLUETOOTH_DEVICE_NAME)
    #error Bluetooth device name must be provided
  #endif
#else
  #error Unsupported Bluetooth configuration. Use at own risk.
#endif

#if (WIFI_ENABLED == 0)
  // Baseline configuration without WiFi is valid
#elif defined(ESP32)
  // Wifi is only supported on ESP32
  #if !defined(WIFI_HOSTNAME)
    #error Wifi hostname must be provided for infrastructure and AP modes
  #endif
  #if (WIFI_MODE == WIFI_MODE_DISABLED)
    // Baseline configuration with disabled WiFi is valid
  #endif
  #if (WIFI_MODE == WIFI_MODE_INFRASTRUCTURE) || (WIFI_MODE == WIFI_MODE_ATTEMPT_INFRASTRUCTURE_FAIL_TO_AP)
    #if !defined(WIFI_INFRASTRUCTURE_MODE_SSID) || !defined(WIFI_INFRASTRUCTURE_MODE_WPAKEY)
      #error Wifi SSID and WPA key must be provided for infrastructure mode
    #endif
  #elif (WIFI_MODE == WIFI_MODE_AP_ONLY) || (WIFI_MODE == WIFI_MODE_ATTEMPT_INFRASTRUCTURE_FAIL_TO_AP)
    #if !defined(WIFI_AP_MODE_WPAKEY)
      #error Wifi WPA key must be provided for AP mode
    #endif
  #else
    #error Unsupported WiFi configuration. Use at own risk.
  #endif
#else
  #error Unsupported WiFi configuration (WiFI only supported on ESP32). Use at own risk.
#endif

// External sensors
#if (USE_GPS == 0)
  // Baseline configuration without GPS is valid
#elif defined(ESP32) || defined(__AVR_ATmega2560__)
  // GPS is supported on ESP32 and ATmega
#else
  #error Unsupported GPS configuration. Use at own risk.
#endif

#if (USE_GYRO_LEVEL == 0)
  // Baseline configuration without gyro is valid
#elif defined(ESP32) || defined(__AVR_ATmega2560__)
  // Gyro is supported on ESP32 and ATmega
#else
  #error Unsupported gyro configuration. Use at own risk.
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                            ////////
// VALIDATE PIN ASSIGNMENTS   ////////
//                            ////////
//////////////////////////////////////

// Motor & driver configurations
#if (DEC_DRIVER_TYPE == DRIVER_TYPE_ULN2003)
  #if !defined(DEC_IN1_PIN) || !defined(DEC_IN2_PIN) || !defined(DEC_IN3_PIN) || !defined(DEC_IN4_PIN)
     // Required pin assignments missing
     #error Missing pin assignments for configured DEC DRIVER_TYPE_ULN2003 driver
  #endif
#elif (DEC_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC) || (DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE)
  #if !defined(DEC_STEP_PIN) || !defined(DEC_DIR_PIN) || !defined(DEC_EN_PIN)
     // Required pin assignments missing
     #error Missing pin assignments for configured DEC DRIVER_TYPE_A4988_GENERIC or DRIVER_TYPE_TMC2209_STANDALONE driver
  #endif
  #if !defined(DEC_MS0_PIN) || !defined(DEC_MS1_PIN) || !defined(DEC_MS2_PIN)
     #warning Missing pin assignments for MS pins
  #endif
#elif (DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
  #if !defined(DEC_STEP_PIN) || !defined(DEC_DIR_PIN) || !defined(DEC_EN_PIN) || !defined(DEC_DIAG_PIN)
     // Required pin assignments missing
     #error Missing pin assignments for configured DEC DRIVER_TYPE_TMC2209_UART driver
  #endif
  #if !((defined(DEC_SERIAL_PORT_TX) && defined(DEC_SERIAL_PORT_RX)) || defined(DEC_SERIAL_PORT))
     // Required pin assignments missing for UART serial
     #error Missing pin assignments for configured DEC DRIVER_TYPE_TMC2209_UART driver serial connection
  #endif
#endif

#if (RA_DRIVER_TYPE == DRIVER_TYPE_ULN2003)
  #if !defined(RA_IN1_PIN) || !defined(RA_IN2_PIN) || !defined(RA_IN3_PIN) || !defined(RA_IN4_PIN)
     // Required pin assignments missing
     #error Missing pin assignments for configured RA DRIVER_TYPE_ULN2003 driver
  #endif
#elif (RA_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC) || (RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE)
  #if !defined(RA_STEP_PIN) || !defined(RA_DIR_PIN) || !defined(RA_EN_PIN)
     // Required pin assignments missing
     #error Missing pin assignments for configured RA DRIVER_TYPE_A4988_GENERIC or DRIVER_TYPE_TMC2209_STANDALONE driver
  #endif
  #if !defined(RA_MS0_PIN) || !defined(RA_MS1_PIN) || !defined(RA_MS2_PIN)
     #warning Missing pin assignments for MS pins
  #endif
#elif (RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
  #if !defined(RA_STEP_PIN) || !defined(RA_DIR_PIN) || !defined(RA_EN_PIN) || !defined(RA_DIAG_PIN)
     // Required pin assignments missing 
     #error Missing pin assignments for configured RA DRIVER_TYPE_TMC2209_UART driver
  #endif
  #if !((defined(RA_SERIAL_PORT_TX) && defined(RA_SERIAL_PORT_RX)) || defined(RA_SERIAL_PORT))
     // Required pin assignments missing for UART serial
     #error Missing pin assignments for configured RA DRIVER_TYPE_TMC2209_UART driver serial connection
  #endif
#endif

#if (AZIMUTH_ALTITUDE_MOTORS == 1)
  #if (AZ_DRIVER_TYPE == DRIVER_TYPE_ULN2003)
    #if !defined(AZ_IN1_PIN) || !defined(AZ_IN2_PIN) || !defined(AZ_IN3_PIN) || !defined(AZ_IN4_PIN)
      // Required pin assignments missing
      #error Missing pin assignments for configured AZ DRIVER_TYPE_ULN2003 driver
    #endif
  #elif (AZ_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC) || (AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE)
    #if !defined(AZ_STEP_PIN) || !defined(AZ_DIR_PIN) || !defined(AZ_EN_PIN) || !defined(AZ_DIAG_PIN)
       // Required pin assignments missing
       #error Missing pin assignments for configured AZ DRIVER_TYPE_A4988_GENERIC or DRIVER_TYPE_TMC2209_STANDALONE driver
    #endif
  #elif (AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
    #if !defined(AZ_STEP_PIN) || !defined(AZ_DIR_PIN) || !defined(AZ_EN_PIN) || !defined(AZ_DIAG_PIN) || !defined(AZ_SERIAL_PORT_TX) || !defined(AZ_SERIAL_PORT_RX)
      // Required pin assignments missing (ATmega uses SoftwareSerial for this driver)
      #error Missing pin assignments for configured AZ DRIVER_TYPE_TMC2209_UART driver
    #endif
    #if !((defined(AZ_SERIAL_PORT_TX) && defined(AZ_SERIAL_PORT_RX)) || defined(AZ_SERIAL_PORT))
       // Required pin assignments missing for UART serial
       #error Missing pin assignments for configured AZ DRIVER_TYPE_TMC2209_UART driver serial connection
    #endif
  #endif

  #if (ALT_DRIVER_TYPE == DRIVER_TYPE_ULN2003)
    #if !defined(ALT_IN1_PIN) || !defined(ALT_IN2_PIN) || !defined(ALT_IN3_PIN) || !defined(ALT_IN4_PIN)
      // Required pin assignments missing
      #error Missing pin assignments for configured ALT DRIVER_TYPE_ULN2003 driver
    #endif
  #elif (ALT_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC) || (ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE)
    #if !defined(ALT_STEP_PIN) || !defined(ALT_DIR_PIN) || !defined(ALT_EN_PIN) || !defined(ALT_DIAG_PIN)
       // Required pin assignments missing
       #error Missing pin assignments for configured AZ DRIVER_TYPE_A4988_GENERIC or DRIVER_TYPE_TMC2209_STANDALONE driver
    #endif
  #elif (ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
    #if !defined(ALT_STEP_PIN) || !defined(ALT_DIR_PIN) || !defined(ALT_EN_PIN) || !defined(ALT_DIAG_PIN) || !defined(ALT_SERIAL_PORT_TX) || !defined(ALT_SERIAL_PORT_RX)
      // Required pin assignments missing (ATmega uses SoftwareSerial for this driver)
      #error Missing pin assignments for configured ALT DRIVER_TYPE_TMC2209_UART driver
    #endif
    #if !((defined(ALT_SERIAL_PORT_TX) && defined(ALT_SERIAL_PORT_RX)) || defined(ALT_SERIAL_PORT))
       // Required pin assignments missing for UART serial
       #error Missing pin assignments for configured ALT DRIVER_TYPE_TMC2209_UART driver serial connection
    #endif
  #endif
#endif

// Displays
#if (DISPLAY_TYPE == DISPLAY_TYPE_NONE) || (DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23008) || (DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23017)
  // No dedicated pins required apart from I2C
#elif (DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD)
  #if !defined(LCD_PIN4) || !defined(LCD_PIN5) || !defined(LCD_PIN6) || !defined(LCD_PIN7)  || !defined(LCD_PIN8) || !defined(LCD_PIN9)
     // Required pin assignments missing
     #error Missing pin assignments for configured DISPLAY_TYPE_LCD_KEYPAD display
  #endif
  #if !defined(LCD_BRIGHTNESS_PIN)
     #warning Missing pin assignment for DISPLAY_TYPE_LCD_KEYPAD brightness pin
  #endif
#elif (DISPLAY_TYPE == DISPLAY_TYPE_LCD_JOY_I2C_SSD1306)
  // No dedicated pins required apart from I2C for display
#endif

// Keypad
#if (DISPLAY_TYPE == DISPLAY_TYPE_NONE) || (DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23008) || (DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23017)
  // No dedicated pins required apart from I2C
#elif (DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD)
  #if !defined(LCD_KEY_SENSE_PIN)
     // Required pin assignments missing
     #error Missing sense pin assignment for configured DISPLAY_TYPE_LCD_KEYPAD keypad
  #endif
#elif (DISPLAY_TYPE == DISPLAY_TYPE_LCD_JOY_I2C_SSD1306)
  #if !defined(LCD_KEY_SENSE_X_PIN) || !defined(LCD_KEY_SENSE_Y_PIN) || !defined(LCD_KEY_SENSE_PUSH_PIN)
     // Required pin assignments missing
     #error Missing sense pin assignments for configured DISPLAY_TYPE_LCD_JOY_I2C_SSD1306 joystick
  #endif
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                ////////
// VALIDATE CRITICAL PARAMETERS   ////////
//                                ////////
//////////////////////////////////////////

#if (DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART) && (DEC_HOLDCURRENT < 1 || DEC_HOLDCURRENT > 31)
  #error "Holdcurrent has to be within 1 and 31!"
#endif

#if (DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART) && (DEC_RMSCURRENT > 2000)
  #error "Do you really want to set the RMS motorcurrent above 2 Ampere? Thats almost 3A peak! Delete this error if you know what youre doing" 
#endif

#if (RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART) && (RA_RMSCURRENT > 2000)
  #error "Do you really want to set the RMS motorcurrent above 2 Ampere? Thats almost 3A peak! Delete this error if you know what youre doing" 
#endif
