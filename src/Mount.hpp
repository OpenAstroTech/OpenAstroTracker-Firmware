#ifndef _MOUNT_HPP_
#define _MOUNT_HPP_

#include "Declination.hpp"
#include "Latitude.hpp"
#include "Longitude.hpp"
#include "Types.hpp"
#include "core/AzAltController.hpp"
#include "core/EventBus.hpp"
#include "core/FocusController.hpp"
#include "core/Microsteps.hpp"
#include "core/MountConfig.hpp"
#include "core/MountCoordinator.hpp"
#include "core/MountRequests.hpp"
#include "core/MountState.hpp"
#include "core/ParkingController.hpp"
#include "core/SlewCompletion.hpp"
#include "core/SlewController.hpp"
#include "core/GuidingController.hpp"
#include "core/TrackingController.hpp"
#include "hal/IGpioPin.hpp"
#include "ports/IClock.hpp"
#include "ports/ILogger.hpp"
#include "ports/IMotorDriver.hpp"
#include "ports/IMountOptionalAxes.hpp"
#include "ports/IMountMotion.hpp"
#include "ports/IMountEvents.hpp"
#include "ports/IMountReadModel.hpp"
#include "ports/IMountSiteTime.hpp"
#include "ports/IInfoDisplay.hpp"
#include "ports/IMountSettings.hpp"
#include "ports/IScheduler.hpp"
#include "ports/IStepperAxis.hpp"

#if (INFO_DISPLAY_TYPE != INFO_DISPLAY_TYPE_NONE)
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

// Mount status bits. `core::MountState` owns the field; these names are the masks the
// firmware has always used, defined once in `core/MountState.hpp`.
#define STATUS_PARKED            ((uint16_t) core::StatusBit::Parked)
#define STATUS_SLEWING           ((uint16_t) core::StatusBit::Slewing)
#define STATUS_SLEWING_TO_TARGET ((uint16_t) core::StatusBit::SlewingToTarget)
#define STATUS_SLEWING_FREE      ((uint16_t) core::StatusBit::SlewingFree)
#define STATUS_SLEWING_MANUAL    ((uint16_t) core::StatusBit::SlewingManual)
#define STATUS_TRACKING          ((uint16_t) core::StatusBit::Tracking)
#define STATUS_PARKING           ((uint16_t) core::StatusBit::Parking)
#define STATUS_PARKING_POS       ((uint16_t) core::StatusBit::ParkingPos)
#define STATUS_GUIDE_PULSE       ((uint16_t) core::StatusBit::GuidePulse)
#define STATUS_GUIDE_PULSE_DIR   ((uint16_t) core::StatusBit::GuidePulseDir)
#define STATUS_GUIDE_PULSE_RA    ((uint16_t) core::StatusBit::GuidePulseRa)
#define STATUS_GUIDE_PULSE_DEC   ((uint16_t) core::StatusBit::GuidePulseDec)
#define STATUS_GUIDE_PULSE_MASK  ((uint16_t) core::StatusBit::GuidePulseMask)
#define STATUS_FINDING_HOME      ((uint16_t) core::StatusBit::FindingHome)

// Focuser support
// The focuser's direction and mode are domain vocabulary, so they live in `core/` with
// the controller that acts on them. The aliases keep the public menu and Meade names.
using core::FOCUS_BACKWARD;
using core::FOCUS_FORWARD;
using FocuserDirection = core::FocuserDirection;

//////////////////////////////////////////////////////////////////
//
// Class that represent the OpenAstroTracker mount, with all its parameters, motors, etc.
//
//////////////////////////////////////////////////////////////////
/// Everything `Mount` is handed by the composition root (`src/b_setup.hpp` today).
///
/// Entries exist only for the features compiled into this build, so a board without
/// an AZ/ALT/Focus axis, or without the UART connection test, spends no RAM on
/// pointers that nothing reads.
struct MountDeps {
    ports::IClock *clock;
    ports::IScheduler *scheduler;
    ports::ILogger *logger;
    ports::IMountSettings *settings;
#if (RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART) && (UART_CONNECTION_TEST_TXRX == 1)
    hal::IGpioPin *raEnablePin;
#endif
#if (DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART) && (UART_CONNECTION_TEST_TXRX == 1)
    hal::IGpioPin *decEnablePin;
#endif
    // Optional-axis driver enable pins. Null on a board without the axis.
    hal::IGpioPin *azEnablePin    = nullptr;
    hal::IGpioPin *altEnablePin   = nullptr;
    hal::IGpioPin *focusEnablePin = nullptr;
};

class Mount final : public ports::IMountOptionalAxes,
                    public ports::IMountReadModel,
                    public ports::IMountSiteTime,
                    public ports::IMountEvents,
                    public ports::IInfoDisplaySource
{
  public:
    /// The infrastructure ports are injected by the composition root
    /// (`src/b_setup.hpp` today). The axes and drivers are created in the
    /// `configure*` methods and owned by the mount for the lifetime of the program,
    /// exactly as the concrete steppers used to be.
    Mount(LcdMenu *lcdMenu, const MountDeps &deps, const core::MountConfig &config);

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
    float getSpeedCalibration() override;

    // Set the current RA tracking speed factor
    void setSpeedCalibration(float val, bool saveToStorage) override;

#if USE_GYRO_LEVEL == 1
    // Get the current pitch angle calibraton
    float getPitchCalibrationAngle() override;

    // Set the current pitch angle calibration
    void setPitchCalibrationAngle(float angle) override;

    // Get the current roll angle calibration
    float getRollCalibrationAngle() override;

    // Set the current pitch angle calibration
    void setRollCalibrationAngle(float angle) override;
#endif

    // Returns the number of slew microsteps the given motor turns to move one degree
    float getStepsPerDegree(StepperAxis which) override;

    // Function to set the number of slew microsteps the given motor turns to move one
    // degree for each axis. This function stores the value in persistent storage
    void setStepsPerDegree(StepperAxis which, float steps) override;

    // Sets the slew rate of the mount. rate is between 1 (slowest) and 4 (fastest)
    void setSlewRate(int rate) override;
    int getSlewRate();

    // Get the number of hours we've been tracking
    float getTrackedHours() const;

    // Set the HA time (HA is derived from LST, the setter calculates and sets LST)
    void setHA(const core::DayTime &haTime) override;
    const DayTime HA() const;

    // Set the LST time (HA is derived from LST)
    void setLST(const core::DayTime &haTime) override;

    void setLatitude(const core::Latitude &lat) override;
    void setLongitude(const core::Longitude &lon) override;
    core::Latitude latitude() const override;
    core::Longitude longitude() const override;

    // Get a reference to the target RA value.
    core::DayTime &targetRA() override;

    // Get a reference to the target DEC value.
    core::Declination &targetDEC() override;

    // Get current RA value.
    core::DayTime currentRA() const override;

    // Get current DEC value.
    core::Declination currentDEC() const override;

    // Set the current RA and DEC position to be the given coordinates
    void syncPosition(const core::DayTime &ra, const core::Declination &dec) override;

    void calculateStepperPositions(float raCoord, float decCoord, long &raPos, long &decPos) override;

    // Calculates movement parameters and program steppers to move
    // there. Must call loop() frequently to actually move.
    void startSlewingToTarget() override;

    // Sends the mount to the home position
    void startSlewingToHome() override;

    // Move AZ and ALT motors to their zero position
    void moveAZALTToHome() override;
    void getAZALTPositions(long &azPos, long &altPos) override;
    void setAZALTHome() override;

    // Various status query functions
    bool isSlewingRAorDEC() const override;
    bool isSlewingIdle() const;
    bool isSlewingTRK() const override;
    bool isParking() const;
    bool isGuiding() const override;
    bool isFindingHome() const;
    bool isAxisRunning(StepperAxis axis);
    bool isRunningAZ() const;
    bool isRunningALT() const;

    bool isRunningFocus() const override;
    float getFocusSpeed() const;

    // Starts manual slewing in one of eight directions or tracking
    void startSlewing(int direction) override;

    // Stop manual slewing in one of two directions or tracking. NS is the same. EW is the same
    void stopSlewing(int direction) override;
    void stopSlewing(StepperAxis axis) override;

    // Block until the motors specified (NORTH, EAST, TRACKING, etc.) are stopped
    void waitUntilStopped(byte direction);

    // Block until all motors are stopped
    void waitUntilAllStopped();

    // Same as Arduino delay() but keeps the tracker going.
    void delay(int ms);

    // Gets the position in one of eight directions or tracking
    long getCurrentStepperPosition(int direction);
    long getCurrentStepperPosition(StepperAxis axis);

    // Set the tracking stepper position
    void setTrackingStepperPos(long stepPos) override;

    // Process any stepper movement.
    void loop();

// Low-level process any stepper movement on interrupt callback.
#if defined(ESP32) || !defined(NEW_STEPPER_LIB)
    void interruptLoop();
#endif

    // Set the current stepper positions to be home.
    void setHome(bool clearZeroPos) override;

    // Set the DEC limit position to the given angle in degrees (saved as DEC steps).
    // If upper is true, sets the upper limit, else the lower limit.
    // If limitAngle is 0, limit is set to current position.
    void setDecLimitPosition(bool upper, float limitAngle = 0) override;

    // Clear the DEC limit position. If upper is true, clears upper limit, else the lower limit.
    void clearDecLimitPosition(bool upper) override;

    // Get the DEC limit positions
    void getDecLimitPositions(float &lowerLimit, float &upperLimit) override;

    // Asynchronously parks the mount. Moves to the home position and stops all motors.
    void park() override;

    // Runs the RA motor at twice the speed (or stops it), or the DEC motor at tracking speed for the given duration in ms.
    void guidePulse(byte direction, int duration) override;

    // Stops given guide operations in progress.
    void stopGuiding(bool ra = true, bool dec = true);

    // Return a string of DEC in the given format. For LCDSTRING, active determines where the cursor is
    String DECString(byte type, byte active = 0);

    // Return a string of DEC in the given format. For LCDSTRING, active determines where the cursor is
    String RAString(byte type, byte active = 0);

    // Returns string (singfle word) representing the mounts status.
    String getStatusStateString();

    // Returns a comma-delimited string with all the mounts' information
    const char *getStatusString() override;

    void setStatus(int state);
    void setStatusFlag(int flag);
    void clearStatusFlag(int flag);

    /// Registers an observer of mount events: the command layer (deferred replies) and the
    /// status panel. Events are drained once per `loop()`, so an observer is never called
    /// from inside the code that published. Registration is per sink, so removing one leaves
    /// the other in place.
    void setEventSink(core::IEventSink *sink);
    void clearEventSink(core::IEventSink *sink);

    // `ports::IMountEvents`: the command layer subscribes to completions so a deferred reply can
    // be sent when the work behind it finishes.
    void subscribe(core::IEventSink *sink) override;
    void unsubscribe(core::IEventSink *sink) override;

    // Get the current speed of the stepper. NORTH, WEST, TRACKING
    float getSpeed(int direction) override;

    // See if a slew to target is in progress and return the percentage along the path
    bool getStepperProgress(int &raPercentage, int &decPercentage);

    /// How far each axis was asked to move by the slew in flight. Only the progress display reads
    /// these, to decide which of the two axes it should show a bar for; ``getStepperProgress``
    /// already folds them into percentages.
    float plannedRaSteps() const;
    float plannedDecSteps() const;

    // Called by Meade processor every time a command is received.
    void commandReceived() override;
    // --- ports::IInfoDisplaySource ---------------------------------------------------------

    /// Which axes are moving this frame, for the panel's refresh policy.
    ports::InfoDisplayActivity activity() override;

    /// Fills one frame's worth of panel state. Everything the renderer shows comes from here,
    /// so the renderer never reads the mount.
    void fillInfoDisplayState(ports::InfoDisplayState &state) override;

    long getNumCommandsReceived() override
    {
        return _commandReceived;
    }

#if SUPPORT_DRIFT_ALIGNMENT == 1
    // Starts the drift alignment phase machine; `loop()` ticks it.
    void startDriftAlignment(int durationSecs) override;
    void updateDriftAlignment(unsigned long now);
#endif

    // Toggle the state where we run the motors at a constant speed
    void setManualSlewMode(bool state) override;

    // Set the speed of the given motor
    void setSpeed(StepperAxis which, float speedDegsPerSec) override;

    /// Creates the end switches this board has. A board without them calls a function
    /// that creates nothing, and every reader below null-checks instead of branching on a
    /// compile-time flag.
    void setupEndSwitches();

    // Support for moving the mount in azimuth and altitude (requires extra hardware).
    // Each entry point is a no-op when the axis is not present in this build.
    void moveBy(int direction, float arcMinutes) override;
    void disableAzAltMotors();
    void enableAzMotor();
    void enableAltMotor();

    // Support for focus motor (requires extra hardware). Each entry point is a no-op
    // when no focuser is present in this build.
    void focusSetSpeedByRate(int rate) override;
    void focusContinuousMove(FocuserDirection direction) override;
    void focusMoveBy(long steps) override;
    long focusGetStepperPosition() override;
    void focusSetStepperPosition(long steps) override;
    void disableFocusMotor();
    void enableFocusMotor();
    void focusStop() override;

#if (USE_HALL_SENSOR_RA_AUTOHOME == 1) || (USE_HALL_SENSOR_DEC_AUTOHOME == 1)
    bool findHomeByHallSensor(StepperAxis axis, int initialDirection, int searchDistance) override;
#endif
    /// Advances whichever hall-sensor homing run is active; a no-op on a board without
    /// one. Called once per tick, so the mount loop needs no `#if` for it.
    void processHomingProgress();

    /// Polls the RA/DEC end switches, if this board has them. No-op otherwise.
    void pollEndSwitches();
    const char *getAutoHomingStates() const override;
    String homingStateText(bool present, const HallSensorHoming *sensor) const;

    void setHomingOffset(StepperAxis axis, long offset) override;
    long getHomingOffset(StepperAxis axis) override;

    // Move the given stepper motor by the given amount of steps.
    void moveStepperBy(StepperAxis which, long steps) override;

    // Move the given stepper motor to the given step position.
    void moveStepperTo(StepperAxis which, long position);

    // Set the number of steps to use for backlash correction
    void setBacklashCorrection(int steps) override;

    // Get the number of steps to use for backlash correction
    int getBacklashCorrection() override;

    // Read the saved configuration from persistent storage
    void readConfiguration();

    // Clear all saved configuration data from persistent storage
    void clearConfiguration() override;

    // Get Mount configuration data
    const char *getMountHardwareInfo() override;

    // Get info about the configured steppers and drivers
    const char *getStepperInfo() override;

    // Returns a flag indicating whether the mount is fully booted.
    bool isBootComplete();

    // Let the mount know that the system has finished booting
    void bootComplete();

    DayTime getUtcTime();
    core::DayTime getLocalTime() override;
    LocalDate getLocalDate() override;

    int getLocalUtcOffset() const override;

    void setLocalStartDate(int year, int month, int day) override;
    void setLocalStartTime(const core::DayTime &localTime) override;
    void setLocalUtcOffset(int offset) override;

    core::DayTime calculateLst() override;
    core::DayTime calculateHa() override;

    // Returns NOT_SLEWING, SLEWING_DEC, SLEWING_RA, or SLEWING_BOTH. SLEWING_TRACKING is an overlaid bit.
    byte slewStatus() const;
    byte mountStatus() const;

    // Returns the remaining tracking time available and stops tracking if it reaches zero.
    float checkRALimit() override;

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
    void testUART_vactual(ports::IMotorDriver *driver, int speed, int duration);
    #endif
#endif

    // Reads values from EEPROM that configure the mount (if previously stored)
    void readPersistentData();

    void publishEvent(core::MountEventType type, bool succeeded = false);

    /// Rewrites `STATUS_PARKING`/`STATUS_PARKING_POS` from `_parking`'s phase.
    void applyParkingStatus();

    /// Snapshot of the state the admission rules read. See `core/MountRequests.hpp`.
    core::MountActivityFacts activityFacts() const;
    bool admit(core::MountRequest request) const;

    /// Switches the RA/DEC driver's microstep register.
    ///
    /// On a board whose driver takes its microsteps from pin strapping these are no-ops,
    /// which is the whole point: the call sites are the same on every board, so code that
    /// has to run "switch RA to tracking microsteps" does not need to know what kind of
    /// driver is fitted. That is what lets the slew sequence become runtime logic.
    void setRaMicrosteps(int microsteps);
    void setDecMicrosteps(int microsteps);

    void moveSteppersTo(float targetRA, float targetDEC, StepperAxis direction);

    void autoCalcHa();

  private:
    // Injected by the composition root; declared first so the constructor's
    // initialiser order is the declaration order (and therefore warning-free).
    MountDeps _deps;

    // The motion shape the board was built with: microstep rates and the steps-per-degree
    // defaults. Filled once by the composition root (`app::buildMountConfig()`).
    core::MountConfig _config;

    // Every controller the mount owns, in one object: `core::MountCoordinator` states
    // which of them exist and, from here on, in what order a tick touches them. The axes are
    // held through `ports::IStepperAxis` on every platform — the concrete engine types live in
    // `hal/` and are built by the composition root's factories, never named here.
    using Coordinator = core::MountCoordinator<ports::IStepperAxis>;
    Coordinator _coordinator;

    LcdMenu *_lcdMenu;
#if (INFO_DISPLAY_TYPE != INFO_DISPLAY_TYPE_NONE)
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

    /// Backing store for the `const char *` readouts, which the protocol sends and the info
    /// display shows. A member rather than a local so the pointer survives the return; the value
    /// is valid until the next call to the method that filled it.
    mutable String _readoutScratch;

    /// Backing store for the status line in `InfoDisplayState`, for the same reason.
    String _infoStatusScratch;

    // Stepper control for RA, DEC and TRK.
    //
    // Held through the axis port on every platform, including AVR: the interrupt-stepper engines
    // are constructed behind `hal::avr::InterruptStepperMotor` by the composition root's factory,
    // so the concrete engine types never reach this class. The AVR step generator is the engine's
    // own ISR, not `Mount::interruptLoop()` (compiled out for `NEW_STEPPER_LIB`), so no virtual
    // call reaches the stepping path.
    ports::IStepperAxis *_stepperRA;
    ports::IStepperAxis *_stepperTRK;
    ports::IStepperAxis *_stepperDEC;
    ports::IStepperAxis *_stepperGUIDE;
    // Driver ports. Always set: a board whose driver has no registers behind it holds the
    // null object, so the call sites below need no guard.
    ports::IMotorDriver *_driverRA;
    ports::IMotorDriver *_driverDEC;

    // Optional axes: present at run time or null. Every caller guards with kHasXStepper.
    ports::IStepperAxis *_stepperAZ = nullptr;
    float _stepsPerAZDegree;  // u-steps/degree (from CTOR)
    ports::IMotorDriver *_driverAZ;
    ports::IStepperAxis *_stepperALT = nullptr;
    float _stepsPerALTDegree;  // u-steps/degree (from CTOR)
    ports::IMotorDriver *_driverALT;

    // The axis is attached by configureFocusStepper(), and stays null when this build has
    // no focuser.
    ports::IStepperAxis *_stepperFocus = nullptr;
    ports::IMotorDriver *_driverFocus;

    // Optional hardware, present at run time or null.
    //
    // `kHas*` is the compile-time half: a board that cannot have the part still pays for
    // these pointers (8 B of SRAM on AVR) but not for the driver code behind them, because
    // the constant lets the compiler drop the branch entirely. Readers test
    // `kHasX && ptr != nullptr`, which reads as a presence check at the call site and
    // still folds to nothing on a board without the part.
    static constexpr bool kHasAzStepper    = (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE);
    static constexpr bool kHasAltStepper   = (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE);
    static constexpr bool kHasFocusStepper = (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE);
    static constexpr bool kHasRaEndSwitch  = (USE_RA_END_SWITCH == 1);
    static constexpr bool kHasDecEndSwitch = (USE_DEC_END_SWITCH == 1);
    static constexpr bool kHasRaHoming     = (USE_HALL_SENSOR_RA_AUTOHOME == 1);
    static constexpr bool kHasDecHoming    = (USE_HALL_SENSOR_DEC_AUTOHOME == 1);

    HallSensorHoming *_raHoming  = nullptr;
    HallSensorHoming *_decHoming = nullptr;
    EndSwitch *_raEndSwitch      = nullptr;
    EndSwitch *_decEndSwitch     = nullptr;

    unsigned long _lastMountPrint = 0;

    char scratchBuffer[24];
    bool _bootComplete;

    int _localUtcOffset;
    LocalDate _localStartDate;
    DayTime _localStartTime;
    long _localStartTimeSetMillis;
};

#endif
