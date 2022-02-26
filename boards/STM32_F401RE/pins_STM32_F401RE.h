/**
 * @brief a pins configuration file for an MKS Gen L V2.1 OAT.
 */

#pragma once

// DRIVER_TYPE_TMC2209_UART requires 4 digital pins in Arduino pin numbering
#ifndef RA_STEP_PIN
    #define RA_STEP_PIN D0  // STEP
#endif
#ifndef RA_DIR_PIN
    #define RA_DIR_PIN D1 // DIR
#endif
#ifndef RA_EN_PIN
    #define RA_EN_PIN D2 // Enable
#endif
#ifndef RA_DIAG_PIN
    #define RA_DIAG_PIN D3  // only needed for autohome function
#endif
#ifndef RA_MS0_PIN
    #define RA_MS0_PIN D4
#endif
#ifndef RA_MS1_PIN
    #define RA_MS1_PIN D5
#endif
#ifndef RA_MS2_PIN
    #define RA_MS2_PIN D6
#endif
// DRIVER_TYPE_TMC2209_UART requires 2 additional digital pins for SoftwareSerial, can be shared across all drivers
#ifndef RA_SERIAL_PORT_TX
    #define RA_SERIAL_PORT_TX D8  // SoftwareSerial TX port
#endif
#ifndef RA_SERIAL_PORT_RX
    #define RA_SERIAL_PORT_RX D7  // SoftwareSerial RX port
#endif
#ifndef RA_DRIVER_ADDRESS
    #define RA_DRIVER_ADDRESS 0b00
#endif
// DRIVER_TYPE_TMC2209_UART requires 4 digital pins in Arduino pin numbering
#ifndef DEC_STEP_PIN
    #define DEC_STEP_PIN D9  // STEP
#endif
#ifndef DEC_DIR_PIN
    #define DEC_DIR_PIN D10 // DIR
#endif
#ifndef DEC_EN_PIN
    #define DEC_EN_PIN D11  // Enable
#endif
#ifndef DEC_DIAG_PIN
    #define DEC_DIAG_PIN D12  // only needed for autohome function
#endif
#ifndef DEC_MS0_PIN
    #define DEC_MS0_PIN D13
#endif
#ifndef DEC_MS1_PIN
    #define DEC_MS1_PIN D14
#endif
#ifndef DEC_MS2_PIN
    #define DEC_MS2_PIN D15
#endif
// DRIVER_TYPE_TMC2209_UART requires 2 additional digital pins for SoftwareSerial, can be shared across all drivers
#ifndef DEC_SERIAL_PORT_TX
    #define DEC_SERIAL_PORT_TX D14  // SoftwareSerial TX port
#endif
#ifndef DEC_SERIAL_PORT_RX
    #define DEC_SERIAL_PORT_RX D15 // SoftwareSerial RX port
#endif
#ifndef DEC_DRIVER_ADDRESS
    #define DEC_DRIVER_ADDRESS 0b00
#endif

#define SW_SERIAL_UART 1
#ifndef UART_CONNECTION_TEST_TXRX
    #define UART_CONNECTION_TEST_TXRX 1
#endif

// DRIVER_TYPE_TMC2209_UART requires 4 digital pins in Arduino pin numbering
#ifndef AZ_STEP_PIN
    #define AZ_STEP_PIN 46  // STEP
#endif
#ifndef AZ_DIR_PIN
    #define AZ_DIR_PIN 48  // DIR
#endif
#ifndef AZ_EN_PIN
    #define AZ_EN_PIN 62  // Enable
#endif
#ifndef AZ_DIAG_PIN
    #define AZ_DIAG_PIN 18  // only needed for autohome function
#endif
// DRIVER_TYPE_TMC2209_UART requires 2 additional digital pins for SoftwareSerial, can be shared across all drivers
#ifndef AZ_SERIAL_PORT_TX
    #define AZ_SERIAL_PORT_TX 42  // SoftwareSerial TX port
#endif
#ifndef AZ_SERIAL_PORT_RX
    #define AZ_SERIAL_PORT_RX 65  // SoftwareSerial RX port
#endif
#ifndef AZ_DRIVER_ADDRESS
    #define AZ_DRIVER_ADDRESS 0b00
#endif
// DRIVER_TYPE_TMC2209_UART requires 4 digital pins in Arduino pin numbering
#ifndef ALT_STEP_PIN
    #define ALT_STEP_PIN 26  // STEP
#endif
#ifndef ALT_DIR_PIN
    #define ALT_DIR_PIN 28  // DIR
#endif
#ifndef ALT_EN_PIN
    #define ALT_EN_PIN 24  // Enable
#endif
#ifndef ALT_DIAG_PIN
    #define ALT_DIAG_PIN 2  // only needed for autohome function
#endif
// DRIVER_TYPE_TMC2209_UART requires 2 additional digital pins for SoftwareSerial, can be shared across all drivers
#ifndef ALT_SERIAL_PORT_TX
    #define ALT_SERIAL_PORT_TX 44  // SoftwareSerial TX port
#endif
#ifndef ALT_SERIAL_PORT_RX
    #define ALT_SERIAL_PORT_RX 66  // SoftwareSerial RX port
#endif
#ifndef ALT_DRIVER_ADDRESS
    #define ALT_DRIVER_ADDRESS 0b00
#endif

// DRIVER_TYPE_TMC2209_UART requires 4 digital pins in Arduino pin numbering. This is the E1 port.
#ifndef FOCUS_STEP_PIN
    #define FOCUS_STEP_PIN 36  // STEP
#endif
#ifndef FOCUS_DIR_PIN
    #define FOCUS_DIR_PIN 34  // DIR
#endif
#ifndef FOCUS_EN_PIN
    #define FOCUS_EN_PIN 30  // Enable
#endif
#ifndef FOCUS_DIAG_PIN
    #define FOCUS_DIAG_PIN 15  // only needed for autohome function
#endif
// DRIVER_TYPE_TMC2209_UART requires 2 additional digital pins for SoftwareSerial, can be shared across all drivers
#ifndef FOCUS_SERIAL_PORT_TX
    #define FOCUS_SERIAL_PORT_TX 20  // SoftwareSerial TX port
#endif
#ifndef FOCUS_SERIAL_PORT_RX
    #define FOCUS_SERIAL_PORT_RX 12  // SoftwareSerial RX port
#endif
#ifndef FOCUS_DRIVER_ADDRESS
    #define FOCUS_DRIVER_ADDRESS 0b00
#endif

// RA Homing pin for Hall sensor
#ifndef RA_HOMING_SENSOR_PIN
    #define RA_HOMING_SENSOR_PIN 53
#endif

//GPS pin configuration
#ifndef GPS_SERIAL_PORT
    #define GPS_SERIAL_PORT Serial1
#endif

// DISPLAY_TYPE_LCD_KEYPAD requires 6 digital & 1 analog output in Arduino pin numbering
#ifndef LCD_PIN4
    #define LCD_PIN4 17
#endif
#ifndef LCD_PIN5
    #define LCD_PIN5 16
#endif
#ifndef LCD_PIN6
    #define LCD_PIN6 23
#endif
#ifndef LCD_PIN7
    #define LCD_PIN7 25
#endif
#ifndef LCD_PIN8
    #define LCD_PIN8 27
#endif
#ifndef LCD_PIN9
    #define LCD_PIN9 29
#endif

// DISPLAY_TYPE_LCD_KEYPAD requires 1 analog input in Arduino pin numbering
#ifndef LCD_KEY_SENSE_PIN
    #define LCD_KEY_SENSE_PIN 58
#endif

//Pin to turn on dew heater MOSFET
#ifndef DEW_HEATER_PIN1
    #define DEW_HEATER_1_PIN 10
#endif
#ifndef DEW_HEATER_PIN2
    #define DEW_HEATER_2_PIN 7
#endif
