/**
 * @brief an example configuration file for an ESP32-based OAT.
 * 
 * This configuration includes 28BYJ48 steppers with ULN2003 drivers, GPS, 
 * gyro levelling, and Wifi. Display is an I2C OLED device with joystick-based 
 * keypad. USB serial port is set for connecting to ASCOM.
 */

#pragma once

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
// DRIVER_TYPE_ULN2003 requires 4 digital outputs in Arduino pin numbering
#define RA_IN1_PIN  13  
#define RA_IN2_PIN  12  
#define RA_IN3_PIN  14  
#define RA_IN4_PIN  27  

#define DEC_DRIVER_TYPE     DRIVER_TYPE_ULN2003
// DRIVER_TYPE_ULN2003 requires 4 digital outputs in Arduino pin numbering
#define DEC_IN1_PIN  26  
#define DEC_IN2_PIN  25  
#define DEC_IN3_PIN  33   
#define DEC_IN4_PIN  32  

/**
 * @brief GPS receiver configuration.
 * Set USE_GPS to 1 to enable, 0 or #undef to exclude GPS from configuration.
 * On ESP32 GPS uses hardware Serial2. No additional pins required. 
 * Note the potential serial port assignment conflict if stepper driver DRIVER_TYPE_TMC2209_UART is used.
 */
#define USE_GPS 1

/**
 * @brief Gyro-based tilt/roll levelling configuration.
 * Set USE_GYRO_LEVEL to 1 to enable, 0 or #undef to exclude gyro from configuration.
 * On ESP32 gyro uses hardware I2C. No additional pins required. 
 */
#define USE_GYRO_LEVEL 1

/**
 * @brief Automated azimuth/altitude adjustment configuration.
 * Set AZIMUTH_ALTITUDE_MOTORS to 1 to enable, 0 or #undef to exclude AZ/ALT from configuration.
 */
#define AZIMUTH_ALTITUDE_MOTORS  0

/**
 * @brief Display & keypad configuration.
 * See Constants.hpp for supported DISPLAY_TYPE options.
 * Pin assignments vary based on display & keypad selection.
 */
#define DISPLAY_TYPE DISPLAY_TYPE_LCD_JOY_I2C_SSD1306
// DISPLAY_TYPE_LCD_JOY_I2C_SSD1306 requires 3 analog inputs in Arduino pin numbering
#define LCD_KEY_SENSE_X_PIN 34
#define LCD_KEY_SENSE_Y_PIN 39
#define LCD_KEY_SENSE_PUSH_PIN 36

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

