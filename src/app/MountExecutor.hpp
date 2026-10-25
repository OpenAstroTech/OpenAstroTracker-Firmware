#pragma once

// Arduino first: `String` is `arduino::String` on RP2040, and the value-type overlays below
// forward-declare it. Including them before Arduino.h makes that declaration a conflict there.
#include "../inc/Globals.hpp"

#include "../Declination.hpp"
#include "../Latitude.hpp"
#include "../Longitude.hpp"
#include "../Types.hpp"
#include "AxisBank.hpp"
#include "BoardPresence.hpp"
#include "MountReadout.hpp"
#include "MountSettings.hpp"
#include "../core/AzAltController.hpp"
#include "../core/EventBus.hpp"
#include "../core/FocusController.hpp"
#include "../core/Microsteps.hpp"
#include "../core/MountConfig.hpp"
#include "../core/MountCoordinator.hpp"
#include "../core/AxisLimits.hpp"
#include "../core/MountRequests.hpp"
#include "../core/MountRuntimeSettings.hpp"
#include "../core/MountState.hpp"
#include "../core/ParkingController.hpp"
#include "../core/SlewCompletion.hpp"
#include "../core/SiteTime.hpp"
#include "../core/SlewController.hpp"
#include "../core/SlewStart.hpp"
#include "../core/GuidingController.hpp"
#include "../core/TrackingController.hpp"
#include "../hal/IGpioPin.hpp"
#include "../ports/IClock.hpp"
#include "../ports/ILogger.hpp"
#include "../ports/IMotorDriver.hpp"
#include "../ports/IMountOptionalAxes.hpp"
#include "../ports/IMountMotion.hpp"
#include "../ports/IMountEvents.hpp"
#include "../ports/IMountReadModel.hpp"
#include "../ports/IInfoDisplay.hpp"
#include "../ports/IMountSettings.hpp"
#include "../ports/IScheduler.hpp"
#include "../ports/IStepperAxis.hpp"

#if (INFO_DISPLAY_TYPE != INFO_DISPLAY_TYPE_NONE)
#endif

class LcdMenu;
class TMC2209Stepper;

// Focuser support
// The focuser's direction and mode are domain vocabulary, so they live in `core/` with
// the controller that acts on them. The aliases keep the public menu and Meade names.
//
// The direction bits, coordinate-format selectors and status masks this header used to define
// now live where their owners are: the bits in `Types.hpp` (they are the ports' vocabulary),
// the status masks in `core/MountState.hpp` (they alias `core::StatusBit`).
using core::FOCUS_BACKWARD;
using core::FOCUS_FORWARD;
using FocuserDirection = core::FocuserDirection;

//////////////////////////////////////////////////////////////////
//
// The mount's hardware half: the axes, the drivers, the sensors and the settings they run on,
// plus the port implementations the UI and the command layer still call.
//
// It lives in `app/` because it owns objects only a composition root may build, and because what
// is left of the old `Mount` facade is the executor — it turns domain requests into axis
// operations and answers the read model. The decision layer is `core::MountCoordinator`; this
// class never decides when to move, only how.
//
//////////////////////////////////////////////////////////////////
namespace app
{

class HallSensorHoming;
class EndSwitch;

/// Everything `MountExecutor` is handed by the composition root (`app/BoardWiring.*`).
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

class MountExecutor final : public ports::IMountOptionalAxes
{
  public:
    /// Every controller the mount drives, in one object. The composition root owns it and builds
    /// it before the mount, so it can also hand it to anything that only needs the controllers.
    using Coordinator = core::MountCoordinator;

    /// The infrastructure ports are injected by the composition root. The axes are built by
    /// `app/MotorWiring.cpp` and handed over by `attachSteppers()`; the drivers come in through
    /// `configure*driver` and live as long as the mount does.
    ///
    /// The coordinator is owned by the composition root and outlives the mount, so the root can
    /// hand the same object to the display, the panel and — as the port implementations move — to
    /// the UI and the command layer.
    MountExecutor(const MountDeps &deps, const core::MountConfig &config, Coordinator &coordinator);

    void initializeVariables();

    /// The status panel's view of the mount. The panel is a pure reader, so it takes the read
    /// model rather than the executor that moves things.
    MountReadout &readout()
    {
        return _readout;
    }

    /// The hardware this board has. Exposed so the step pump can hold a reference to it: the axes
    /// are built in `setup()` while the pump is a static, so a copy would capture the empty bank.
    AxisBank &axes()
    {
        return _axes;
    }

    /// The session's site and time state. Exposed so the site/time port can be built over it:
    /// the mount still owns the storage, the adapter owns the write protocol around it.
    core::SiteTime &siteTime()
    {
        return _siteTime;
    }

    void configureHemisphere(bool isNorthern, bool force = false);

    /// Takes the axes the composition root built, plus the limits they run at, and wires the
    /// controller half into them. The engines themselves — and with them every platform `#ifdef` —
    /// live in `app/MotorWiring.cpp`; nothing here constructs one.
    void attachSteppers(const AxisBank::Steppers &steppers, const core::AxisLimits &limits);

    /// Takes the drivers `app/DriverWiring.cpp` built and programmed.
    void attachDrivers(const AxisBank::Drivers &drivers);

    /// Takes the end switches and hall sensors `app/SensorWiring.cpp` built.
    void attachSensors(const AxisBank::Sensors &sensors);

#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART || DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART                                              \
    || AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART || ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART                                           \
    || FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    bool connectToDriver(const String &driverKind, uint16_t *rmsCurrent = nullptr);

#endif
#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    // Configure the RA Driver (TMC2209 UART only)
    #if SW_SERIAL_UART == 0
    #elif SW_SERIAL_UART == 1
    #endif
#endif
#if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    // Configure the DEC Driver (TMC2209 UART only)
    #if SW_SERIAL_UART == 0
    #elif SW_SERIAL_UART == 1
    #endif
#endif

// Configure the AZ driver.
#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    #if AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
        #if SW_SERIAL_UART == 0
        #elif SW_SERIAL_UART == 1
        #endif
    #endif
#endif

// Configure the ALT driver.
#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    #if ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
        #if SW_SERIAL_UART == 0
        #elif SW_SERIAL_UART == 1
        #endif
    #endif
#endif

// Configure the Focus driver.
#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    #if FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
        #if SW_SERIAL_UART == 0
        #elif SW_SERIAL_UART == 1
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

    // Get the number of hours we've been tracking
    float getTrackedHours() const;

    // The hour angle derived from the current sidereal time and the pole star's RA.
    const DayTime HA() const;

    // Get a reference to the target RA value.
    core::DayTime &targetRA();

    // Get a reference to the target DEC value.
    core::Declination &targetDEC();

    // Get current RA value.
    core::DayTime currentRA() const;

    // Get current DEC value.
    core::Declination currentDEC() const;

    // Set the current RA and DEC position to be the given coordinates
    void syncPosition(const core::DayTime &ra, const core::Declination &dec);

    void calculateStepperPositions(float raCoord, float decCoord, long &raPos, long &decPos);

    // Calculates movement parameters and program steppers to move
    // there. Must call loop() frequently to actually move.
    void startSlewingToTarget();

    // Sends the mount to the home position
    void startSlewingToHome();

    // Move AZ and ALT motors to their zero position
    void moveAZALTToHome() override;
    void getAZALTPositions(long &azPos, long &altPos) override;
    void setAZALTHome() override;

    // Various status query functions
    bool isSlewingRAorDEC() const;
    bool isSlewingIdle() const;
    bool isSlewingTRK() const;
    bool isParking() const;
    bool isGuiding() const;
    bool isFindingHome() const;
    bool isRunningAZ() const;
    bool isRunningALT() const;

    bool isRunningFocus() const override;
    float getFocusSpeed() const override;

    // Starts manual slewing in one of eight directions or tracking
    void startSlewing(int direction);

    // Stop manual slewing in one of two directions or tracking. NS is the same. EW is the same
    void stopSlewing(int direction);
    void stopSlewing(StepperAxis axis);

    // Block until the motors specified (NORTH, EAST, TRACKING, etc.) are stopped
    void waitUntilStopped(byte direction);

    // Block until all motors are stopped
    void waitUntilAllStopped();

    // Same as Arduino delay() but keeps the tracker going.
    void delay(int ms);

    // Gets the position in one of eight directions or tracking
    long getCurrentStepperPosition(int direction);

    // Set the tracking stepper position
    void setTrackingStepperPos(long stepPos);

    // Process any stepper movement.
    void loop();

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
    const char *getStatusString();

    void setStatus(int state);
    void setStatusFlag(int flag);
    void clearStatusFlag(int flag);

    /// Registers an observer of mount events: the command layer (deferred replies) and the
    /// status panel. Events are drained once per `loop()`, so an observer is never called
    /// from inside the code that published. Registration is per sink, so removing one leaves
    /// the other in place.
    // Get the current speed of the stepper. NORTH, WEST, TRACKING
    float getSpeed(int direction);

    // See if a slew to target is in progress and return the percentage along the path
    bool getStepperProgress(int &raPercentage, int &decPercentage);

    /// How far each axis was asked to move by the slew in flight. Only the progress display reads
    /// these, to decide which of the two axes it should show a bar for; ``getStepperProgress``
    /// already folds them into percentages.
    float plannedRaSteps() const;
    float plannedDecSteps() const;

    // Called by Meade processor every time a command is received.
    void commandReceived();
    // --- ports::IMountReadModel (UI-facing reads) ------------------------------------------

    core::DayTime hourAngle();
    long getCurrentStepperPosition(StepperAxis axis);
    bool isAxisRunning(StepperAxis axis);
    const char *raText(uint8_t type, uint8_t active = 0);
    const char *decText(uint8_t type, uint8_t active = 0);

    long getNumCommandsReceived()
    {
        return _readout.getNumCommandsReceived();
    }

#if SUPPORT_DRIFT_ALIGNMENT == 1
    // Starts the drift alignment phase machine; `loop()` ticks it.
    void startDriftAlignment(int durationSecs);
    void updateDriftAlignment(unsigned long now);
#endif

    // Toggle the state where we run the motors at a constant speed
    void setManualSlewMode(bool state);

    // Set the speed of the given motor
    void setSpeed(StepperAxis which, float speedDegsPerSec);

    /// Creates the end switches this board has. A board without them calls a function
    /// that creates nothing, and every reader below null-checks instead of branching on a
    /// compile-time flag.

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
    bool findHomeByHallSensor(StepperAxis axis, int initialDirection, int searchDistance);
#endif
    /// Advances whichever hall-sensor homing run is active; a no-op on a board without
    /// one. Called once per tick, so the mount loop needs no `#if` for it.
    void processHomingProgress();

    /// Polls the RA/DEC end switches, if this board has them. No-op otherwise.
    void pollEndSwitches();
    const char *getAutoHomingStates() const;
    String homingStateText(bool present, const HallSensorHoming *sensor) const;

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
    const char *getMountHardwareInfo();

    // Get info about the configured steppers and drivers
    const char *getStepperInfo();

    // Returns a flag indicating whether the mount is fully booted.
    bool isBootComplete();

    // Let the mount know that the system has finished booting
    void bootComplete();

    core::DayTime calculateLst();
    core::DayTime calculateHa();

    // Returns NOT_SLEWING, SLEWING_DEC, SLEWING_RA, or SLEWING_BOTH. SLEWING_TRACKING is an overlaid bit.
    byte slewStatus() const;

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

    /// The sequence both slew entry points run once their target is known. `plan` says which of
    /// the pre-steps apply; see `core/SlewStart.hpp`.
    void beginSlew(const core::SlewStartPlan &plan, core::SlewIntent intent, long raTarget, long decTarget);

    /// Programs both axes for full-speed slewing. Takes no label: a runtime string would be a
    /// RAM-resident literal in debug builds, where the log format is the PROGMEM-aware path.
    void setSlewSpeeds();

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
    Coordinator &_coordinator;

#if (INFO_DISPLAY_TYPE != INFO_DISPLAY_TYPE_NONE)
#endif
    /// The live, EEPROM-backed motion settings: u-steps/degree, backlash, slew rate, DEC limits.
    core::MountRuntimeSettings _settings;

    /// Latitude, longitude, local sidereal time and the session clock, in one object.
    core::SiteTime _siteTime;

    DayTime _targetRA;
    long _currentRAStepperPosition;

    Declination _targetDEC;
    // The DEC offset from home position
    float _zeroPosDEC;
    long _lastTRKCheck;

    long _commandReceived;

    /// The axes, drivers, sensors and limits this board has. The only object that holds them, so
    /// every cluster below reaches the hardware through one owner instead of through loose members
    /// nobody owns. Held by value: same bytes as the pointers it replaces.
    AxisBank _axes;

    /// The read model's formatting half. Declared after every member it references, because it is
    /// constructed from references to them.
    MountReadout _readout;

    /// The write half of the configuration: every setting that reaches the EEPROM.
    MountSettings _persisted;

    unsigned long _lastMountPrint = 0;

    bool _bootComplete;
};

}  // namespace app
