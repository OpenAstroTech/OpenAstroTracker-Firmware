/**
 * @brief an example configuration file for an ESP32-based OAT.
 * 
 * This configuration includes NEMA17 steppers on RA & DEC axes with TMC2209_UART drivers, 
 * GPS, gyro levelling. Display is Arduino LCD shield with keypad. 
 * Azimith/Altitude motors are also configured as 28BYJ48 with ULN2003 drivers.
 * USB serial port is set for direct connection to Stellarium.
 * Wifi and Bluetooth is not available on ATmega, so is disabled.
 */

#pragma once

// Set to 1 for the northern hemisphere, 0 otherwise
#define NORTHERN_HEMISPHERE 1

// Used RA wheel version. Unless you printed your OAT before March 2020, you're using 
// a version 2 or higher (software only differentiates between 1 and more than 1)
#define RA_WHEEL_VERSION 4

/**
 * @brief Main control board selection.
 * See Constants.hpp for supported options.
 */
#define BOARD BOARD_AVR_MEGA2560

/**
 * @brief Stepper motor type in use on RA and DEC axes.
 * See Constants.hpp for supported options.
 */
#define RA_STEPPER_TYPE     STEPPER_TYPE_28BYJ48
#define DEC_STEPPER_TYPE    STEPPER_TYPE_28BYJ48

/**
 * @brief Stepper driver type in use on RA and DEC axes, with associated pin assignments
 * See Constants.hpp for supported DRIVER_TYPE options.
 */
#define RA_DRIVER_TYPE      DRIVER_TYPE_ULN2003
#define DEC_DRIVER_TYPE     DRIVER_TYPE_ULN2003
#define RA_STEPPER_SPEED          400   // Max. Speed = 600 for 28BYJ-48 and 3000 for NEMA17. Defaults = 400 for 28BYJ-48 and 1200 for NEMA17
#define RA_STEPPER_ACCELERATION   600   // Defaults: 600 for 28BYJ-48, 6000 for NEMA17
#define DEC_STEPPER_SPEED          600   // Max. Speed = 600 for 28BYJ-48 and 3000 for NEMA17. Defaults = 600 for 28BYJ-48 and 1300 for NEMA17
#define DEC_STEPPER_ACCELERATION   600   // Defaults: 600 for 28BYJ-48, 6000 for NEMA17

// TMC2209 UART settings
// These settings work only with TMC2209 in UART connection (single wire to TX)
#define RA_RMSCURRENT 1200       // RMS current in mA. Warning: Peak current will be 1.414 times higher!! Do not exceed your steppers max current!
#define DEC_RMSCURRENT 1000      // RMS current in mA. Warning: Peak current will be 1.414 times higher!! Do not exceed your steppers max current!

/**
 * @brief GPS receiver configuration.
 * Set USE_GPS to 1 to enable, 0 or #undef to exclude GPS from configuration.
 */
#define USE_GPS 1

/**
 * @brief Gyro-based tilt/roll levelling configuration.
 * Set USE_GYRO_LEVEL to 1 to enable, 0 or #undef to exclude gyro from configuration.
 */
#define USE_GYRO_LEVEL 1

/**
 * @brief Automated azimuth/altitude adjustment configuration.
 * Set AZIMUTH_ALTITUDE_MOTORS to 1 to enable, 0 or #undef to exclude AZ/ALT from configuration.
 */
#define AZIMUTH_ALTITUDE_MOTORS  0
#define AZ_STEPPER_TYPE     STEPPER_TYPE_28BYJ48
#define ALT_STEPPER_TYPE    STEPPER_TYPE_28BYJ48
#define AZ_DRIVER_TYPE      DRIVER_TYPE_ULN2003
#define ALT_DRIVER_TYPE     DRIVER_TYPE_ULN2003
#define AZ_CORRECTION_FACTOR 1.000f
#define ALT_CORRECTION_FACTOR 1.000f

/**
 * @brief Display & keypad configuration.
 * See Constants.hpp for supported DISPLAY_TYPE options.
 * Pin assignments vary based on display & keypad selection.
 */
#if (BOARD == BOARD_ESP32_ESP32DEV)
  #define DISPLAY_TYPE DISPLAY_TYPE_LCD_JOY_I2C_SSD1306
#else
  #define DISPLAY_TYPE DISPLAY_TYPE_LCD_KEYPAD
#endif

/**
 * @brief External (USB) serial port configuration.
 * See Constants.hpp for predefined SERIAL_BAUDRATE options, or customize as required.
 */
#define SERIAL_BAUDRATE SERIAL_BAUDRATE_ASCOM

/**
 * @brief Wifi configuration.
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
 * WIFI_HOSTNAME is what you need to enter into SkySafari's Settings for the scope name.
 */
#define WIFI_ENABLED 0
#define WIFI_MODE WIFI_MODE_AP_ONLY
#define WIFI_HOSTNAME "OAT"
#define WIFI_AP_MODE_WPAKEY "secret-key-for-oat"

/**
 * @brief Bluetooth configuration.
 * Set BLUETOOTH_ENABLED to 1 to enable, 0 or #undef to exclude Bluetooth from configuration.
 * If Bluetooth is enabled then the BLUETOOTH_DEVICE_NAME must be set.
 * Note that enabling Bluetooth increases flash usage by about 627 kB.
 */
#define BLUETOOTH_ENABLED 0 
#define BLUETOOTH_DEVICE_NAME "OpenAstroTracker"