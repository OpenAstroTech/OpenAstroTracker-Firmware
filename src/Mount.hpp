#ifndef _MOUNT_HPP_
#define _MOUNT_HPP_

#include "Declination.hpp"
#include "Latitude.hpp"
#include "Longitude.hpp"
#include "Types.hpp"

#if (INFO_DISPLAY_TYPE != INFO_DISPLAY_TYPE_NONE)
class InfoDisplayRender;
#endif

#ifdef NEW_STEPPER_LIB

    #include "StepperConfiguration.hpp"

    // Forward declarations
    #ifdef ARDUINO_AVR_ATmega2560
using StepperRaSlew  = InterruptAccelStepper<config::Ra::stepper_slew>;
using StepperRaTrk   = InterruptAccelStepper<config::Ra::stepper_trk>;
using StepperDecSlew = InterruptAccelStepper<config::Dec::stepper_slew>;
using StepperDecTrk  = InterruptAccelStepper<config::Dec::stepper_trk>;

        #if AZ_STEPPER_TYPE != STEPPER_TYPE_NONE
using StepperAzSlew = InterruptAccelStepper<config::Az::stepper_slew>;
        #endif

        #if ALT_STEPPER_TYPE != STEPPER_TYPE_NONE
using StepperAltSlew = InterruptAccelStepper<config::Alt::stepper_slew>;
        #endif

        #if FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE
using StepperFocusSlew = InterruptAccelStepper<config::Focus::stepper_slew>;
        #endif

    #else
        #include "AccelStepper.h"
class AccelStepper;
using StepperRaSlew    = AccelStepper;
using StepperRaTrk     = AccelStepper;
using StepperDecSlew   = AccelStepper;
using StepperDecTrk    = AccelStepper;

        #if AZ_STEPPER_TYPE != STEPPER_TYPE_NONE
using StepperAzSlew    = AccelStepper;
        #endif

        #if ALT_STEPPER_TYPE != STEPPER_TYPE_NONE
using StepperAltSlew   = AccelStepper;
        #endif

        #if ALT_STEPPER_TYPE != STEPPER_TYPE_NONE
using StepperAltSlew   = AccelStepper;
        #endif

        #if FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE
using StepperFocusSlew = AccelStepper;
        #endif

    #endif
#else
    #include "AccelStepper.h"
class AccelStepper;
using StepperRaSlew    = AccelStepper;
using StepperRaTrk     = AccelStepper;
using StepperDecSlew   = AccelStepper;
using StepperDecTrk    = AccelStepper;

    #if AZ_STEPPER_TYPE != STEPPER_TYPE_NONE
using StepperAzSlew    = AccelStepper;
    #endif

    #if ALT_STEPPER_TYPE != STEPPER_TYPE_NONE
using StepperAltSlew   = AccelStepper;
    #endif

    #if ALT_STEPPER_TYPE != STEPPER_TYPE_NONE
using StepperAltSlew   = AccelStepper;
    #endif

    #if FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE
using StepperFocusSlew = AccelStepper;
    #endif
#endif

class LcdMenu;
class TMC2209Stepper;
class HallSensorHoming;
class EndSwitch;

#define NORTH          B00000001
#define EAST           B00000010
#define SOUTH          B00000100
#define WEST           B00001000
#define ALL_DIRECTIONS B00001111
#define TRACKING       B00010000
#define FOCUSING       B00100000

#define LCDMENU_STRING     B0001
#define MEADE_STRING       B0010
#define PRINT_STRING       B0011
#define LCD_STRING         B0100
#define COMPACT_STRING     B0101
#define FORMAT_STRING_MASK B0111

#define TARGET_STRING  B01000
#define CURRENT_STRING B10000

//mountstatus
#define STATUS_PARKED            0B0000000000000000
#define STATUS_SLEWING           0B0000000000000010
#define STATUS_SLEWING_TO_TARGET 0B0000000000000100
#define STATUS_SLEWING_FREE      0B0000000000000010
#define STATUS_SLEWING_MANUAL    0B0000000100000000
#define STATUS_TRACKING          0B0000000000001000
#define STATUS_PARKING           0B0000000000010000
#define STATUS_PARKING_POS       0B0001000000000000
#define STATUS_GUIDE_PULSE       0B0000000010000000
#define STATUS_GUIDE_PULSE_DIR   0B0000000001100000
#define STATUS_GUIDE_PULSE_RA    0B0000000001000000
#define STATUS_GUIDE_PULSE_DEC   0B0000000000100000
#define STATUS_GUIDE_PULSE_MASK  0B0000000011100000
#define STATUS_FINDING_HOME      0B0010000000000000

struct LocalDate {
    int year;
    int month;
    int day;
};

// Focuser support
enum FocuserMode
{
    FOCUS_IDLE,
    FOCUS_TO_TARGET,
    FOCUS_CONTINUOUS,
};

enum FocuserDirection
{
    FOCUS_BACKWARD = -1,
    FOCUS_FORWARD  = 1
};

//////////////////////////////////////////////////////////////////
//
// Class that represent the OpenAstroTracker mount, with all its parameters, motors, etc.
//
//////////////////////////////////////////////////////////////////
class Mount
{
  public:
    Mount(LcdMenu *lcdMenu);

    void initializeVariables();

    // Configure the RA stepper motor. This also sets up the TRK stepper on the same pins.
    void configureRAStepper(byte pin1, byte pin2, uint32_t maxSpeed, uint32_t maxAcceleration);

    void configureHemisphere(bool isNorthern, bool force = false);

    // Configure the DEC stepper motor.
    void configureDECStepper(byte pin1, byte pin2, uint32_t maxSpeed, uint32_t maxAcceleration);

// Configure the AZ stepper motors.
#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    void configureAZStepper(byte pin1, byte pin2, int maxSpeed, int maxAcceleration);
#endif

// Configure the ALT stepper motors.
#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    void configureALTStepper(byte pin1, byte pin2, int maxSpeed, int maxAcceleration);
#endif

// Configure the Focus stepper motors.
#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    void configureFocusStepper(byte pin1, byte pin2, int maxSpeed, int maxAcceleration);
#endif

#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART || DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART                                              \
    || AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART || ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART                                           \
    || FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    bool connectToDriver(const String &driverKind, uint16_t *rmsCurrent = nullptr);

#endif
#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
        // Configure the RA Driver (TMC2209 UART only)
    #if SW_SERIAL_UART == 0
    void configureRAdriver(Stream *serial, float rsense, byte driveraddress, int rmscurrent, int stallvalue);
    #elif SW_SERIAL_UART == 1
    void configureRAdriver(uint16_t RA_SW_RX, uint16_t RA_SW_TX, float rsense, byte driveraddress, int rmscurrent, int stallvalue);
    #endif
#endif
#if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    // Configure the DEC Driver (TMC2209 UART only)
    #if SW_SERIAL_UART == 0
    void configureDECdriver(Stream *serial, float rsense, byte driveraddress, int rmscurrent, int stallvalue);
    #elif SW_SERIAL_UART == 1
    void configureDECdriver(uint16_t DEC_SW_RX, uint16_t DEC_SW_TX, float rsense, byte driveraddress, int rmscurrent, int stallvalue);
    #endif
#endif

// Configure the AZ driver.
#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    #if AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
        #if SW_SERIAL_UART == 0
    void configureAZdriver(Stream *serial, float rsense, byte driveraddress, int rmscurrent, int stallvalue);
        #elif SW_SERIAL_UART == 1
    void configureAZdriver(uint16_t AZ_SW_RX, uint16_t AZ_SW_TX, float rsense, byte driveraddress, int rmscurrent, int stallvalue);
        #endif
    #endif
#endif

// Configure the ALT driver.
#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    #if ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
        #if SW_SERIAL_UART == 0
    void configureALTdriver(Stream *serial, float rsense, byte driveraddress, int rmscurrent, int stallvalue);
        #elif SW_SERIAL_UART == 1
    void configureALTdriver(uint16_t AlT_SW_RX, uint16_t ALT_SW_TX, float rsense, byte driveraddress, int rmscurrent, int stallvalue);
        #endif
    #endif
#endif

// Configure the Focus driver.
#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    #if FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
        #if SW_SERIAL_UART == 0
    void configureFocusDriver(Stream *serial, float rsense, byte driveraddress, int rmscurrent, int stallvalue);
        #elif SW_SERIAL_UART == 1
    void configureFocusDriver(uint16_t FOCUS_SW_RX, uint16_t FOCUS_SW_TX, float rsense, byte driveraddress, int rmscurrent, int stallvalue);
        #endif
    #endif
#endif

    // Get the current RA tracking speed factor
    float getSpeedCalibration();

    // Set the current RA tracking speed factor
    void setSpeedCalibration(float val, bool saveToStorage);

#if USE_GYRO_LEVEL == 1
    // Get the current pitch angle calibraton
    float getPitchCalibrationAngle();

    // Set the current pitch angle calibration
    void setPitchCalibrationAngle(float angle);

    // Get the current roll angle calibration
    float getRollCalibrationAngle();

    // Set the current pitch angle calibration
    void setRollCalibrationAngle(float angle);
#endif

    // Returns the number of slew microsteps the given motor turns to move one degree
    float getStepsPerDegree(StepperAxis which);

    // Function to set the number of slew microsteps the given motor turns to move one
    // degree for each axis. This function stores the value in persistent storage
    void setStepsPerDegree(StepperAxis which, float steps);

    // Sets the slew rate of the mount. rate is between 1 (slowest) and 4 (fastest)
    void setSlewRate(int rate);
    int getSlewRate();

    // Set the HA time (HA is derived from LST, the setter calculates and sets LST)
    void setHA(const DayTime &haTime);
    const DayTime HA() const;

    // Set the LST time (HA is derived from LST)
    void setLST(const DayTime &haTime);

    void setLatitude(Latitude lat);
    void setLongitude(Longitude lon);
    const Latitude latitude() const;
    const Longitude longitude() const;

    // Get a reference to the target RA value.
    DayTime &targetRA();

    // Get a reference to the target DEC value.
    Declination &targetDEC();

    // Get current RA value.
    const DayTime currentRA() const;

    // Get current DEC value.
    const Declination currentDEC() const;

    // Set the current RA and DEC position to be the given coordinates
    void syncPosition(DayTime ra, Declination dec);

    void calculateStepperPositions(float raCoord, float decCoord, long &raPos, long &decPos);

    // Calculates movement parameters and program steppers to move
    // there. Must call loop() frequently to actually move.
    void startSlewingToTarget();

    // Sends the mount to the home position
    void startSlewingToHome();

    // Move AZ and ALT motors to their zero position
    void moveAZALTToHome();
    void getAZALTPositions(long &azPos, long &altPos);
    void setAZALTHome();

    // Various status query functions
    bool isSlewingRAorDEC() const;
    bool isSlewingIdle() const;
    bool isSlewingTRK() const;
    bool isParking() const;
    bool isGuiding() const;
    bool isFindingHome() const;
    bool isAxisRunning(StepperAxis axis);
#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    bool isRunningAZ() const;
#endif
#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    bool isRunningALT() const;
#endif

#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    bool isRunningFocus() const;
    float getFocusSpeed() const;
#endif

    // Starts manual slewing in one of eight directions or tracking
    void startSlewing(int direction);

    // Stop manual slewing in one of two directions or tracking. NS is the same. EW is the same
    void stopSlewing(int direction);
    void stopSlewing(StepperAxis axis);

    // Block until the motors specified (NORTH, EAST, TRACKING, etc.) are stopped
    void waitUntilStopped(byte direction);

    // Same as Arduino delay() but keeps the tracker going.
    void delay(int ms);

    // Gets the position in one of eight directions or tracking
    long getCurrentStepperPosition(int direction);
    long getCurrentStepperPosition(StepperAxis axis);

    // Set the tracking stepper position
    void setTrackingStepperPos(long stepPos);

    // Process any stepper movement.
    void loop();

// Low-level process any stepper movement on interrupt callback.
#if defined(ESP32) || !defined(NEW_STEPPER_LIB)
    void interruptLoop();
#endif

    // Set the current stepper positions to be home.
    void setHome(bool clearZeroPos);

    // Set the DEC limit position to the given angle in degrees (saved as DEC steps).
    // If upper is true, sets the upper limit, else the lower limit.
    // If limitAngle is 0, limit is set to current position.
    void setDecLimitPosition(bool upper, float limitAngle = 0);

    // Clear the DEC limit position. If upper is true, clears upper limit, else the lower limit.
    void clearDecLimitPosition(bool upper);

    // Get the DEC limit positions
    void getDecLimitPositions(float &lowerLimit, float &upperLimit);

    // Asynchronously parks the mount. Moves to the home position and stops all motors.
    void park();

    // Runs the RA motor at twice the speed (or stops it), or the DEC motor at tracking speed for the given duration in ms.
    void guidePulse(byte direction, int duration);

    // Stops given guide operations in progress.
    void stopGuiding(bool ra = true, bool dec = true);

    // Return a string of DEC in the given format. For LCDSTRING, active determines where the cursor is
    String DECString(byte type, byte active = 0);

    // Return a string of DEC in the given format. For LCDSTRING, active determines where the cursor is
    String RAString(byte type, byte active = 0);

    // Returns string (singfle word) representing the mounts status.
    String getStatusStateString();

    // Returns a comma-delimited string with all the mounts' information
    String getStatusString();

    void setStatusFlag(int flag);
    void clearStatusFlag(int flag);

    // Get the current speed of the stepper. NORTH, WEST, TRACKING
    float getSpeed(int direction);

    // See if a slew to target is in progress and return the percentage along the path
    bool getStepperProgress(int &raPercentage, int &decPercentage);

    // Displays the current location of the mount every n ms, where n is defined in Globals.h as DISPLAY_UPDATE_TIME
    void displayStepperPositionThrottled();
#if (INFO_DISPLAY_TYPE != INFO_DISPLAY_TYPE_NONE)
    void setupInfoDisplay();
    void updateInfoDisplay();
    InfoDisplayRender *getInfoDisplay();
    long _loops;
#endif

    // Called by Meade processor every time a command is received.
    void commandReceived();
    long getNumCommandsReceived()
    {
        return _commandReceived;
    }

#if SUPPORT_DRIFT_ALIGNMENT == 1
    // Runs a phase of the drift alignment procedure
    void runDriftAlignmentPhase(int direction, int durationSecs);
#endif

    // Toggle the state where we run the motors at a constant speed
    void setManualSlewMode(bool state);

    // Set the speed of the given motor
    void setSpeed(StepperAxis which, float speedDegsPerSec);

#if (USE_RA_END_SWITCH == 1) || (USE_DEC_END_SWITCH == 1)
    void setupEndSwitches();
#endif

#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE) || (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    // Support for moving the mount in azimuth and altitude (requires extra hardware)
    void moveBy(int direction, float arcMinutes);
    void disableAzAltMotors();
    void enableAzAltMotors();
#endif

#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    // Support for focus motor (requires extra hardware)
    void focusSetSpeedByRate(int rate);
    void focusContinuousMove(FocuserDirection direction);
    void focusMoveBy(long steps);
    long focusGetStepperPosition();
    void focusSetStepperPosition(long steps);
    void disableFocusMotor();
    void enableFocusMotor();
    void focusStop();
#endif

#if (USE_HALL_SENSOR_RA_AUTOHOME == 1) || (USE_HALL_SENSOR_DEC_AUTOHOME == 1)
    bool findHomeByHallSensor(StepperAxis axis, int initialDirection, int searchDistance);
    void processHomingProgress();
#endif
    String getAutoHomingStates() const;

    void setHomingOffset(StepperAxis axis, long offset);
    long getHomingOffset(StepperAxis axis);

    // Move the given stepper motor by the given amount of steps.
    void moveStepperBy(StepperAxis which, long steps);

    // Move the given stepper motor to the given step position.
    void moveStepperTo(StepperAxis which, long position);

    // Set the number of steps to use for backlash correction
    void setBacklashCorrection(int steps);

    // Get the number of steps to use for backlash correction
    int getBacklashCorrection();

    // Read the saved configuration from persistent storage
    void readConfiguration();

    // Clear all saved configuration data from persistent storage
    void clearConfiguration();

    // Get Mount configuration data
    String getMountHardwareInfo();

    // Get info about the configured steppers and drivers
    String getStepperInfo();

    // Returns a flag indicating whether the mount is fully booted.
    bool isBootComplete();

    // Let the mount know that the system has finished booting
    void bootComplete();

    DayTime getUtcTime();
    DayTime getLocalTime();
    LocalDate getLocalDate();

    int getLocalUtcOffset() const;

    void setLocalStartDate(int year, int month, int day);
    void setLocalStartTime(DayTime localTime);
    void setLocalUtcOffset(int offset);

    DayTime calculateLst();
    DayTime calculateHa();

    // Returns NOT_SLEWING, SLEWING_DEC, SLEWING_RA, or SLEWING_BOTH. SLEWING_TRACKING is an overlaid bit.
    byte slewStatus() const;
    byte mountStatus() const;

    // Returns the remaining tracking time available and stops tracking if it reaches zero.
    float checkRALimit();

    // Calculate the stepper positions for the current target coordinates
    void calculateRAandDECSteppers(long &targetRASteps, long &targetDECSteps, long pSolutions[6] = nullptr) const;

#if UART_CONNECTION_TEST_TX == 1
    #if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    void testRA_UART_TX();
    #endif
    #if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    void testDEC_UART_TX();
    #endif
#endif
  private:
#if UART_CONNECTION_TEST_TX == 1
    #if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART || DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    void testUART_vactual(TMC2209Stepper *driver, int speed, int duration);
    #endif
#endif

    // Reads values from EEPROM that configure the mount (if previously stored)
    void readPersistentData();

    void displayStepperPosition();
    void moveSteppersTo(float targetRA, float targetDEC, StepperAxis direction);

    void autoCalcHa();

  private:
    LcdMenu *_lcdMenu;
#if (INFO_DISPLAY_TYPE != INFO_DISPLAY_TYPE_NONE)
    InfoDisplayRender *infoDisplay;
#endif
    float _stepsPerRADegree;   // u-steps/degree when slewing (see RA_STEPS_PER_DEGREE)
    float _stepsPerDECDegree;  // u-steps/degree when slewing (see DEC_STEPS_PER_DEGREE)
    uint32_t _maxRASpeed;
    uint32_t _maxDECSpeed;
    int _maxAZSpeed;
    int _maxALTSpeed;
    int _maxFocusSpeed;
    uint32_t _maxRAAcceleration;
    uint32_t _maxDECAcceleration;
    int _maxAZAcceleration;
    int _maxALTAcceleration;
    int _maxFocusAcceleration;
    int _backlashCorrectionSteps;
    int _moveRate;
    long _decLowerLimit;  // Movement limit in slewing steps
    long _decUpperLimit;  // Movement limit in slewing steps

#if USE_GYRO_LEVEL == 1
    float _pitchCalibrationAngle;
    float _rollCalibrationAngle;
#endif

    DayTime _LST;
    DayTime _zeroPosRA;

    DayTime _targetRA;
    long _currentRAStepperPosition;

    Declination _targetDEC;
    // The DEC offset from home position
    float _zeroPosDEC;
    long _lastTRKCheck;

    float _totalDECMove;
    float _totalRAMove;
    Latitude _latitude;
    Longitude _longitude;
    long _commandReceived;

    // Stepper control for RA, DEC and TRK.
#ifdef NEW_STEPPER_LIB
    StepperRaSlew *_stepperRA;
    StepperRaTrk *_stepperTRK;
    StepperDecSlew *_stepperDEC;
    StepperDecTrk *_stepperGUIDE;
#else
    AccelStepper *_stepperRA;
    AccelStepper *_stepperDEC;
    AccelStepper *_stepperTRK;
    AccelStepper *_stepperGUIDE;
#endif
#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    TMC2209Stepper *_driverRA;
#endif
#if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    TMC2209Stepper *_driverDEC;
#endif

#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE) || (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    bool _azAltWasRunning;
    #if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
        #ifdef NEW_STEPPER_LIB
    StepperAzSlew *_stepperAZ;
        #else
    AccelStepper *_stepperAZ;
        #endif
    const long _stepsPerAZDegree;  // u-steps/degree (from CTOR)
        #if AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    TMC2209Stepper *_driverAZ;
        #endif
    #endif
    #if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
        #ifdef NEW_STEPPER_LIB
    StepperAltSlew *_stepperALT;
        #else
    AccelStepper *_stepperALT;
        #endif
    const long _stepsPerALTDegree;  // u-steps/degree (from CTOR)
        #if ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    TMC2209Stepper *_driverALT;
        #endif
    #endif
#endif

#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    bool _focuserWasRunning  = false;
    FocuserMode _focuserMode = FOCUS_IDLE;
    float _maxFocusRateSpeed;
    #if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
        #ifdef NEW_STEPPER_LIB
    StepperFocusSlew *_stepperFocus;
        #else
    AccelStepper *_stepperFocus;
        #endif
    int _focusRate;
        #if FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    TMC2209Stepper *_driverFocus;
        #endif
    #endif
#endif

#if USE_HALL_SENSOR_RA_AUTOHOME == 1
    HallSensorHoming *_raHoming;
#endif
#if USE_HALL_SENSOR_DEC_AUTOHOME == 1
    HallSensorHoming *_decHoming;
#endif

#if USE_RA_END_SWITCH == 1
    EndSwitch *_raEndSwitch;
#endif
#if USE_DEC_END_SWITCH == 1
    EndSwitch *_decEndSwitch;
#endif

    unsigned long _guideRaEndTime;
    unsigned long _guideDecEndTime;
    unsigned long _lastMountPrint = 0;
    float _trackingSpeed;             // RA u-steps/sec when in tracking mode
    float _trackingSpeedCalibration;  // Dimensionless, very close to 1.0
    unsigned long _lastDisplayUpdate;
    unsigned long _trackerStoppedAt;
    bool _compensateForTrackerOff;
    volatile int _mountStatus;

    char scratchBuffer[24];
    bool _stepperWasRunning;
    bool _correctForBacklash;
    bool _slewingToHome;
    bool _slewingToPark;
    bool _bootComplete;

    int _localUtcOffset;
    LocalDate _localStartDate;
    DayTime _localStartTime;
    long _localStartTimeSetMillis;
};

#endif
