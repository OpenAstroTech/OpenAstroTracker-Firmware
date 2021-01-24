#pragma once

/**
 * This file contains constants that SHOULD NOT BE CHANGED by oat users!
 * If you are a developer and want to add new hardware support, add a
 * proper definition here with increased value.
 **/

/**
 * Supported boards. The name consists of the platform and the board name (model).
 * Applies predefined pin mappings for common boards.
 * If BOARD_UNKNOWN is used then user is responsible for specifying complete mapping in Configuration_local.hpp
 **/
#define BOARD_UNKNOWN            0000

// AVR based boards
#define BOARD_AVR_MEGA2560       0001
#define BOARD_AVR_MKS_GEN_L_V21  0002
#define BOARD_AVR_MKS_GEN_L_V2   0003
#define BOARD_AVR_MKS_GEN_L_V1   0004

// ESP32 based boards
#define BOARD_ESP32_ESP32DEV     1001

/**
 * Supported display types. Use one of these values for DISPLAY_TYPE configuration matching your used display.
 * 
 * DISPLAY_TYPE_NONE:                       No display. Use this if you don't use any display.
 * DISPLAY_TYPE_LCD_KEYPAD:                 1602 LCD Keypad shield which can be mounted directly to an Arduino UNO / Mega boards.
 *                                          Example: https://www.digikey.com/en/products/detail/dfrobot/DFR0009/7597118
 * DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23008:    RGB LCD Keypad shield based on the MCP23008 I/O Expander. It can be mounted 
 *                                          directly to an Arduino UNO / Mega boards and controlled over I2C.
 * DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23017:    RGB LCD Keypad shield based on the MCP23017 I/O Expander. It can be mounted 
 *                                          directly to an Arduino UNO / Mega boards and controlled over I2C.                                         
 * DISPLAY_TYPE_LCD_JOY_I2C_SSD1306:        I2C 32x128 OLED display module with SSD1306 controller, plus mini joystick
 *                                          Display: https://www.banggood.com/Geekcreit-0_91-Inch-128x32-IIC-I2C-Blue-OLED-LCD-Display-DIY-Module-SSD1306-Driver-IC-DC-3_3V-5V-p-1140506.html
 *                                          Joystick: https://www.banggood.com/3pcs-JoyStick-Module-Shield-2_54mm-5-pin-Biaxial-Buttons-Rocker-for-PS2-Joystick-Game-Controller-Sensor-p-1586026.html
 **/
#define DISPLAY_TYPE_NONE                       0
#define DISPLAY_TYPE_LCD_KEYPAD                 1
#define DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23008    2
#define DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23017    3
#define DISPLAY_TYPE_LCD_JOY_I2C_SSD1306        4


// Supported stepper models
#define STEPPER_TYPE_28BYJ48    0
#define STEPPER_TYPE_NEMA17     1


// Supported stepper driver models
#define DRIVER_TYPE_ULN2003             0   // Supports halfstepping
#define DRIVER_TYPE_A4988_GENERIC       1   // Supports fixed microstepping
#define DRIVER_TYPE_TMC2209_STANDALONE  2   // Supports fixed microstepping
#define DRIVER_TYPE_TMC2209_UART        3   // Supports dynamic microstepping


// USB serial port speed according to external controller
#define SERIAL_BAUDRATE_STELLARIUM_DIRECT   9600
#define SERIAL_BAUDRATE_ASCOM               57600

// Wifi operating modes (ESP32 only)
#define WIFI_MODE_INFRASTRUCTURE                        0   // Infrastructure Only - OAT connects to an existing Router
#define WIFI_MODE_AP_ONLY                               1   // AP Mode Only        - OAT acts as a local Router/Hotspot
#define WIFI_MODE_ATTEMPT_INFRASTRUCTURE_FAIL_TO_AP     2   // Attempt Infrastructure, Fail over to AP Mode - Attempt infrastructure mode, with fail over to AP Mode.
#define WIFI_MODE_DISABLED                              3   // Wifi disabled, transceiver switched off

// Debugging output control
// Each bit in the debug level specifies a kind of debug to enable. Combine these into DEBUG_LEVEL for the required information e.g.:
//  #define DEBUG_LEVEL (DEBUG_STEPPERS|DEBUG_MOUNT)
//  #define DEBUG_LEVEL (DEBUG_INFO|DEBUG_MOUNT|DEBUG_GENERAL)
//  #define DEBUG_LEVEL (DEBUG_SERIAL|DEBUG_WIFI|DEBUG_INFO|DEBUG_MOUNT|DEBUG_GENERAL)
//  #define DEBUG_LEVEL (DEBUG_ANY)
//  #define DEBUG_LEVEL (DEBUG_INFO|DEBUG_MOUNT|DEBUG_GENERAL)
#define DEBUG_NONE           0x0000     // No debug output (release build)
#define DEBUG_INFO           0x0001     // General output, like startup variables and status
#define DEBUG_SERIAL         0x0002     // Serial commands and replies
#define DEBUG_WIFI           0x0004     // Wifi related output
#define DEBUG_MOUNT          0x0008     // Mount processing output
#define DEBUG_MOUNT_VERBOSE  0x0010     // Verbose mount processing (coordinates, etc)
#define DEBUG_GENERAL        0x0020     // Other misc. output
#define DEBUG_MEADE          0x0040     // Meade command handling output
#define DEBUG_VERBOSE        0x0080     // High-rate mount activity, incl. stepper servicing
#define DEBUG_STEPPERS       0x0100     // Stepper motor-related activity
#define DEBUG_EEPROM         0x0200     // Storing and retrieval of non-volatile configuration data
#define DEBUG_GYRO           0x0400     // Gyro activity (tilt/roll) calibration
#define DEBUG_ANY            0xFFFF     // All debug output
