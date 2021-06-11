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

/**
 * Use default values for any parameters the user didn't provide.
 */

// Uncomment the definition and set a board (see Constants.hpp for valid values) if you build in Arduino IDE.
// PlatformIO will set this value automatically and no action is needed.
#ifndef BOARD
// #define BOARD BOARD_AVR_MEGA2560
#endif

#ifndef BOARD
    #error You have to specify the board
#endif

// Include the user-specific local configuration
#include "LocalConfiguration.hpp"

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
    #define RA_STEPPER_TYPE STEPPER_TYPE_28BYJ48
#endif
#ifndef DEC_STEPPER_TYPE
    #define DEC_STEPPER_TYPE STEPPER_TYPE_28BYJ48
#endif
#ifndef AZ_STEPPER_TYPE
    #define AZ_STEPPER_TYPE STEPPER_TYPE_NONE
#endif
#ifndef ALT_STEPPER_TYPE
    #define ALT_STEPPER_TYPE STEPPER_TYPE_NONE
#endif
#ifndef FOCUS_STEPPER_TYPE
    #define FOCUS_STEPPER_TYPE STEPPER_TYPE_NONE
#endif

/**
 * @brief Stepper driver type in use on each axis.
 * See Constants.hpp for supported DRIVER_TYPE options.
 */
#ifndef RA_DRIVER_TYPE
    #define RA_DRIVER_TYPE DRIVER_TYPE_ULN2003
#endif
#ifndef DEC_DRIVER_TYPE
    #define DEC_DRIVER_TYPE DRIVER_TYPE_ULN2003
#endif
#ifndef AZ_DRIVER_TYPE
    #define AZ_DRIVER_TYPE DRIVER_TYPE_NONE
#endif
#ifndef ALT_DRIVER_TYPE
    #define ALT_DRIVER_TYPE DRIVER_TYPE_NONE
#endif
#ifndef FOCUS_DRIVER_TYPE
    #define FOCUS_DRIVER_TYPE DRIVER_TYPE_NONE
#endif

// Your pulley tooth count. 16 for the bought (aluminium) one, 20 for the printed one.
#ifndef RA_PULLEY_TEETH
    #define RA_PULLEY_TEETH 16
#endif
#ifndef DEC_PULLEY_TEETH
    #define DEC_PULLEY_TEETH 16
#endif
#ifndef AZ_PULLEY_TEETH
    #define AZ_PULLEY_TEETH 16
#endif
#ifndef ALT_PULLEY_TEETH
    #define ALT_PULLEY_TEETH 16
#endif

// Set these factors to correct Alt/Az arcsecond/step values
#ifndef AZ_CORRECTION_FACTOR
    #define AZ_CORRECTION_FACTOR 1.0000f
#endif
#ifndef ALT_CORRECTION_FACTOR
    #define ALT_CORRECTION_FACTOR 1.0000f
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
    #define POLARIS_RA_HOUR 2
#endif
#ifndef POLARIS_RA_MINUTE
    #define POLARIS_RA_MINUTE 58
#endif
#ifndef POLARIS_RA_SECOND
    #define POLARIS_RA_SECOND 34
#endif

// Set this to specify the amount of debug output OAT should send to the serial port.
// Note that if you use an app to control OAT, ANY debug output will likely confuse that app.
// Debug output is useful if you are using Wifi to control the OAT or if you are issuing
// manual commands via a terminal.
#ifndef DEBUG_LEVEL
    #define DEBUG_LEVEL (DEBUG_NONE)
#endif

// Append board specific pins data.
#if (BOARD == BOARD_AVR_MEGA2560)
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

#include "Configuration_adv.hpp"
#include "ConfigurationValidation.hpp"