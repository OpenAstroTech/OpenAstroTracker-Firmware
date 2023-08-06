/**
 * @brief a pins configuration file for an MKS Gen L V2.1 OAT.
 * https://github.com/makerbase-mks/MKS-GEN_L/blob/master/hardware/MKS%20Gen_L%20V1.0_008/MKS%20Gen_L%20V1.0_008%20PIN.pdf
 */

#pragma once

// DRIVER_TYPE_TMC2209_UART requires 4 digital pins in Arduino pin numbering
#ifndef RA_STEP_PIN
    #define RA_STEP_PIN 54  // STEP
#endif
#ifndef RA_DIR_PIN
    #define RA_DIR_PIN 55  // DIR
#endif
#ifndef RA_EN_PIN
    #define RA_EN_PIN 38  // Enable
#endif
#ifndef RA_DIAG_PIN
    #define RA_DIAG_PIN 3  // only needed for autohome function
#endif
// DRIVER_TYPE_TMC2209_UART requires 2 additional digital pins for SoftwareSerial, can be shared across all drivers
#ifndef RA_SERIAL_PORT_TX
    #define RA_SERIAL_PORT_TX 4  // SoftwareSerial TX port
#endif
#ifndef RA_SERIAL_PORT_RX
    #define RA_SERIAL_PORT_RX 49  // SoftwareSerial RX port
#endif
#ifndef RA_DRIVER_ADDRESS
    #define RA_DRIVER_ADDRESS 0b00
#endif
// DRIVER_TYPE_TMC2209_UART requires 4 digital pins in Arduino pin numbering
#ifndef DEC_STEP_PIN
    #define DEC_STEP_PIN 60  // STEP
#endif
#ifndef DEC_DIR_PIN
    #define DEC_DIR_PIN 61  // DIR
#endif
#ifndef DEC_EN_PIN
    #define DEC_EN_PIN 56  // Enable
#endif
#ifndef DEC_DIAG_PIN
    #define DEC_DIAG_PIN 14  // only needed for autohome function
#endif
// DRIVER_TYPE_TMC2209_UART requires 2 additional digital pins for SoftwareSerial, can be shared across all drivers
#ifndef DEC_SERIAL_PORT_TX
    #define DEC_SERIAL_PORT_TX 5  // SoftwareSerial TX port
#endif
#ifndef DEC_SERIAL_PORT_RX
    #define DEC_SERIAL_PORT_RX 50  // SoftwareSerial RX port
#endif
#ifndef DEC_DRIVER_ADDRESS
    #define DEC_DRIVER_ADDRESS 0b00
#endif

#define SW_SERIAL_UART 1

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
    #define AZ_SERIAL_PORT_TX 6  // SoftwareSerial TX port
#endif
#ifndef AZ_SERIAL_PORT_RX
    #define AZ_SERIAL_PORT_RX 51  // SoftwareSerial RX port
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
    #define ALT_SERIAL_PORT_TX 11  // SoftwareSerial TX port
#endif
#ifndef ALT_SERIAL_PORT_RX
    #define ALT_SERIAL_PORT_RX 52  // SoftwareSerial RX port
#endif
#ifndef ALT_DRIVER_ADDRESS
    #define ALT_DRIVER_ADDRESS 0b00
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
    #define LCD_KEY_SENSE_PIN 65
#endif

//Pin to turn on dew heater MOSFET
#ifndef DEW_HEATER_1_PIN
    #define DEW_HEATER_1_PIN 10
#endif
#ifndef DEW_HEATER_2_PIN
    #define DEW_HEATER_2_PIN 7
#endif

//Serial port for external debugging
#if DEBUG_SEPARATE_SERIAL == 1
    #ifndef DEBUG_SERIAL_PORT
        #define DEBUG_SERIAL_PORT Serial2  //D16 (LCD_RS) - TXD2 and D17 (LCD_EN) - RXD2
    #endif
#else
    #ifndef DEBUG_SERIAL_PORT
        #define DEBUG_SERIAL_PORT Serial
    #endif
#endif
