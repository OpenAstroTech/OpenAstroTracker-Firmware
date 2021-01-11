#pragma once

/**
 * This file contains advanced configurations. Edit values here only if you know what you are doing. Invalid values
 * can lead to OAT misbehaving very bad and in worst case could even lead to hardware damage. The default values here
 * were chosen after many tests and can are currently concidered to work the best.
 **/

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                            ////////
// MOTOR & DRIVER SETTINGS    ////////
//                            ////////
//////////////////////////////////////
// 
// This is how many steps your stepper needs for a full rotation.
#if RA_STEPPER_TYPE == STEPPER_TYPE_28BYJ48
  #define RA_STEPPER_SPR            4096  // 28BYJ-48 = 4096  |  NEMA 0.9° = 400  |  NEMA 1.8° = 200
  #ifndef RA_STEPPER_SPEED
    #define RA_STEPPER_SPEED          400   // You can change the speed and acceleration of the steppers here. Max. Speed = 600. 
  #endif
  #ifndef RA_STEPPER_ACCELERATION
    #define RA_STEPPER_ACCELERATION   600   // High speeds tend to make these cheap steppers unprecice
  #endif
#elif RA_STEPPER_TYPE == STEPPER_TYPE_NEMA17
  #define RA_STEPPER_SPR            400   // 28BYJ-48 = 4096  |  NEMA 0.9° = 400  |  NEMA 1.8° = 200
  #ifndef RA_STEPPER_SPEED
    #define RA_STEPPER_SPEED          1200  // You can change the speed and acceleration of the steppers here. Max. Speed = 3000. 
  #endif
  #ifndef RA_STEPPER_ACCELERATION
    #define RA_STEPPER_ACCELERATION   6000
  #endif
#else
  #error New RA Stepper type? Add it here...
#endif

#if DEC_STEPPER_TYPE == STEPPER_TYPE_28BYJ48
  #define DEC_STEPPER_SPR            4096  // 28BYJ-48 = 4096  |  NEMA 0.9° = 400  |  NEMA 1.8° = 200
  #ifndef DEC_STEPPER_SPEED
    #define DEC_STEPPER_SPEED          600   // You can change the speed and acceleration of the steppers here. Max. Speed = 600. 
  #endif
  #ifndef DEC_STEPPER_ACCELERATION
    #define DEC_STEPPER_ACCELERATION   400   // High speeds tend to make these cheap steppers unprecice
  #endif
#elif DEC_STEPPER_TYPE == STEPPER_TYPE_NEMA17
  #define DEC_STEPPER_SPR            400   // 28BYJ-48 = 4096  |  NEMA 0.9° = 400  |  NEMA 1.8° = 200
  #ifndef DEC_STEPPER_SPEED
    #define DEC_STEPPER_SPEED          1300  // You can change the speed and acceleration of the steppers here. Max. Speed = 3000. 
  #endif
  #ifndef DEC_STEPPER_ACCELERATION
    #define DEC_STEPPER_ACCELERATION   6000
  #endif
#else
  #error New DEC Stepper type? Add it here...
#endif

// MICROSTEPPING
// The DRIVER_TYPE_TMC2209_UART driver can dynamically switch between microstep settings.
// The DRIVER_TYPE_ULN2003 is essentially two different (software) drivers for RA (full-step) & TRK (half-step).
// All other drivers are set by the MS pins, therefore values here must match pin strapping.
// Valid values: 1, 2, 4, 8, 16, 32, 64, 128, 256
// !! Values >=32 are only supported by the TMC2209 driver.
//
// NOTE: The previous implmentation allowed dynamic switching between microstep rates for slew and tracking 
// if the TMC2209 driver was used. This was intended to allow fine control when tracking & guiding.
// Unfortunately it breaks most functionality that depends on AccelStepper::position(): essentially AccelStepper
// counts steps, and is unaware of the microstep mode configured in TMC2209Stepper, which in turn affects angle moved per step.
// Therefore dynamically changing microstep mode causes errors when deriving angle from AccelStepper::position(). 
// Even though the ULN2003 does not support microstepping, using different modes (full/half-step) between
// tracking/guiding & slewing has the problem.
// Consequently slewing, tracking, guiding now use the same microstep configuration regardless of mode.
// We plan to re-instate fine modes in a future release, but this will require a significant rework of the implementation.
//
#if (RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
  #define RA_SLEW_MICROSTEPPING 8         // The (default) microstep mode used for slewing RA axis
  #define RA_TRACKING_MICROSTEPPING 8     // The fine microstep mode for tracking RA axis
#elif (RA_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC) || (RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE)
  #define RA_SLEW_MICROSTEPPING 8         // Microstep mode set by MS pin strapping. Use the same microstep mode for both slewing & tracking   
  #define RA_TRACKING_MICROSTEPPING RA_SLEW_MICROSTEPPING   
#elif (RA_DRIVER_TYPE == DRIVER_TYPE_ULN2003)
  #define RA_SLEW_MICROSTEPPING 2         // The (default) half-step mode used for slewing RA axis
  #define RA_TRACKING_MICROSTEPPING 2     // The fine half-step mode for tracking RA axis
#else
  #error Unknown RA driver type
#endif

#if (DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
  #define DEC_SLEW_MICROSTEPPING  16  // The (default) microstep mode used for slewing DEC
  #define DEC_GUIDE_MICROSTEPPING 16  // The fine microstep mode used for guiding DEC only
#elif (DEC_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC) || (DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE)
  #define DEC_SLEW_MICROSTEPPING  16  // Only UART drivers support dynamic switching. Use the same microstep mode for both slewing & guiding
  #define DEC_GUIDE_MICROSTEPPING DEC_SLEW_MICROSTEPPING
#elif (DEC_DRIVER_TYPE == DRIVER_TYPE_ULN2003)
  #define DEC_SLEW_MICROSTEPPING   2  // Runs in half-step mode always
  #define DEC_GUIDE_MICROSTEPPING DEC_SLEW_MICROSTEPPING
#else
  #error Unknown DEC driver type
#endif

// Extended TMC2209 UART settings
// These settings work only with TMC2209 in UART connection (single wire to TX)
#if (DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
  #define RA_RMSCURRENT 1200       // RMS current in mA. Warning: Peak current will be 1.414 times higher!! Do not exceed your steppers max current!
  #define RA_STALL_VALUE 100       // adjust this value if the RA autohoming sequence often false triggers, or triggers too late

  #define DEC_STALL_VALUE 10    // adjust this value if the RA autohoming sequence often false triggers, or triggers too late
  #define DEC_RMSCURRENT 1000   // RMS current in mA. Warning: Peak current will be 1.414 times higher!! Do not exceed your steppers max current!
  #define DEC_HOLDCURRENT 20    // [0, ... , 31] x/32 of the run current when standing still. 0=1/32... 31=32/32
  #define USE_AUTOHOME 0        // Autohome with TMC2209 stall detection:  ON = 1  |  OFF = 0   
  //                  ^^^ leave at 0 for now, doesnt work properly yet
  #define RA_AUDIO_FEEDBACK  0 // If one of these are set to 1, the respective driver will shut off the stealthchop mode, resulting in a audible whine
  #define DEC_AUDIO_FEEDBACK 0 // of the stepper coils. Use this to verify that UART is working properly. 
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                        ////////
// MECHANICS SETTINGS     ////////
//                        ////////
//////////////////////////////////
// 

// the pitch of the GT2 timing belt
#define GT2_BELT_PITCH  2.0 // mm

// the Circumference of the RA wheel.  V1 = 1057.1  |  V2 = 1131.0
#if RA_WHEEL_VERSION == 1
  #define RA_WHEEL_CIRCUMFERENCE 1057.1
#elif RA_WHEEL_VERSION >= 2
  #define RA_WHEEL_CIRCUMFERENCE 1132.73
#else
  #error Unsupported RA wheel version, please recheck RA_WHEEL_VERSION
#endif

// the Circumference of the DEC wheel.
#define DEC_WHEEL_CIRCUMFERENCE 565.5

// RA movement:
// The radius of the surface that the belt runs on (in V1 of the ring) was 168.24mm.
// Belt moves 40mm for one stepper revolution (2mm pitch, 20 teeth).
// RA wheel is 2 x PI x 168.24mm (V2:180mm) circumference = 1057.1mm (V2:1131mm)
// One RA revolution needs 26.43 (1057.1mm / 40mm) stepper revolutions (V2: 28.27 (1131mm/40mm))
// Which means 108245 steps (26.43 x 4096) moves 360 degrees (V2: 115812 steps (28.27 x 4096))
// So there are 300.1 steps/degree (108245 / 360)  (V2: 322 (115812 / 360))
// Theoretically correct RA tracking speed is 1.246586 (300 x 14.95903 / 3600) (V2 : 1.333800 (322 x 14.95903 / 3600) steps/sec (this is for 20T)
// Include microstepping ratio here such that steps/sec is updates/sec to stepper driver
#define RA_STEPS_PER_DEGREE   (RA_WHEEL_CIRCUMFERENCE / (RA_PULLEY_TEETH * GT2_BELT_PITCH) * RA_STEPPER_SPR * RA_SLEW_MICROSTEPPING / 360.0)

// DEC movement:
// Belt moves 40mm for one stepper revolution (2mm pitch, 20 teeth).
// DEC wheel is 2 x PI x 90mm circumference which is 565.5mm
// One DEC revolution needs 14.13 (565.5mm/40mm) stepper revolutions
// Which means 57907 steps (14.14 x 4096) moves 360 degrees
// So there are 160.85 steps/degree (57907/360) (this is for 20T)
// Include microstepping ratio here such that steps/sec is updates/sec to stepper driver
#define DEC_STEPS_PER_DEGREE  (DEC_WHEEL_CIRCUMFERENCE / (DEC_PULLEY_TEETH * GT2_BELT_PITCH) * DEC_STEPPER_SPR * DEC_SLEW_MICROSTEPPING / 360.0)

////////////////////////////
//
// GUIDE SETTINGS
// This is the multiplier of the normal tracking speed that a guiding pulse will have. 
// Note that the North & South (DEC) tracking speed is calculated as the +multiplier & -multiplier
// Note that the West & East (RA) tracking speed is calculated as the (multiplier+1.0) & (multiplier-1.0)
#if RA_STEPPER_TYPE == STEPPER_TYPE_28BYJ48
  #define RA_PULSE_MULTIPLIER 1.0
#elif RA_STEPPER_TYPE == STEPPER_TYPE_NEMA17
  #define RA_PULSE_MULTIPLIER 1.5
#else
  #error New RA Stepper type? Add it here...
#endif

#if DEC_STEPPER_TYPE == STEPPER_TYPE_28BYJ48
  #define DEC_PULSE_MULTIPLIER 1.0
#elif DEC_STEPPER_TYPE == STEPPER_TYPE_NEMA17
  #define DEC_PULSE_MULTIPLIER 1.0
#else
  #error New DEC Stepper type? Add it here...
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
#define LCD_BUTTON_TEST 0


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                     ///
// HARDWARE EXTENSIONS SUPPORT SECTION ///
//                                     ///
//////////////////////////////////////////
//

// Enable Azimuth and Altitude motor functionality in Configuration.hpp
#ifdef AZIMUTH_ALTITUDE_MOTORS

  #if AZ_DRIVER_TYPE == DRIVER_TYPE_ULN2003
    #define AZ_MICROSTEPPING        2     // Halfstep mode using ULN2003 driver
  #elif AZ_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC || AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE || AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    #define AZ_MICROSTEPPING        32
  #else
    #error Unknown AZ driver type
  #endif
  #if AZ_STEPPER_TYPE == STEPPER_TYPE_28BYJ48
    #define AZ_STEPPER_SPR            2048  // 28BYJ-48 in full step mode
    #define AZ_STEPPER_SPEED          600   // You can change the speed and acceleration of the steppers here. Max. Speed = 600. 
    #define AZ_STEPPER_ACCELERATION   400   // High speeds tend to make these cheap steppers unprecice
  #elif AZ_STEPPER_TYPE == STEPPER_TYPE_NEMA17
    #define AZ_STEPPER_SPR            400   // NEMA 0.9° = 400  |  NEMA 1.8° = 200
    #define AZ_STEPPER_SPEED          600  // You can change the speed and acceleration of the steppers here. Max. Speed = 3000. 
    #define AZ_STEPPER_ACCELERATION   1000
  #else
    #error Unknown AZ stepper type
  #endif


  #if ALT_DRIVER_TYPE == DRIVER_TYPE_ULN2003
    #define ALT_MICROSTEPPING        1     // Fullstep mode using ULN2003 driver
  #elif ALT_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC || ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE || ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    #define ALT_MICROSTEPPING        32
  #else
    #error Unknown ALT driver type
  #endif
  #if ALT_STEPPER_TYPE == STEPPER_TYPE_28BYJ48
    #define ALT_STEPPER_SPR            2048  // 28BYJ-48 in full step mode
    #define ALT_STEPPER_SPEED          600   // You can change the speed and acceleration of the steppers here. Max. Speed = 600. 
    #define ALT_STEPPER_ACCELERATION   400   // High speeds tend to make these cheap steppers unprecice
  #elif ALT_STEPPER_TYPE == STEPPER_TYPE_NEMA17
    #define ALT_STEPPER_SPR            400   // NEMA 0.9° = 400  |  NEMA 1.8° = 200
    #define ALT_STEPPER_SPEED          600  // You can change the speed and acceleration of the steppers here. Max. Speed = 3000. 
    #define ALT_STEPPER_ACCELERATION   1000
  #else
    #error Unknown ALT stepper type
  #endif


  // the Circumference of the AZ rotation. 808mm dia.
  #define AZ_CIRCUMFERENCE 2538.4
  // the Circumference of the AZ rotation. 770mm dia.
  #define ALT_CIRCUMFERENCE 2419
  // the ratio of the ALT gearbox (40:3)
  #define ALT_WORMGEAR_RATIO (40/3)

  #define AZIMUTH_STEPS_PER_REV           (AZ_CORRECTION_FACTOR * (AZ_CIRCUMFERENCE / (AZ_PULLEY_TEETH * GT2_BELT_PITCH)) * AZ_STEPPER_SPR * AZ_MICROSTEPPING)   // Actually u-steps/rev
  #define ALTITUDE_STEPS_PER_REV          (ALT_CORRECTION_FACTOR * (ALT_CIRCUMFERENCE / (ALT_PULLEY_TEETH * GT2_BELT_PITCH)) * ALT_STEPPER_SPR * ALT_MICROSTEPPING * ALT_WORMGEAR_RATIO)   // Actually u-steps/rev
  #define AZIMUTH_STEPS_PER_ARC_MINUTE    (AZIMUTH_STEPS_PER_REV / (360 * 60.0f)) // Used to determine move distance in steps
  #define ALTITUDE_STEPS_PER_ARC_MINUTE   (ALTITUDE_STEPS_PER_REV / (360 * 60.0f)) // Used to determine move distance in steps

  // ALT/AZ TMC2209 UART settings
  // These settings work only with TMC2209 in UART connection (single wire to TX)
  #define AZ_AUDIO_FEEDBACK 0 // of the stepper coils. Use this to verify that UART is working properly. 
  #define ALT_AUDIO_FEEDBACK 0 // of the stepper coils. Use this to verify that UART is working properly.
  #define AZ_STALL_VALUE 10    // adjust this value if the RA autohoming sequence often false triggers, or triggers too late
  #define AZ_RMSCURRENT 1000   // RMS current in mA. Warning: Peak current will be 1.414 times higher!! Do not exceed your steppers max current!
  #define ALT_STALL_VALUE 10    // adjust this value if the RA autohoming sequence often false triggers, or triggers too late
  #define ALT_RMSCURRENT 1000   // RMS current in mA. Warning: Peak current will be 1.414 times higher!! Do not exceed your steppers max current!

#endif

#if DISPLAY_TYPE != DISPLAY_TYPE_NONE

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                         ///
// FEATURE SUPPORT SECTION ///
//                         ///
//////////////////////////////
// Since the Arduino Uno has very little memory (32KB code, 2KB data) all features
// stretch the Uno a little too far. So in order to save memory we allow you to enable 
// and disable features to help manage memory usage.
// If you run the tracker with an Arduino Mega, you can set all the features to 1.
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
  #define SUPPORT_POINTS_OF_INTEREST   1

// Set this to 1 to support Guided Startup 
  #define SUPPORT_GUIDED_STARTUP       1

// Set this to 1 to support CTRL menu, allowing you to manually slew the mount with the buttons. 
  #define SUPPORT_MANUAL_CONTROL       1

// Set this to 1 to support CAL menu, allowing you to calibrate various things
  #define SUPPORT_CALIBRATION          1

// Set this to 1 to support INFO menu that displays various pieces of information about the mount. 
  #define SUPPORT_INFO_DISPLAY         1

#endif  // DISPLAY_TYPE

// Enable Meade protocol communication over serial
#define SUPPORT_SERIAL_CONTROL 1

// This is set to 1 for boards that do not support interrupt timers
#define RUN_STEPPERS_IN_MAIN_LOOP 0

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
    #define RA_SERIAL_PORT Serial2    // Can be shared with DEC_SERIAL_PORT
  #elif defined(__AVR_ATmega2560__)
    // Uses SoftwareSerial
  #endif
#endif

#if (DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
  #if defined(ESP32)
    #define DEC_SERIAL_PORT Serial2   // Can be shared with RA_SERIAL_PORT
  #elif defined(__AVR_ATmega2560__)
    // Uses SoftwareSerial
  #endif
#endif

// GPS
#if USE_GPS == 1
  #if defined(ESP32)
    #define GPS_SERIAL_PORT Serial2 // TODO: Resolve potential conflict with RA_SERIAL_PORT & DEC_SERIAL_PORT
    #define GPS_BAUD_RATE 9600
  #elif defined(__AVR_ATmega2560__)
    #define GPS_SERIAL_PORT Serial1
    #define GPS_BAUD_RATE 9600
  #endif
#endif

