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

/**
 * @brief Stepper motor type in use on RA and DEC axes.
 * See Constants.hpp for supported options.
 */
#define RA_STEPPER_TYPE     STEPPER_TYPE_NEMA17
#define DEC_STEPPER_TYPE    STEPPER_TYPE_NEMA17

/**
 * @brief Stepper driver type in use on RA and DEC axes, with associated pin assignments
 * See Constants.hpp for supported DRIVER_TYPE options.
 */
#define RA_DRIVER_TYPE      DRIVER_TYPE_TMC2209_UART
// DRIVER_TYPE_TMC2209_UART requires 4 digital pins in Arduino pin numbering
#define RA_STEP_PIN 22  // STEP
#define RA_DIR_PIN  24  // DIR
#define RA_EN_PIN   26  // Enable
#define RA_DIAG_PIN 28  // only needed for autohome function
// DRIVER_TYPE_TMC2209_UART requires 2 additional digital pins for SoftwareSerial, can be shared across all drivers
#define RA_SERIAL_PORT_TX 14 // SoftwareSerial TX port
#define RA_SERIAL_PORT_RX 15 // SoftwareSerial RX port
// Pin strapping on TMC2209 controls its address on the shared serial bus
#define RA_DRIVER_ADDRESS 0b00  // Set by MS1/MS2. LOW/LOW in this case

#define DEC_DRIVER_TYPE     DRIVER_TYPE_TMC2209_UART
// DRIVER_TYPE_TMC2209_UART requires 4 digital pins in Arduino pin numbering
#define DEC_STEP_PIN 23  // STEP
#define DEC_DIR_PIN  25  // DIR
#define DEC_EN_PIN   27  // Enable
#define DEC_DIAG_PIN 29  // only needed for autohome function
// DRIVER_TYPE_TMC2209_UART requires 2 additional digital pins for SoftwareSerial, can be shared across all drivers
#define DEC_SERIAL_PORT_TX 14 // SoftwareSerial TX port
#define DEC_SERIAL_PORT_RX 15 // SoftwareSerial RX port
// Pin strapping on TMC2209 controls its address on the shared serial bus
#define DEC_DRIVER_ADDRESS 0b01  // Set by MS1/MS2 (MS1 HIGH, MS2 LOW)

/**
 * @brief GPS receiver configuration.
 * Set USE_GPS to 1 to enable, 0 or #undef to exclude GPS from configuration.
 * On ATmega GPS uses hardware Serial1 by default. No additional pins required. 
 */
#define USE_GPS 1

/**
 * @brief Gyro-based tilt/roll levelling configuration.
 * Set USE_GYRO_LEVEL to 1 to enable, 0 or #undef to exclude gyro from configuration.
 * On ATmega gyro uses hardware I2C. No additional pins required. 
 */
#define USE_GYRO_LEVEL 1

/**
 * @brief Automated azimuth/altitude adjustment configuration.
 * Set AZIMUTH_ALTITUDE_MOTORS to 1 to enable, 0 or #undef to exclude AZ/ALT from configuration.
 */
#define AZIMUTH_ALTITUDE_MOTORS  1

// Azimuth axis
#define AZ_STEPPER_TYPE     STEPPER_TYPE_28BYJ48
#define AZ_DRIVER_TYPE      DRIVER_TYPE_ULN2003
// DRIVER_TYPE_ULN2003 requires 4 digital outputs in Arduino pin numbering
#define AZ_IN1_PIN 47
#define AZ_IN2_PIN 49
#define AZ_IN3_PIN 51
#define AZ_IN4_PIN 53

// Altitude axis
#define ALT_STEPPER_TYPE    STEPPER_TYPE_28BYJ48
#define ALT_DRIVER_TYPE     DRIVER_TYPE_ULN2003
// DRIVER_TYPE_ULN2003 requires 4 digital outputs in Arduino pin numbering
#define ALT_IN1_PIN 46
#define ALT_IN2_PIN 48
#define ALT_IN3_PIN 50
#define ALT_IN4_PIN 52

/**
 * @brief Display & keypad configuration.
 * See Constants.hpp for supported DISPLAY_TYPE options.
 * Pin assignments vary based on display & keypad selection.
 */
#define DISPLAY_TYPE DISPLAY_TYPE_LCD_KEYPAD
// DISPLAY_TYPE_LCD_KEYPAD requires 6 digital & 1 analog output in Arduino pin numbering
#define LCD_BRIGHTNESS_PIN 10   // Brightness PWM control (#undef to disable)
#define LCD_PIN4 4              // LCD DB4 pin
#define LCD_PIN5 5              // LCD DB5 pin
#define LCD_PIN6 6              // LCD DB6 pin
#define LCD_PIN7 7              // LCD DB7 pin
#define LCD_PIN8 8              // LCD RS pin
#define LCD_PIN9 9              // LCD ENABLE pin
// DISPLAY_TYPE_LCD_KEYPAD requires 1 analog input in Arduino pin numbering
#define LCD_KEY_SENSE_PIN 0     // Analog input for shield keys (#undef to disable)

/**
 * @brief External (USB) serial port configuration.
 * See Constants.hpp for predefined SERIAL_BAUDRATE options, or customize as required.
 */
#define SERIAL_BAUDRATE SERIAL_BAUDRATE_STELLARIUM_DIRECT

