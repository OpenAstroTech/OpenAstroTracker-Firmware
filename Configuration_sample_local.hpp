#pragma once

// Set to 1 for the northern hemisphere, 0 otherwise
#define NORTHERN_HEMISPHERE 1

// Used RA wheel version. Unless you printed your OAT before March 2020, you're using 
// a version 2 or higher (software only differentiates between 1 and more than 1)
#define RA_WHEEL_VERSION 4

// Stepper types/models. See supported stepper values in Constants.hpp. Change according to the steppers you are using
#define RA_STEPPER_TYPE     STEPPER_TYPE_NEMA17
#define DEC_STEPPER_TYPE    STEPPER_TYPE_NEMA17

// Driver selection
// GENERIC drivers include A4988 and any Bipolar STEP/DIR based drivers. See supported driver values in Constants.hpp. Note Microstep assignments in config_pins.
#define RA_DRIVER_TYPE      DRIVER_TYPE_TMC2209_UART
#define DEC_DRIVER_TYPE     DRIVER_TYPE_TMC2209_UART
#define RA_STEPPER_SPEED          400   // Max. Speed = 600 for 28BYJ-48 and 3000 for NEMA17. Defaults = 400 for 28BYJ-48 and 1200 for NEMA17
#define RA_STEPPER_ACCELERATION   600   // Defaults: 600 for 28BYJ-48, 6000 for NEMA17
#define DEC_STEPPER_SPEED          600   // Max. Speed = 600 for 28BYJ-48 and 3000 for NEMA17. Defaults = 600 for 28BYJ-48 and 1300 for NEMA17
#define DEC_STEPPER_ACCELERATION   600   // Defaults: 600 for 28BYJ-48, 6000 for NEMA17

// TMC2209 UART settings
// These settings work only with TMC2209 in UART connection (single wire to TX)
#define RA_RMSCURRENT 1200       // RMS current in mA. Warning: Peak current will be 1.414 times higher!! Do not exceed your steppers max current!
#define DEC_RMSCURRENT 1000      // RMS current in mA. Warning: Peak current will be 1.414 times higher!! Do not exceed your steppers max current!

// Set this to 1 if you are using a NEO6m GPS module for HA/LST and location automatic determination.
// GPS uses Serial1 by default, which is pins 18/19 on Mega. Change pins in configuration_pins.hpp
#define USE_GPS 1

// Set this to 1 if you are using a MPU6050 electronic level
// Wire the board to 20/21 on Mega. Change pins in configuration_pins.hpp if you use other pins
#define USE_GYRO_LEVEL 1

// Set this to 1 if the mount has motorized Azimuth and Altitude adjustment. Set pins in configuration_pins.hpp. Change motor speeds in Configuration_adv.hpp
#define AZIMUTH_ALTITUDE_MOTORS  0
#define AZ_STEPPER_TYPE     STEPPER_TYPE_28BYJ48
#define ALT_STEPPER_TYPE    STEPPER_TYPE_28BYJ48
#define AZ_DRIVER_TYPE      DRIVER_TYPE_ULN2003
#define ALT_DRIVER_TYPE     DRIVER_TYPE_ULN2003
#define AZ_CORRECTION_FACTOR 1.000f
#define ALT_CORRECTION_FACTOR 1.000f
