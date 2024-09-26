#pragma once

/**
 * This file contains advanced configurations. Edit values here only if you know what you are doing. Invalid values
 * can lead to OAT misbehaving very bad and in worst case could even lead to hardware damage. The default values here
 * were chosen after many tests and can are currently concidered to work the best.
 * 
 * 
 *         YOU SHOULD NOT NEED TO EDIT THIS FILE!
 *         --------------------------------------
 **/

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                            ////////
// Misc Configuration         ////////
//                            ////////
//////////////////////////////////////
#if !defined(USE_DUMMY_EEPROM)
    #define USE_DUMMY_EEPROM false
#endif
#if !defined(BUFFER_LOGS)
    #define BUFFER_LOGS false
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                            ////////
// MOTOR & DRIVER SETTINGS    ////////
//                            ////////
//////////////////////////////////////
//
// This is how many steps your stepper needs for a full rotation.
#ifndef RA_STEPPER_SPR
    #define RA_STEPPER_SPR 400  // NEMA 0.9° = 400  |  NEMA 1.8° = 200
#endif
#ifndef RA_STEPPER_SPEED
    #define RA_STEPPER_SPEED 1200  // You can change the speed and acceleration of the steppers here. Max. Speed = 3000.
#endif
#ifndef RA_STEPPER_ACCELERATION
    #define RA_STEPPER_ACCELERATION 6000
#endif

#ifndef DEC_STEPPER_SPR
    #define DEC_STEPPER_SPR 400  // NEMA 0.9° = 400  |  NEMA 1.8° = 200
#endif
#ifndef DEC_STEPPER_SPEED
    #define DEC_STEPPER_SPEED 1300  // You can change the speed and acceleration of the steppers here. Max. Speed = 3000.
#endif
#ifndef DEC_STEPPER_ACCELERATION
    #define DEC_STEPPER_ACCELERATION 6000
#endif

// MICROSTEPPING
// The DRIVER_TYPE_TMC2209_UART driver can dynamically switch between microstep settings.
// All other drivers are set by the MS pins, therefore values here must match pin strapping.
// Valid values: 1, 2, 4, 8, 16, 32, 64, 128, 256
// !! Ensure the value corresponds to the specific driver capabilities and the correct MS pin configuration
//
#if (RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
    #ifndef RA_SLEW_MICROSTEPPING
        #define RA_SLEW_MICROSTEPPING 8  // The (default) microstep mode used for slewing RA axis
    #endif
    #ifndef RA_TRACKING_MICROSTEPPING
        #define RA_TRACKING_MICROSTEPPING 256  // The fine microstep mode for tracking RA axis
    #endif
    #ifndef RA_UART_STEALTH_MODE
        #define RA_UART_STEALTH_MODE 0
    #endif
#elif (RA_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC) || (RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE)
    #ifndef RA_SLEW_MICROSTEPPING
        #define RA_SLEW_MICROSTEPPING 8  // Microstep mode set by MS pin strapping. Use the same microstep mode for both slewing & tracking
    #endif
    #if defined(RA_TRACKING_MICROSTEPPING) && (RA_TRACKING_MICROSTEPPING != RA_SLEW_MICROSTEPPING)
        #error With A4988 drivers or TMC2209 drivers in Standalone mode, RA microstepping must be the same for slewing and tracking. Delete RA_TRACKING_MICROSTEPPING from your config.
    #endif
    #define RA_TRACKING_MICROSTEPPING RA_SLEW_MICROSTEPPING
#else
    #error Unknown RA driver type
#endif

#if (DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
    #ifndef DEC_SLEW_MICROSTEPPING
        #define DEC_SLEW_MICROSTEPPING 16  // The (default) microstep mode used for slewing DEC
    #endif
    #ifndef DEC_GUIDE_MICROSTEPPING
        #define DEC_GUIDE_MICROSTEPPING 256  // The fine microstep mode used for guiding DEC only
    #endif
    #ifndef DEC_UART_STEALTH_MODE
        #define DEC_UART_STEALTH_MODE 0
    #endif
#elif (DEC_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC) || (DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE)
    #ifndef DEC_SLEW_MICROSTEPPING
        #define DEC_SLEW_MICROSTEPPING                                                                                                     \
            16  // Only UART drivers support dynamic switching. Use the same microstep mode for both slewing & guiding
    #endif
    #if defined(DEC_GUIDE_MICROSTEPPING) && (DEC_GUIDE_MICROSTEPPING != DEC_SLEW_MICROSTEPPING)
        #error With A4988 drivers or TMC2209 drivers in Standalone mode, DEC microstepping must be the same for slewing and guiding. Delete DEC_GUIDE_MICROSTEPPING from your config.
    #endif
    #define DEC_GUIDE_MICROSTEPPING DEC_SLEW_MICROSTEPPING
#else
    #error Unknown DEC driver type
#endif

// Extended TMC2209 UART settings
// These settings work only with TMC2209 in UART connection (single wire to TX)
#if (RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART) || (DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
    #if defined(RA_RMSCURRENT) || defined(DEC_RMSCURRENT)
        #error Please delete any definitions of RA_RMSCURRENT or DEC_RMSCURRENT in your local configuration file. Define XXX_MOTOR_CURRENT_RATING and XXX_OPERATING_CURRENT_SETTING instead.
    #endif
    //UART Current settings
    #define RA_RMSCURRENT  RA_MOTOR_CURRENT_RATING *(RA_OPERATING_CURRENT_SETTING / 100.0f) / 1.414f
    #define DEC_RMSCURRENT DEC_MOTOR_CURRENT_RATING *(DEC_OPERATING_CURRENT_SETTING / 100.0f) / 1.414f

    #define RA_STALL_VALUE  100  // adjust this value if the RA autohoming sequence often false triggers, or triggers too late
    #define DEC_STALL_VALUE 10   // adjust this value if the RA autohoming sequence often false triggers, or triggers too late

    #ifndef RA_AUDIO_FEEDBACK
        #define RA_AUDIO_FEEDBACK                                                                                                          \
            0  // If one of these are set to 1, the respective driver will shut off the stealthchop mode, resulting in a audible whine
    #endif
    #ifndef DEC_AUDIO_FEEDBACK
        #define DEC_AUDIO_FEEDBACK 0  // of the stepper coils. Use this to verify that UART is working properly.
    #endif

    #ifndef USE_VREF
        #define USE_VREF 0  //By default Vref is ignored when using UART to specify rms current. Only enable if you know what you are doing.
    #endif

    #ifndef UART_CONNECTION_TEST_TXRX
        #define UART_CONNECTION_TEST_TXRX 0
    #endif
    #ifndef UART_CONNECTION_TEST_TX
        #define UART_CONNECTION_TEST_TX 0
    #endif
    #ifndef UART_CONNECTION_TEST_TX_DEG
        #define UART_CONNECTION_TEST_TX_DEG 5.0f  //Default degrees to rotate during testing
    #endif
#endif

// Backlash Settings
#ifndef BACKLASH_STEPS
    #define BACKLASH_STEPS                                                                                                                 \
        0 * RA_SLEW_MICROSTEPPING  // set the number of backlash steps the motor has (0 for NEMA motors, 16 for 28BYJ motors)
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                        ////////
// MECHANICS SETTINGS     ////////
//                        ////////
//////////////////////////////////
//

// the pitch of the GT2 timing belt
#define GT2_BELT_PITCH 2.0f  // mm

// the Circumference of the RA wheel.  V1 = 1057.1  |  V2 = 1131.0
#ifndef RA_WHEEL_CIRCUMFERENCE
    #if RA_WHEEL_VERSION == 1
        #define RA_WHEEL_CIRCUMFERENCE 1057.1f
    #elif RA_WHEEL_VERSION >= 2
        #define RA_WHEEL_CIRCUMFERENCE 1132.73f
    #else
        #error Unsupported RA wheel version, please recheck RA_WHEEL_VERSION
    #endif
#endif

// the Circumference of the DEC wheel.
#ifndef DEC_WHEEL_CIRCUMFERENCE
    #define DEC_WHEEL_CIRCUMFERENCE 565.5f
#endif

#ifndef RA_TRANSMISSION
    #define RA_TRANSMISSION (RA_WHEEL_CIRCUMFERENCE / (RA_PULLEY_TEETH * GT2_BELT_PITCH))
#endif

#ifndef RA_SLEWING_SPEED_DEG
    #define RA_SLEWING_SPEED_DEG 4.0f  // deg/s
#endif

#ifndef RA_SLEWING_ACCELERATION_DEG
    #define RA_SLEWING_ACCELERATION_DEG 4.0f  // deg/s/s
#endif

#ifndef DEC_SLEWING_SPEED_DEG
    #define DEC_SLEWING_SPEED_DEG 4.0f  // deg/s
#endif

#ifndef DEC_SLEWING_ACCELERATION_DEG
    #define DEC_SLEWING_ACCELERATION_DEG 4.0f  // deg/s/s
#endif

// RA movement:
// The radius of the surface that the belt runs on (in V1 of the ring) was 168.24mm.
// Belt moves 40mm for one stepper revolution (2mm pitch, 20 teeth).
// RA wheel is 2 x PI x 168.24mm (V2:180mm) circumference = 1057.1mm (V2:1131mm)
// One RA revolution needs 26.43 (1057.1mm / 40mm) stepper revolutions (V2: 28.27 (1131mm/40mm))
// Which means 108245 steps (26.43 x 4096) moves 360 degrees (V2: 115812 steps (28.27 x 4096))
// So there are 300.1 steps/degree (108245 / 360)  (V2: 322 (115812 / 360))
// Theoretically correct RA tracking speed is 1.246586 (300 x 14.95903 / 3600) (V2 : 1.333800 (322 x 14.95903 / 3600) steps/sec (this is for 20T)
// Include microstepping ratio here such that steps/sec is updates/sec to stepper driver
#ifndef RA_STEPS_PER_DEGREE
    #define RA_STEPS_PER_DEGREE (RA_TRANSMISSION * RA_STEPPER_SPR * RA_SLEW_MICROSTEPPING / 360.0f)
#endif

// RA limits
#ifndef RA_LIMIT_LEFT
    #define RA_LIMIT_LEFT 5.0f
#endif
#ifndef RA_LIMIT_RIGHT
    #define RA_LIMIT_RIGHT 7.0f
#endif
#ifndef RA_TRACKING_LIMIT
    #define RA_TRACKING_LIMIT 7.0f
#endif

#ifndef RA_PHYSICAL_LIMIT
    #define RA_PHYSICAL_LIMIT 7.0f
#endif

#ifndef DEC_TRANSMISSION
    #define DEC_TRANSMISSION (DEC_WHEEL_CIRCUMFERENCE / (DEC_PULLEY_TEETH * GT2_BELT_PITCH))
#endif

// DEC movement:
// Belt moves 40mm for one stepper revolution (2mm pitch, 20 teeth).
// DEC wheel is 2 x PI x 90mm circumference which is 565.5mm
// One DEC revolution needs 14.13 (565.5mm/40mm) stepper revolutions
// Which means 57907 steps (14.14 x 4096) moves 360 degrees
// So there are 160.85 steps/degree (57907/360) (this is for 20T)
// Include microstepping ratio here such that steps/sec is updates/sec to stepper driver
#ifndef DEC_STEPS_PER_DEGREE
    #define DEC_STEPS_PER_DEGREE (DEC_TRANSMISSION * DEC_STEPPER_SPR * DEC_SLEW_MICROSTEPPING / 360.0f)
#endif

#ifndef DEC_LIMIT_UP
    #ifdef OAM
        #define DEC_LIMIT_UP 135.0f
    #else
        #define DEC_LIMIT_UP 0.0f
    #endif
#endif
#ifndef DEC_LIMIT_DOWN
    #ifdef OAM
        #define DEC_LIMIT_DOWN 135.0f
    #else
        #define DEC_LIMIT_DOWN 0.0f
    #endif
#endif

////////////////////////////
//
// GUIDE SETTINGS
// This is the multiplier of the normal tracking speed that a guiding pulse will have.
// Note that the North & South (DEC) tracking speed is calculated as the +multiplier & -multiplier
// Note that the West & East (RA) tracking speed is calculated as the (multiplier+1.0) & (multiplier-1.0)
#ifndef RA_PULSE_MULTIPLIER
    #define RA_PULSE_MULTIPLIER 1.5f
#endif
#ifndef DEC_PULSE_MULTIPLIER
    #define DEC_PULSE_MULTIPLIER 0.5f
#endif

////////////////////////////
//
// INVERT AXIS
// Set to 1 or 0 to invert motor directions
#ifndef RA_INVERT_DIR
    #define RA_INVERT_DIR 0
#endif
#ifndef DEC_INVERT_DIR
    #define DEC_INVERT_DIR 0
#endif
#ifndef AZ_INVERT_DIR
    #define AZ_INVERT_DIR 0
#endif
#ifndef ALT_INVERT_DIR
    #define ALT_INVERT_DIR 0
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                  ////////
// LCD SETTINGS     ////////
//                  ////////
////////////////////////////
//
// UPDATE TIME
// Time in ms between LCD screen updates during slewing operations
#define DISPLAY_UPDATE_TIME 200

////////////////////////////
//
// LCD BUTTON TEST
// Set this to 1 to run a key diagnostic. No tracker functions are on at all.
#ifndef LCD_BUTTON_TEST
    #define LCD_BUTTON_TEST 0
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                     ///
// HARDWARE EXTENSIONS SUPPORT SECTION ///
//                                     ///
//////////////////////////////////////////
//
// Backwards compatability. V1.9.07 changed from combined Azimuth/Altitude addon to seperate controls for each
//

//////////////////////////////////////////
// AutoPA AZIMUTH support
//////////////////////////////////////////
#ifdef AZIMUTH_ALTITUDE_MOTORS
    #if AZIMUTH_ALTITUDE_MOTORS == 1
        #error Please remove AZIMUTH_ALTITUDE_MOTORS definition and use only ALT_STEPPER_TYPE and AZ_STEPPER_TYPE
    #endif
    #undef AZIMUTH_ALTITUDE_MOTORS
#endif

//////////////////////////////////////////
// AutoPA AZIMUTH support
//////////////////////////////////////////
// Enable Azimuth motor functionality in your local Configuration. Do not edit here!
#if AZ_STEPPER_TYPE != STEPPER_TYPE_NONE
    #ifndef AZ_MICROSTEPPING
        #define AZ_MICROSTEPPING 64
    #endif
    #ifndef AZ_STEPPER_SPR
        #define AZ_STEPPER_SPR 400  // NEMA 0.9° = 400  |  NEMA 1.8° = 200
    #endif
    #ifndef AZ_STEPPER_SPEED
        #define AZ_STEPPER_SPEED                                                                                                           \
            (100 * AZ_MICROSTEPPING)  // You can change the speed and acceleration of the steppers here. Max. Speed = 3000.
    #endif
    #ifndef AZ_STEPPER_ACCELERATION
        #define AZ_STEPPER_ACCELERATION (100 * AZ_MICROSTEPPING)
    #endif

    // the Circumference of the AZ rotation. 808mm dia.
    #ifndef AZ_CIRCUMFERENCE
        #define AZ_CIRCUMFERENCE 2538.4f
    #endif
    #ifndef AZIMUTH_STEPS_PER_REV
        #define AZIMUTH_STEPS_PER_REV                                                                                                      \
            (AZ_CORRECTION_FACTOR * (AZ_CIRCUMFERENCE / (AZ_PULLEY_TEETH * GT2_BELT_PITCH)) * AZ_STEPPER_SPR                               \
             * AZ_MICROSTEPPING)  // Actually u-steps/rev
    #endif
    #define AZIMUTH_STEPS_PER_ARC_MINUTE (AZIMUTH_STEPS_PER_REV / (360 * 60.0f))  // Used to determine move distance in steps

    // AZ TMC2209 UART settings
    // These settings work only with TMC2209 in UART connection (single wire to TX)
    #if (AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
        #define AZ_RMSCURRENT AZ_MOTOR_CURRENT_RATING *(AZ_OPERATING_CURRENT_SETTING / 100.0f) / 1.414f

        #ifndef AZ_MOTOR_HOLD_SETTING
            #define AZ_MOTOR_HOLD_SETTING 100
        #endif

        #define AZ_STALL_VALUE 10  // adjust this value if the RA autohoming sequence often false triggers, or triggers too late

        #ifndef USE_VREF
            #define USE_VREF                                                                                                               \
                0  //By default Vref is ignored when using UART to specify rms current. Only enable if you know what you are doing.
        #endif
    #endif
    #ifndef AZ_ALWAYS_ON
        #define AZ_ALWAYS_ON 0
    #endif
#endif

//////////////////////////////////////////
// AutoPA ALTITUDE support
//////////////////////////////////////////
// Enable Altitude motor functionality in your local configuration. Do not edit here!
#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    #ifndef ALT_MICROSTEPPING
        #define ALT_MICROSTEPPING 4
    #endif
    #ifndef ALT_STEPPER_SPR
        #define ALT_STEPPER_SPR 400  // NEMA 0.9° = 400  |  NEMA 1.8° = 200
    #endif
    #ifndef ALT_STEPPER_SPEED
        #define ALT_STEPPER_SPEED 2000
    #endif
    #ifndef ALT_STEPPER_ACCELERATION
        #define ALT_STEPPER_ACCELERATION 2000
    #endif

    #ifndef AUTOPA_VERSION
        #define AUTOPA_VERSION 1
    #endif

    #ifdef OAM
        #ifndef ALT_ROD_PITCH
            #define ALT_ROD_PITCH 1.0  // mm/rev
        #endif
        // the Circumference of the AZ rotation. 209.1mm radius.
        #define ALT_CIRCUMFERENCE 209.1 * 2 * PI
        #define ALTITUDE_STEPS_PER_REV                                                                                                     \
            (ALT_CORRECTION_FACTOR * (ALT_CIRCUMFERENCE / ALT_ROD_PITCH) * ALT_STEPPER_SPR * ALT_MICROSTEPPING)  // Actually u-steps/rev

    #else
        // the Circumference of the AZ rotation. 770mm dia.
        #define ALT_CIRCUMFERENCE 2419.0f
        #if AUTOPA_VERSION == 1
            // the ratio of the ALT gearbox for AutoPA V1 (40:3)
            #define ALT_WORMGEAR_RATIO (40.0f / 3.0f)
        #else
            // the ratio of the ALT gearbox for AutoPA V2 (40:1)
            #define ALT_WORMGEAR_RATIO (40.0f)
        #endif
        #ifndef ALTITUDE_STEPS_PER_REV
            #define ALTITUDE_STEPS_PER_REV                                                                                                 \
                (ALT_CORRECTION_FACTOR * (ALT_CIRCUMFERENCE / (ALT_PULLEY_TEETH * GT2_BELT_PITCH)) * ALT_STEPPER_SPR * ALT_MICROSTEPPING   \
                 * ALT_WORMGEAR_RATIO)  // Actually u-steps/rev
        #endif
    #endif

    #ifndef ALTITUDE_STEPS_PER_ARC_MINUTE
        #define ALTITUDE_STEPS_PER_ARC_MINUTE (ALTITUDE_STEPS_PER_REV / (360 * 60.0f))  // Used to determine move distance in steps
    #endif

    // ALT TMC2209 UART settings
    // These settings work only with TMC2209 in UART connection (single wire to TX)
    #if (ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
        #define ALT_RMSCURRENT ALT_MOTOR_CURRENT_RATING *(ALT_OPERATING_CURRENT_SETTING / 100.0f) / 1.414f

        #ifndef ALT_MOTOR_HOLD_SETTING
            #define ALT_MOTOR_HOLD_SETTING 100
        #endif

        #define ALT_STALL_VALUE 10  // adjust this value if the RA autohoming sequence often false triggers, or triggers too late

        #ifndef USE_VREF
            #define USE_VREF                                                                                                               \
                0  //By default Vref is ignored when using UART to specify rms current. Only enable if you know what you are doing.
        #endif
    #endif
    #ifndef ALT_ALWAYS_ON
        #define ALT_ALWAYS_ON 0
    #endif
#endif

//////////////////////////////////////////
// Focuser support
//////////////////////////////////////////
// Enable focuser functionality in your local configuration. Do not edit here!
#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    #ifndef FOCUS_UART_STEALTH_MODE
        #define FOCUS_UART_STEALTH_MODE 1
    #endif
    #if FOCUS_DRIVER_TYPE != DRIVER_TYPE_NONE
        #ifndef FOCUS_MICROSTEPPING
            #define FOCUS_MICROSTEPPING 8
        #endif
    #else
        #error Unknown Focus driver type. Did you define FOCUS_DRIVER_TYPE?
    #endif
    #ifndef FOCUS_STEPPER_SPR
        #define FOCUS_STEPPER_SPR 400  // NEMA 0.9° = 400  |  NEMA 1.8° = 200
    #endif
    #ifndef FOCUS_STEPPER_SPEED
        #define FOCUS_STEPPER_SPEED 1000  // You can change the speed and acceleration of the steppers here. Max. Speed = 3000.
    #endif
    #ifndef FOCUS_STEPPER_ACCELERATION
        #define FOCUS_STEPPER_ACCELERATION 1000
    #endif

    // FOCUS TMC2209 UART settings
    // These settings work only with TMC2209 in UART connection (single wire to TX)
    #ifndef FOCUSER_ALWAYS_ON
        #define FOCUSER_ALWAYS_ON 0
    #endif

    #if (FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
        #define FOCUS_RMSCURRENT FOCUS_MOTOR_CURRENT_RATING *(FOCUS_OPERATING_CURRENT_SETTING / 100.0f) / 1.414f
        #ifndef FOCUSER_MOTOR_HOLD_SETTING
            #define FOCUSER_MOTOR_HOLD_SETTING 100
        #endif
        #define FOCUS_STALL_VALUE 1  // adjust this value if the Focus autohoming sequence often false triggers, or triggers too late
        #ifndef USE_VREF
            #define USE_VREF                                                                                                               \
                0  //By default Vref is ignored when using UART to specify rms current. Only enable if you know what you are doing.
        #endif
    #endif
#endif

//////////////////////////////////////////
// RA Homing support
//////////////////////////////////////////
// Enable homing in your local configuration. Do not edit here!
#ifndef USE_HALL_SENSOR_RA_AUTOHOME
    #define USE_HALL_SENSOR_RA_AUTOHOME 0
#elif USE_HALL_SENSOR_RA_AUTOHOME == 1
    #ifndef RA_HOMING_SENSOR_ACTIVE_STATE
        #define RA_HOMING_SENSOR_ACTIVE_STATE LOW
    #endif
    #ifndef RA_HOMING_SENSOR_SEARCH_DEGREES
        #define RA_HOMING_SENSOR_SEARCH_DEGREES 30
    #endif
#endif

//////////////////////////////////////////
// DEC Homing support
//////////////////////////////////////////
// Enable homing in your local configuration. Do not edit here!
#ifndef USE_HALL_SENSOR_DEC_AUTOHOME
    #define USE_HALL_SENSOR_DEC_AUTOHOME 0
#elif USE_HALL_SENSOR_DEC_AUTOHOME == 1
    #ifndef DEC_HOMING_SENSOR_ACTIVE_STATE
        #define DEC_HOMING_SENSOR_ACTIVE_STATE LOW
    #endif
    #ifndef DEC_HOMING_SENSOR_SEARCH_DEGREES
        #define DEC_HOMING_SENSOR_SEARCH_DEGREES 30
    #endif
#endif

// RA EndSwitch support
//////////////////////////////////////////
// Enable RA End Switches in your local configuration. Do not edit here!
#ifndef USE_RA_END_SWITCH
    #define USE_RA_END_SWITCH 0
#else
    #ifndef RA_END_SWITCH_ACTIVE_STATE
        #define RA_END_SWITCH_ACTIVE_STATE LOW
    #endif
    // You can define how many degrees to slew back after the end switch has triggered.
    // Mechanical end switches might have a hysteresis behavior, meaning once signaled,
    // it needs to move well back beyond the signal point to become un-signaled.
    #ifndef RA_ENDSWITCH_BACKSLEW_DEG
        #define RA_ENDSWITCH_BACKSLEW_DEG 0.5
    #endif
#endif

//////////////////////////////////////////
// DEC EndSwitch support
//////////////////////////////////////////
// Enable DEC End Switches in your local configuration. Do not edit here!
#ifndef USE_DEC_END_SWITCH
    #define USE_DEC_END_SWITCH 0
#else
    #ifndef DEC_END_SWITCH_ACTIVE_STATE
        #define DEC_END_SWITCH_ACTIVE_STATE LOW
    #endif
    // You can define how many degrees to slew back after the end switch has triggered.
    // Mechanical end switches might have a hysteresis behavior, meaning once signaled,
    // it needs to move well back beyond the signal point to become un-signaled.
    #ifndef DEC_ENDSWITCH_BACKSLEW_DEG
        #define DEC_ENDSWITCH_BACKSLEW_DEG 0.5
    #endif

#endif

//////////////////////////////////////////
// LCD Display support
//////////////////////////////////////////
// Enable LCD functionality in your local configuration. Do not edit here!
#if DISPLAY_TYPE != DISPLAY_TYPE_NONE

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                         ///
// FEATURE SUPPORT SECTION ///
//     FOR MOUNTS WITH     ///
//       LCD DISPLAY       ///
//                         ///
//////////////////////////////
//
// If you feel comfortable with configuring the OAT at startup manually, you should set
// SUPPORT_GUIDED_STARTUP to 0 (maybe after you've used it for a while you know what to do).
//
// The POI menu can take a little data memory and you may not need it. If not, you can set
// SUPPORT_POINTS_OF_INTEREST to 0
//
//
// Set this to 1 to support full GO menu.
// If this is set to 0 you still have a GO menu that has Home and Park.
    #ifndef SUPPORT_POINTS_OF_INTEREST
        #define SUPPORT_POINTS_OF_INTEREST 1
    #endif

    // Set this to 1 to support Guided Startup
    #ifndef SUPPORT_GUIDED_STARTUP
        #define SUPPORT_GUIDED_STARTUP 1
    #endif

// Set this to 1 to support CTRL menu, allowing you to manually slew the mount with the buttons.
    #define SUPPORT_MANUAL_CONTROL 1

// Set this to 1 to support CAL menu, allowing you to calibrate various things
    #define SUPPORT_CALIBRATION 1

// Set this to 1 to support INFO menu that displays various pieces of information about the mount.
    #define SUPPORT_INFO_DISPLAY 1

#else  // No Display section

    #define SUPPORT_POINTS_OF_INTEREST 0
    #if SUPPORT_GUIDED_STARTUP == 1
        #error "Guided startup is only available with a display."
    #endif
    #define SUPPORT_GUIDED_STARTUP 0
    #define SUPPORT_MANUAL_CONTROL 0
    #define SUPPORT_CALIBRATION    0
    #define SUPPORT_INFO_DISPLAY   0
    #if SUPPORT_DRIFT_ALIGNMENT == 1
        #error "Drift Alignment is only available with a display."
    #endif
    #define SUPPORT_DRIFT_ALIGNMENT 0
#endif  // DISPLAY_TYPE

// Enable Meade protocol communication over serial
#if !defined(SUPPORT_SERIAL_CONTROL)
    #define SUPPORT_SERIAL_CONTROL 1
#endif

// The port number to access OAT control over WiFi (ESP32 only)
#define WIFI_PORT 4030

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                  ////////
// OTHER HARDWARE CONFIGURATION     ////////
//                                  ////////
////////////////////////////////////////////

// Stepper drivers
#if (RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
    #if defined(ESP32)
        #define RA_SERIAL_PORT Serial2  // Can be shared with DEC_SERIAL_PORT
    #elif defined(__AVR_ATmega2560__)
    // Uses SoftwareSerial
    #endif
#endif

#if (DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
    #if defined(ESP32)
        #define DEC_SERIAL_PORT Serial2  // Can be shared with RA_SERIAL_PORT
    #elif defined(__AVR_ATmega2560__)
    // Uses SoftwareSerial
    #endif
#endif

// Focuser
#if (FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
    #if defined(ESP32)
        #define FOCUS_SERIAL_PORT Serial2  // Can be shared with RA_SERIAL_PORT
    #elif defined(__AVR_ATmega2560__)
    // Uses SoftwareSerial
    #endif
#endif

// GPS
#if USE_GPS == 1
    #define GPS_BAUD_RATE 9600
#endif

////////////////////////////
//
// DEBUG OUTPUT
//
// #define DEBUG_LEVEL (DEBUG_STEPPERS|DEBUG_MOUNT)
// #define DEBUG_LEVEL (DEBUG_INFO|DEBUG_MOUNT|DEBUG_GENERAL)
// #define DEBUG_LEVEL (DEBUG_SERIAL|DEBUG_WIFI|DEBUG_INFO|DEBUG_MOUNT|DEBUG_GENERAL)
// #define DEBUG_LEVEL (DEBUG_ANY)
// #define DEBUG_LEVEL (DEBUG_INFO|DEBUG_MOUNT|DEBUG_GENERAL)
//
// Bit Name                 Output
//  0  DEBUG_INFO           General output, like startup variables and status
//  1  DEBUG_SERIAL         Serial commands and replies
//  2  DEBUG_WIFI           Wifi related output
//  3  DEBUG_MOUNT          Mount processing output
//  4  DEBUG_MOUNT_VERBOSE  Verbose mount processing (coordinates, etc)
//  5  DEBUG_GENERAL        Other misc. output
//  6  DEBUG_MEADE          Meade command handling output
// Set this to specify the amount of debug output OAT should send to the serial port.
// Note that if you use an app to control OAT, ANY debug output will likely confuse that app.
// Debug output is useful if you are using Wifi to control the OAT or if you are issuing
// manual commands via a terminal only.
//
#ifndef DEBUG_LEVEL
    #define DEBUG_LEVEL (DEBUG_NONE)
#endif

#ifndef DEBUG_SEPARATE_SERIAL
    #define DEBUG_SEPARATE_SERIAL 0
#endif

#ifndef DEBUG_SERIAL_BAUDRATE
    #define DEBUG_SERIAL_BAUDRATE 115200
#endif

#if defined(OAT_DEBUG_BUILD)
    // AVR based boards have numbers < 1000
    #if BOARD < 1000
        /*
     * Debugging on the mega2560 using avr-stub dissallows application-code from
     * using the normal (USB) serial port
     */
        // Disable debug output
        #if defined(DEBUG_LEVEL)
            #undef DEBUG_LEVEL
        #endif
        #define DEBUG_LEVEL (DEBUG_NONE)

        // Disable serial control
        #if defined(SUPPORT_SERIAL_CONTROL)
            #undef SUPPORT_SERIAL_CONTROL
        #endif
        #define SUPPORT_SERIAL_CONTROL 0
    #else
        #error "Debugging not supported on this platform"
    #endif
#endif
