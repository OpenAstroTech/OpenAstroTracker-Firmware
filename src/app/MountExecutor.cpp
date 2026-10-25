#include "../Configuration.hpp"
#include "../Utility.hpp"
#include "../EPROMStore.hpp"
#include "../LcdMenu.hpp"
#include "../HallSensorHoming.hpp"
#include "../EndSwitches.hpp"
#include "MountExecutor.hpp"
#include "../Sidereal.hpp"
#include "../core/CalendarMath.hpp"
#include "../core/CoordinateFormatter.hpp"
#include "../core/CoordinateText.hpp"
#include "../core/ManualSlew.hpp"
#include "../core/RelativeMove.hpp"

// The menu screens pass the historical `B0xxx` constants through `ports::IMountReadModel`; core
// owns the format vocabulary, so the two must not drift.
static_assert(static_cast<uint8_t>(core::CoordinateFormat::LcdMenu) == LCDMENU_STRING, "format value drift: LcdMenu");
static_assert(static_cast<uint8_t>(core::CoordinateFormat::Meade) == MEADE_STRING, "format value drift: Meade");
static_assert(static_cast<uint8_t>(core::CoordinateFormat::Print) == PRINT_STRING, "format value drift: Print");
static_assert(static_cast<uint8_t>(core::CoordinateFormat::Lcd) == LCD_STRING, "format value drift: Lcd");
static_assert(static_cast<uint8_t>(core::CoordinateFormat::Compact) == COMPACT_STRING, "format value drift: Compact");
static_assert(core::kCoordinateFormatMask == FORMAT_STRING_MASK, "format mask drift");
static_assert(core::kTargetFlag == TARGET_STRING, "target flag drift");

static_assert(static_cast<uint8_t>(core::SlewDirection::North) == NORTH, "direction value drift: North");
static_assert(static_cast<uint8_t>(core::SlewDirection::East) == EAST, "direction value drift: East");
static_assert(static_cast<uint8_t>(core::SlewDirection::South) == SOUTH, "direction value drift: South");
static_assert(static_cast<uint8_t>(core::SlewDirection::West) == WEST, "direction value drift: West");
static_assert(static_cast<uint8_t>(core::SlewDirection::Tracking) == TRACKING, "direction value drift: Tracking");
#include "../core/CoordinateMath.hpp"
#include "../core/ActivityState.hpp"
#include "../core/meade/MountStatusReport.hpp"
#include "../core/CoordinateRead.hpp"
#include "../core/DecLimitClamp.hpp"
#include "../core/FirmwareVersion.hpp"
#include "../core/PersistentConfig.hpp"
#include "../core/SiderealClock.hpp"
#include "../core/RaTrackingLimit.hpp"
#include "../core/SlewProgressReport.hpp"
#include "../core/SlewStart.hpp"
#include "../core/SyncAdjustment.hpp"
#include "../libs/MappedDict/MappedDict.hpp"

#include "UiPorts.hpp"
#include "../adapters/GpioEndSwitch.hpp"
#include "../adapters/HallHomingSensor.hpp"
#include "../adapters/NullMotorDriver.hpp"
#include "../adapters/Tmc2209Driver.hpp"

#include "../hal/CriticalSection.hpp"
#include "../hal/arduino/ArduinoGpioPin.hpp"

// slewStatus()
#define SLEWING_DEC      B00000010
#define SLEWING_RA       B00000001
#define SLEWING_BOTH     B00000011
#define SLEWING_TRACKING B00001000
#define NOT_SLEWING      B00000000
#define SLEW_MASK_ANY    B1111

#define UART_CONNECTION_TEST_RETRIES 5

const float siderealDegreesInHour = 14.95904348958;

namespace app
{

/////////////////////////////////
//
// CTOR
//
/////////////////////////////////
MountExecutor::MountExecutor(const MountDeps &deps, const core::MountConfig &config, Coordinator &coordinator)
    : _deps(deps), _config(config), _coordinator(coordinator),
      _readout(_axes, coordinator, _settings, config, _siteTime, _targetRA, _targetDEC, _zeroPosDEC)
{
    _commandReceived = 0;

    initializeVariables();
}

void MountExecutor::initializeVariables()
{
    // We are now defaulting to northern hemisphere at 45deg. Switching is now supported
    // at runtime when the Latitude is received via Meade command.
    inNorthernHemisphere = NORTHERN_HEMISPHERE == 1;

    // u-steps per degree when slewing, plus the motion tuning defaults
    _settings.reset(_config.defaultRaStepsPerDegree, _config.defaultDecStepsPerDegree);

    _coordinator.state().reset(0);
    _coordinator.slew().noteRunning(false);

    // Created by the configure* methods below; null until then. The drivers go back to the null
    // driver, which `reset()` puts in place.
    _axes.reset();
    _coordinator.tracking().detach();
    _coordinator.focus().detach();

    _siteTime.setLatitude(core::Latitude(inNorthernHemisphere ? 45.0f : -45.0f));
    _siteTime.setLongitude(core::Longitude(100.0));
    _zeroPosDEC = 0.0f;

    _coordinator.slew().clearTotalMove();
    _coordinator.slew().clearBacklashCorrection();
    _coordinator.slew().end();
    _coordinator.parking().clear();
    applyParkingStatus();
#if USE_GYRO_LEVEL == 1
    _pitchCalibrationAngle = 0;
    _rollCalibrationAngle  = 0;
#endif

    _siteTime.setLocalUtcOffset(0);
    _siteTime.setLocalStartDate({2021, 1, 1});
    _siteTime.setLocalStartTime(DayTime(0), static_cast<unsigned long>(-1));
    _lastTRKCheck = 0;
}

/////////////////////////////////
//
// clearConfiguration
//
/////////////////////////////////
void MountExecutor::clearConfiguration()
{
    _deps.settings->clearConfiguration();
    initializeVariables();
    readConfiguration();
}

/////////////////////////////////
//
// readConfiguration
//
/////////////////////////////////
void MountExecutor::readConfiguration()
{
    OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: Reading configuration data from EEPROM");
    readPersistentData();
    OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: Done reading configuration data from EEPROM");
}

/////////////////////////////////
//
// readPersistentData
//
/////////////////////////////////
void MountExecutor::readPersistentData()
{
    // The upgrade check runs before anything is read: it may zero the AZ/ALT home positions,
    // and those are read below. The store always returns valid data, even on a blank EEPROM.
    const int16_t version                = core::firmwareVersionCode(VERSION);
    const core::MigrationOutcome outcome = core::applyVersionMigration(*_deps.settings, version);

    if (!outcome.versionChanged)
    {
        OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: Same firmware version as last boot %d.", version);
    }
    else
    {
        OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: New Flash detected! Flashed to %d.", version);
        if (outcome.homePositionsReset)
        {
            OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: First time post 1.13.6, setting AZ and ALT home to 0");
        }
    }

    const core::PersistedSettings stored = core::loadPersistedSettings(*_deps.settings);

    _settings.setStepsPerDegree(RA_STEPS, stored.raStepsPerDegree);
    OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: RA steps/deg is %f", _settings.stepsPerDegree(RA_STEPS));

    _settings.setStepsPerDegree(DEC_STEPS, stored.decStepsPerDegree);
    OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: DEC steps/deg is %f", _settings.stepsPerDegree(DEC_STEPS));

    if (kHasAzStepper)
    {
        _axes.setAzStepsPerDegree(stored.azStepsPerDegree);
        OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: AZ steps/deg is %f", _axes.azStepsPerDegree());
    }

    if (kHasAltStepper)
    {
        _axes.setAltStepsPerDegree(stored.altStepsPerDegree);
        OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: ALT steps/deg is %f", _axes.altStepsPerDegree());
    }

    OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: Speed factor is %f", stored.speedFactor);
    setSpeedCalibration(stored.speedFactor, false);

    _settings.setBacklashCorrectionSteps(stored.backlashCorrectionSteps);
    OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: Backlash correction is %d", _settings.backlashCorrectionSteps());

    _siteTime.setLatitude(stored.latitude);
    OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: Latitude is %s", Latitude(_siteTime.latitude()).ToString());

    _siteTime.setLongitude(stored.longitude);
    OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: Longitude is %s", Longitude(_siteTime.longitude()).ToString());

    _siteTime.setLocalUtcOffset(stored.utcOffset);
    OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: UTC offset is %d", _siteTime.localUtcOffset());

#if USE_GYRO_LEVEL == 1
    _pitchCalibrationAngle = stored.pitchCalibrationAngle;
    OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: Pitch Offset is %f", _pitchCalibrationAngle);

    _rollCalibrationAngle = stored.rollCalibrationAngle;
    OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: Roll Offset is %f", _rollCalibrationAngle);
#endif

    const core::DecLimits limits = core::decLimitsFromStored(
        stored.decLowerLimitDegrees, stored.decUpperLimitDegrees, _settings.stepsPerDegree(DEC_STEPS), DEC_LIMIT_DOWN, DEC_LIMIT_UP);
    _settings.setDecLowerLimitSteps(limits.lowerSteps);
    _settings.setDecUpperLimitSteps(limits.upperSteps);
    OAT_LOG(_deps.logger,
            DEBUG_INFO,
            "[MOUNT]: EEPROM: DEC limits read as %ld -> %ld",
            _settings.decLowerLimitSteps(),
            _settings.decUpperLimitSteps());

    if (kHasAltStepper)
    {
        _axes.alt()->setCurrentPosition(stored.altPosition);
    }

    if (kHasAzStepper)
    {
        _axes.az()->setCurrentPosition(stored.azPosition);
    }

    configureHemisphere(_siteTime.latitude().getTotalHours() > 0);
}

/////////////////////////////////
//
// configureHemisphere
//
/////////////////////////////////
void MountExecutor::configureHemisphere(bool inNorthern, bool force)
{
    if ((inNorthernHemisphere != inNorthern) || force)
    {
        bool wasTracking = isSlewingTRK();
        OAT_LOG(_deps.logger, DEBUG_ANY, "[SYSTEM]: Hemisphere changed (or forced update) to %s.", inNorthern ? "northern" : "southern");
        OAT_LOG(_deps.logger, DEBUG_ANY, "[SYSTEM]: Stopping all steppers.");
        stopSlewing(ALL_DIRECTIONS | TRACKING);
        waitUntilStopped(ALL_DIRECTIONS);
        inNorthernHemisphere = inNorthern;
        bool invertDir       = inNorthernHemisphere ? (RA_INVERT_DIR == 1) : (RA_INVERT_DIR != 1);
        OAT_LOG(_deps.logger, DEBUG_ANY, "[SYSTEM]: Configured RA steppers, DIR Invert is %d", invertDir);
        _axes.ra()->setPinsInverted(invertDir, false, false);
        _axes.tracking()->setPinsInverted(invertDir, false, false);

        OAT_LOG(_deps.logger, DEBUG_ANY, "[SYSTEM]: Reset RA and TRK positions to 0");
        _axes.tracking()->setCurrentPosition(0);
        _axes.ra()->setCurrentPosition(0);
        if (wasTracking)
        {
            OAT_LOG(_deps.logger, DEBUG_ANY, "[SYSTEM]: Restarting TRK since it was on.");
            startSlewing(TRACKING);
        }
    }
    else
    {
        OAT_LOG(_deps.logger,
                DEBUG_ANY,
                "[SYSTEM]: Already in %s hemisphere, no action taken.",
                inNorthernHemisphere ? "northern" : "southern");
    }
}

/////////////////////////////////
//
// attachSteppers
//
/////////////////////////////////
// The engines are built by the composition root (`app/MotorWiring.cpp`) and handed over here
// fully configured: speed, acceleration and direction inversion are hardware facts, and the
// coordinator wiring below is the mount's own business.
void MountExecutor::attachSteppers(const AxisBank::Steppers &steppers, const core::AxisLimits &limits)
{
    _axes.attachSteppers(steppers, limits);

    if (_axes.tracking() != nullptr)
    {
        _coordinator.tracking().attach(_axes.tracking());
    }

    if (_axes.focus() != nullptr)
    {
        _coordinator.focus().attach(_axes.focus());
        _coordinator.focus().initialize(limits.focusSpeed);
    }
}

/////////////////////////////////
//
// attachDrivers
//
/////////////////////////////////
// The drivers arrive fully programmed; all that is left is to take ownership and keep the null
// driver in place for the axes this board does not have.
void MountExecutor::attachDrivers(const AxisBank::Drivers &drivers)
{
    _axes.attachDrivers(drivers);
}

/////////////////////////////////
//
// attachSensors
//
/////////////////////////////////
// The switches and sensors arrive from `app/SensorWiring.cpp`, already bound to their pins.
void MountExecutor::attachSensors(const AxisBank::Sensors &sensors)
{
    _axes.attachSensors(sensors);
}

#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART || DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART                                              \
    || AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART || ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART                                           \
    || FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    #if (UART_CONNECTION_TEST_TXRX == 1) || (TEST_VERIFY_MODE == 1)
bool MountExecutor::connectToDriver(const String &driverKind, uint16_t *rmsCurrent)
{
    MappedDict<String, ports::IMotorDriver *>::DictEntry_t lookupTable[] = {
        #if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
        {"RA", _axes.raDriver()},
        #endif
        #if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
        {"DEC", _axes.decDriver()},
        #endif
        #if ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
        {"ALT", _axes.altDriver()},
        #endif
        #if AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
        {"AZ", _axes.azDriver()},
        #endif
        #if FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
        {"FOC", _axes.focusDriver()},
        #endif
    };
    auto driverLookup = MappedDict<String, ports::IMotorDriver *>(lookupTable, ARRAY_SIZE(lookupTable));

    ports::IMotorDriver *driver = nullptr;
    driverLookup.tryGet(driverKind, &driver);

    if (driver != nullptr)
    {
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: Testing UART Connection to %s driver...", driverKind.c_str());
        for (int i = 0; i < UART_CONNECTION_TEST_RETRIES; i++)
        {
            if (driver->testConnection())
            {
                OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: UART connection to %s driver successful.", driverKind.c_str());
                if (rmsCurrent != nullptr)
                {
                    *rmsCurrent = driver->rms_current();
                }
                return true;
            }
            else
            {
                delay(500);
            }
        }
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: UART connection to %s driver failed.", driverKind.c_str());
    }
    if (rmsCurrent != nullptr)
    {
        *rmsCurrent = 0;
    }
    return false;
}
    #endif
#endif

float MountExecutor::getSpeedCalibration()
{
    return _coordinator.tracking().calibration();
}

/////////////////////////////////
//
// setSpeedCalibration
//
/////////////////////////////////
void MountExecutor::setSpeedCalibration(float val, bool saveToStorage)
{
    OAT_LOG(_deps.logger, DEBUG_MOUNT, "[MOUNT]: Updating speed calibration from %f to %f", _coordinator.tracking().calibration(), val);
    _coordinator.tracking().setCalibration(val);

    OAT_LOG(_deps.logger, DEBUG_MOUNT, "[MOUNT]: Current tracking speed is %f steps/sec", _coordinator.tracking().speed());

    // The tracking rate is derived from the slewing geometry; see core/TrackingController.hpp.
    _coordinator.tracking().setStepsPerDegree(_settings.stepsPerDegree(RA_STEPS));
    OAT_LOG(_deps.logger, DEBUG_MOUNT, "[MOUNT]: RA steps per degree is %f steps/deg", _settings.stepsPerDegree(RA_STEPS));
    OAT_LOG(_deps.logger, DEBUG_MOUNT, "[MOUNT]: New tracking speed is %f steps/sec", _coordinator.tracking().speed());

    OAT_LOG(_deps.logger,
            DEBUG_MOUNT,
            "[MOUNT]: FactorToSpeed : %s, %s",
            String(val, 6).c_str(),
            String(_coordinator.tracking().speed(), 6).c_str());

    if (saveToStorage)
        _deps.settings->storeSpeedFactor(_coordinator.tracking().calibration());

    // If we are currently tracking, update the speed. No need to update microstepping mode
    if (isSlewingTRK())
    {
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[MOUNT]: SpeedCalibration TRK.setSpeed(%f)", _coordinator.tracking().speed());
        _coordinator.tracking().applySpeed();
    }
}

#if USE_GYRO_LEVEL == 1
/////////////////////////////////
//
// getPitchCalibrationAngle
//
// The pitch value at which the mount is level
/////////////////////////////////
float MountExecutor::getPitchCalibrationAngle()
{
    return _pitchCalibrationAngle;
}

/////////////////////////////////
//
// setPitchCalibrationAngle
//
/////////////////////////////////
void MountExecutor::setPitchCalibrationAngle(float angle)
{
    _pitchCalibrationAngle = angle;
    _deps.settings->storePitchCalibrationAngle(_pitchCalibrationAngle);
}

/////////////////////////////////
//
// getRollCalibration
//
// The roll value at which the mount is level
/////////////////////////////////
float MountExecutor::getRollCalibrationAngle()
{
    return _rollCalibrationAngle;
}

/////////////////////////////////
//
// setRollCalibration
//
/////////////////////////////////
void MountExecutor::setRollCalibrationAngle(float angle)
{
    _rollCalibrationAngle = angle;
    _deps.settings->storeRollCalibrationAngle(_rollCalibrationAngle);
}
#endif

float MountExecutor::getStepsPerDegree(StepperAxis which)
{
    if (which == RA_STEPS)
    {
        return _settings.stepsPerDegree(RA_STEPS);  // u-steps/degree
    }
    if (which == DEC_STEPS)
    {
        return _settings.stepsPerDegree(DEC_STEPS);  // u-steps/degree
    }
    if (which == AZIMUTH_STEPS)
    {
        return kHasAzStepper ? _axes.azStepsPerDegree() : 1;  // u-steps/degree
    }
    if (which == ALTITUDE_STEPS)
    {
        return kHasAltStepper ? _axes.altStepsPerDegree() : 1;  // u-steps/degree
    }

    return 0;
}

/////////////////////////////////
//
// setStepsPerDegree
//
/////////////////////////////////
// Function to set steps per degree for each axis. This function stores the value in persistent storage.
void MountExecutor::setStepsPerDegree(StepperAxis which, float steps)
{
    if (which == DEC_STEPS)
    {
        _settings.setStepsPerDegree(DEC_STEPS, steps);
        _deps.settings->storeDECStepsPerDegree(_settings.stepsPerDegree(DEC_STEPS));
    }
    else if (which == RA_STEPS)
    {
        _settings.setStepsPerDegree(RA_STEPS, steps);
        _deps.settings->storeRAStepsPerDegree(_settings.stepsPerDegree(RA_STEPS));
        setSpeedCalibration(_coordinator.tracking().calibration(), false);
    }
    else if (kHasAzStepper && which == AZIMUTH_STEPS)
    {
        _axes.setAzStepsPerDegree(steps);
        _deps.settings->storeAZStepsPerDegree(_axes.azStepsPerDegree());
    }
    else if (kHasAltStepper && which == ALTITUDE_STEPS)
    {
        _axes.setAltStepsPerDegree(steps);
        _deps.settings->storeALTStepsPerDegree(_axes.altStepsPerDegree());
    }
}

int MountExecutor::getBacklashCorrection()
{
    return _settings.backlashCorrectionSteps();
}

/////////////////////////////////
//
// setBacklashCorrection
//
/////////////////////////////////
// Function to set steps per degree for each axis. This function stores the value in persistent storage.
void MountExecutor::setBacklashCorrection(int steps)
{
    _settings.setBacklashCorrectionSteps(steps);
    _deps.settings->storeBacklashCorrectionSteps(_settings.backlashCorrectionSteps());
}

/////////////////////////////////
//
// setSlewRate
//
/////////////////////////////////
void MountExecutor::setSlewRate(int rate)
{
    _settings.setMoveRate(clamp(rate, 1, 4));
    float speedFactor[] = {0, 0.05, 0.2, 0.5, 1.0};
    OAT_LOG(_deps.logger, DEBUG_MOUNT, "[MOUNT]: setSlewRate, rate is %d -> %f", _settings.moveRate(), speedFactor[_settings.moveRate()]);
    _axes.dec()->setMaxSpeed(speedFactor[_settings.moveRate()] * _axes.limits().decSpeed);
    _axes.ra()->setMaxSpeed(speedFactor[_settings.moveRate()] * _axes.limits().raSpeed);
    OAT_LOG(
        _deps.logger, DEBUG_MOUNT, "[MOUNT]: setSlewRate, new speeds are RA: %f  DEC: %f", _axes.ra()->maxSpeed(), _axes.dec()->maxSpeed());
}

/////////////////////////////////
//
// setSlewRate
//
/////////////////////////////////
int MountExecutor::getSlewRate()
{
    return _settings.moveRate();
}

/////////////////////////////////
//
// HA
//
/////////////////////////////////
const DayTime MountExecutor::HA() const
{
    // LOG(DEBUG_MOUNT_VERBOSE, "[MOUNT]: Get HA.");
    // LOG(DEBUG_MOUNT_VERBOSE, "[MOUNT]: Polaris adjust: %s", DayTime(POLARIS_RA_HOUR, POLARIS_RA_MINUTE, POLARIS_RA_SECOND).ToString());
    DayTime ha = DayTime(_siteTime.lst());
    // LOG(DEBUG_MOUNT_VERBOSE, "[MOUNT]: LST: %s", DayTime(_siteTime.lst()).ToString());
    ha.subtractTime(DayTime(POLARIS_RA_HOUR, POLARIS_RA_MINUTE, POLARIS_RA_SECOND));
    // LOG(DEBUG_MOUNT, "[MOUNT]: GetHA: LST-Polaris is HA %s", ha.ToString());
    return ha;
}

/////////////////////////////////
//
// targetRA
//
/////////////////////////////////
// Get a reference to the target RA value.
core::DayTime &MountExecutor::targetRA()
{
    return _targetRA;
}

/////////////////////////////////
//
// targetDEC
//
/////////////////////////////////
// Get a reference to the target DEC value.
core::Declination &MountExecutor::targetDEC()
{
    return _targetDEC;
}

// Get current RA value.

// Get current DEC value.

/////////////////////////////////
//
// syncPosition
//
/////////////////////////////////
// Set the current RA and DEC position to be the given coordinate. We do this by setting the stepper motor coordinate
// to be at the calculated positions (that they would be if we were slewing there).
void MountExecutor::syncPosition(const core::DayTime &ra, const core::Declination &dec)
{
    long solutions[6];
    _targetDEC = Declination(dec);
    _targetRA  = DayTime(ra);
    OAT_LOG(_deps.logger,
            DEBUG_COORD_CALC,
            "[MOUNT]: syncPosition: Target Sync is RA: %f  and DEC: %f",
            _targetRA.getTotalHours(),
            _targetDEC.getTotalDegrees());
    OAT_LOG(_deps.logger,
            DEBUG_COORD_CALC,
            "[MOUNT]: syncPosition: Current Pos is RA: %f  and DEC: %f )",
            currentRA().getTotalHours(),
            currentDEC().getTotalDegrees());
    OAT_LOG(_deps.logger,
            DEBUG_COORD_CALC,
            "[MOUNT]: syncPosition: ZeroPos values RA: %f  and DEC: %f)",
            DayTime(_siteTime.raZeroPoint()).getTotalHours(),
            _zeroPosDEC);

    // The declination the stepper position maps to, before the correction: negative when the
    // mount sits on the far side of home, which is what flips the declination correction.
    const float degreePos
        = (_axes.dec()->currentPosition() / _settings.stepsPerDegree(DEC_STEPS)) + _zeroPosDEC;  // u-steps / u-steps/deg = deg
    OAT_LOG(_deps.logger, DEBUG_COORD_CALC, "[MOUNT]: syncPosition: DEC degreePos is: %f", degreePos);

    const core::SyncAdjustment adjustment = core::syncAdjustment(
        ra.getTotalHours(), currentRA().getTotalHours(), dec.getTotalDegrees(), currentDEC().getTotalDegrees(), degreePos);
    OAT_LOG(_deps.logger, DEBUG_COORD_CALC, "[MOUNT]: syncPosition: AdjustRA is %f", adjustment.raHours);
    _siteTime.adjustRaZeroPoint(adjustment.raHours);
    OAT_LOG(_deps.logger, DEBUG_COORD_CALC, "[MOUNT]: syncPosition: ZeroPosRA is now %f", DayTime(_siteTime.raZeroPoint()).getTotalHours());

    _zeroPosDEC += adjustment.decDegrees;
    OAT_LOG(_deps.logger, DEBUG_COORD_CALC, "[MOUNT]: syncPosition: _zeroPosDEC adjusted by: %f", adjustment.decDegrees);
    OAT_LOG(_deps.logger, DEBUG_COORD_CALC, "[MOUNT]: syncPosition: _zeroPosDEC: %f", _zeroPosDEC);

    long targetRAPosition, targetDECPosition;
    calculateRAandDECSteppers(targetRAPosition, targetDECPosition, solutions);

    OAT_LOG(_deps.logger, DEBUG_COORD_CALC, "[MOUNT]: syncPosition: Solution 1: RA %ld and DEC %ld", solutions[0], solutions[1]);
    OAT_LOG(_deps.logger, DEBUG_COORD_CALC, "[MOUNT]: syncPosition: Solution 2: RA %ld and DEC %ld", solutions[2], solutions[3]);
    OAT_LOG(_deps.logger, DEBUG_COORD_CALC, "[MOUNT]: syncPosition: Solution 3: RA %ld and DEC %ld", solutions[4], solutions[5]);
    OAT_LOG(_deps.logger, DEBUG_COORD_CALC, "[MOUNT]: syncPosition: Chosen    : RA %ld and DEC %ld", targetRAPosition, targetDECPosition);
}

/////////////////////////////////
//
// startSlewingToTarget
//
/////////////////////////////////
// Calculates movement parameters and program steppers to move
// there. Must call loop() frequently to actually move.
void MountExecutor::startSlewingToTarget()
{
    if (!admit(core::MountRequest::GoTo))
    {
        // A go-to used to reprogram both axes underneath a park sequence that was still
        // homing. PLAN.md lists this refusal as the one intentional behaviour change.
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: startSlewingToTarget: refused while parking");
        return;
    }

    stopGuiding();
    setSlewSpeeds();

    // Calculate new RA stepper target (and DEC). We are never in guiding mode here.
    _currentRAStepperPosition = _axes.ra()->currentPosition();
    long targetRAPosition, targetDECPosition;
    calculateRAandDECSteppers(targetRAPosition, targetDECPosition);

    const core::SlewStartPlan plan
        = core::planSlewStart(core::SlewStartKind::Target, {_axes.ra()->currentPosition(), targetRAPosition, isSlewingTRK()});
    beginSlew(plan, core::SlewIntent::Target, targetRAPosition, targetDECPosition);
}

/////////////////////////////////
//
// startSlewingToHome
//
/////////////////////////////////
// Calculates movement parameters and program steppers to move to home position.
// Takes any sync operations that have happened and tracking into account.
void MountExecutor::startSlewingToHome()
{
    stopGuiding();
    setSlewSpeeds();

    _currentRAStepperPosition = _axes.ra()->currentPosition();

    const long targetDECPosition = 0;
    const long targetRAPosition
        = core::homeRaTargetSteps(_axes.tracking()->currentPosition(), _config.raSlewMicrosteps, _config.raTrackingMicrosteps());
    OAT_LOG(_deps.logger,
            DEBUG_STEPPERS,
            "[STEPPERS]: startSlewingToHome: Adjusted with tracking distance: %ld, result: %ld",
            _axes.tracking()->currentPosition(),
            targetRAPosition);

    const core::SlewStartPlan plan
        = core::planSlewStart(core::SlewStartKind::Home, {_axes.ra()->currentPosition(), targetRAPosition, isSlewingTRK()});
    beginSlew(plan, core::SlewIntent::Home, targetRAPosition, targetDECPosition);
}
/////////////////////////////////
//
// beginSlew — the tail both slew entry points share
//
/////////////////////////////////
// The two entry points differ only in what they compute and in whether the tracker is stopped
// unconditionally (core::planSlewStart states that difference); from here on they are identical,
// so the sequence exists once.
void MountExecutor::beginSlew(const core::SlewStartPlan &plan, core::SlewIntent intent, long raTarget, long decTarget)
{
    if (plan.stopTracking)
    {
        // Only stop tracking if we're actually going to slew somewhere else, otherwise the
        // loop() code won't detect the end of the slewing operation...
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[MOUNT]: Stop tracking (NEMA steppers)");
        stopSlewing(TRACKING);
        _coordinator.tracking().noteStopped(_deps.clock->now());
    }

    if (plan.enterRaSlewMicrosteps)
    {
        // Set RA to slew microsteps for TMC2209 UART once the TRK stepper has stopped
        setRaMicrosteps(_config.raSlewMicrosteps);
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: beginSlew: TRK stopped at %ldms", _coordinator.tracking().stoppedAt());
    }

    _coordinator.slew().begin(intent);
    setStatusFlag(STATUS_SLEWING | STATUS_SLEWING_TO_TARGET);
    publishEvent(core::MountEventType::SlewStarted);
    // Normal state for DEC is guide microstepping, so switch to slew microstepping here.
    setDecMicrosteps(_config.decSlewMicrosteps);
    _coordinator.slew().noteRunning(true);
    moveSteppersTo(raTarget, decTarget, RA_AND_DEC_STEPS);  // u-steps (in slew mode)
    _coordinator.slew().setTotalMove(static_cast<float>(_axes.ra()->distanceToGo()), static_cast<float>(_axes.dec()->distanceToGo()));
    OAT_LOG(_deps.logger, DEBUG_MOUNT, "[MOUNT]: RA Dist: %ld,   DEC Dist: %ld", _axes.ra()->distanceToGo(), _axes.dec()->distanceToGo());
}

/////////////////////////////////
//
// setSlewSpeeds
//
/////////////////////////////////
// Both slew entry points run the axes at their configured maximum; the rate table is applied
// afterwards by setSlewRate().
void MountExecutor::setSlewSpeeds()
{
    OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: beginSlew: Set DEC to MaxSpeed(%ld)", _axes.limits().decSpeed);
    _axes.dec()->setMaxSpeed(_axes.limits().decSpeed);
    OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: beginSlew: Set RA  to MaxSpeed(%ld)", _axes.limits().raSpeed);
    _axes.ra()->setMaxSpeed(_axes.limits().raSpeed);
}

/////////////////////////////////
//
// stopGuiding
//
/////////////////////////////////
void MountExecutor::stopGuiding(bool ra, bool dec)
{
    if (!isGuiding())
        return;

    // Stop RA guide first, since it's just a speed change back to tracking speed
    if (ra && _coordinator.state().test(STATUS_GUIDE_PULSE_RA))
    {
        OAT_LOG(_deps.logger,
                DEBUG_STEPPERS | DEBUG_GUIDE,
                "[GUIDE]: stopGuide:    TRK stop guide at  : %ld",
                _axes.tracking()->currentPosition());
        _coordinator.tracking().applySpeed();
        OAT_LOG(
            _deps.logger, DEBUG_STEPPERS | DEBUG_GUIDE, "[GUIDE]: stopGuide:    TRK speed set to   : %f", _coordinator.tracking().speed());
        _coordinator.guiding().clearRa();
        clearStatusFlag(STATUS_GUIDE_PULSE_RA);
    }

    if (dec && _coordinator.state().test(STATUS_GUIDE_PULSE_DEC))
    {
        OAT_LOG(_deps.logger,
                DEBUG_STEPPERS | DEBUG_GUIDE,
                "[GUIDE]: stopGuide:    DEC stop guide at   : %ld",
                _axes.guide()->currentPosition());

        // Stop DEC guiding and wait for it to stop.
        _axes.guide()->stop();

#if !defined(ESP32BOARD)
        while (_axes.guide()->isRunning())
        {
            _axes.guide()->run();
            _axes.tracking()->runSpeed();
        }
#endif

        _coordinator.guiding().clearDec();
        clearStatusFlag(STATUS_GUIDE_PULSE_DEC);
    }

    //disable pulse state if no direction is active
    if (!_coordinator.state().test(STATUS_GUIDE_PULSE_DIR))
    {
        OAT_LOG(_deps.logger, DEBUG_STEPPERS | DEBUG_GUIDE, "[GUIDE]: Clear guiding state");
        clearStatusFlag(STATUS_GUIDE_PULSE_MASK);
        publishEvent(core::MountEventType::GuidingStopped);
    }
    else
    {
        OAT_LOG(_deps.logger, DEBUG_STEPPERS | DEBUG_GUIDE, "[GUIDE]: One axis still guiding");
    }
}

/////////////////////////////////
//
// guidePulse
//
/////////////////////////////////
void MountExecutor::guidePulse(byte direction, int duration)
{
#if (DEBUG_LEVEL != DEBUG_NONE)
    const char *directionName = "-NE-S---W";
#endif
    OAT_LOG(_deps.logger,
            DEBUG_STEPPERS | DEBUG_GUIDE,
            "[GUIDE]: guidePulse: > Guide Pulse %c for %dms requested",
            directionName[direction],
            duration);
    const bool wasGuiding = _coordinator.state().test(STATUS_GUIDE_PULSE);

    if (!admit(core::MountRequest::Guide))
    {
        OAT_LOG(_deps.logger, DEBUG_STEPPERS | DEBUG_GUIDE, "[GUIDE]: guidePulse: Not tracking (at limit?), ignoring guide pulse");
    }
    else
    {
        if ((direction == NORTH) || (direction == SOUTH))
        {
            OAT_LOG(_deps.logger,
                    DEBUG_STEPPERS | DEBUG_GUIDE,
                    "[GUIDE]: guidePulse:   DEC current steps   : %ld",
                    _axes.guide()->currentPosition());
            OAT_LOG(_deps.logger,
                    DEBUG_STEPPERS | DEBUG_GUIDE,
                    "[GUIDE]: guidePulse:   DEC steps/deg       : %f",
                    _settings.stepsPerDegree(DEC_STEPS));
            OAT_LOG(_deps.logger,
                    DEBUG_STEPPERS | DEBUG_GUIDE,
                    "[GUIDE]: guidePulse:   DEC Microstep ratio : %f",
                    (1.0 * _config.decGuidingMicrosteps() / _config.decSlewMicrosteps));
        }
        else
        {
            OAT_LOG(_deps.logger,
                    DEBUG_STEPPERS | DEBUG_GUIDE,
                    "[GUIDE]: guidePulse:   RA  current steps   : %ld",
                    _axes.tracking()->currentPosition());
            OAT_LOG(_deps.logger,
                    DEBUG_STEPPERS | DEBUG_GUIDE,
                    "[GUIDE]: guidePulse:   RA  steps/deg       : %f",
                    _settings.stepsPerDegree(RA_STEPS));
            OAT_LOG(_deps.logger,
                    DEBUG_STEPPERS | DEBUG_GUIDE,
                    "[GUIDE]: guidePulse:   RA  Microstep ratio : %f",
                    (1.0 * _config.raTrackingMicrosteps() / _config.raSlewMicrosteps));
        }

        // DEC stepper moves at sidereal rate in both directions
        // RA stepper moves at either 2.5x sidereal rate or 0.5x sidereal rate.
        // Also compensate for microstepping mode change between slew & guiding/tracking
        const core::GuideAxisGeometry raGeometry
            = {_settings.stepsPerDegree(RA_STEPS), _config.raTrackingMicrosteps(), _config.raSlewMicrosteps, RA_PULSE_MULTIPLIER};
        const core::GuideAxisGeometry decGeometry
            = {_settings.stepsPerDegree(DEC_STEPS), _config.decGuidingMicrosteps(), _config.decSlewMicrosteps, DEC_PULSE_MULTIPLIER};

        // TODO: Do we need to track how many steps the steppers took and add them to the GoHome calculation?
        // If so, we need to remember where we were when we started the guide pulse. Then at the end,
        // we can calculate the difference. Ignore DEC Guide for now.
        // TODO: Take guide pulses on DEC into account

        switch (direction)
        {
            case NORTH:
                {
                    // If a upper limit is set, check we're not there. If we are, refuse the guide pulse.
                    const core::DecPulse pulse = core::GuidingController::decPulse(true,
                                                                                   decGeometry,
                                                                                   siderealDegreesInHour,
                                                                                   _axes.dec()->currentPosition(),
                                                                                   _settings.decLowerLimitSteps(),
                                                                                   _settings.decUpperLimitSteps());
                    if (!pulse.allowed)
                    {
                        // TODO: Make sure Southern hemisphere does not need a sign/direction inversion.
                        OAT_LOG(_deps.logger,
                                DEBUG_STEPPERS | DEBUG_GUIDE,
                                "[GUIDE]: guidePulse:   DEC is at (%ld) which is at or above upper limit (%ld), ignoring guide pulse",
                                _axes.dec()->currentPosition(),
                                _settings.decUpperLimitSteps());
                    }
                    else
                    {
                        OAT_LOG(
                            _deps.logger, DEBUG_STEPPERS | DEBUG_GUIDE, "[GUIDE]: guidePulse:   DEC base speed      : %f", pulse.baseSpeed);
                        OAT_LOG(_deps.logger,
                                DEBUG_STEPPERS | DEBUG_GUIDE,
                                "[GUIDE]: guidePulse:   DEC guide speed     : %f",
                                pulse.targetSpeed);
                        _axes.guide()->setSpeed(pulse.targetSpeed);
                        setStatusFlag(STATUS_GUIDE_PULSE | STATUS_GUIDE_PULSE_DEC);
                        _coordinator.guiding().startDec(duration);
                    }
                }
                break;

            case SOUTH:
                {
                    const core::DecPulse pulse = core::GuidingController::decPulse(false,
                                                                                   decGeometry,
                                                                                   siderealDegreesInHour,
                                                                                   _axes.dec()->currentPosition(),
                                                                                   _settings.decLowerLimitSteps(),
                                                                                   _settings.decUpperLimitSteps());
                    if (!pulse.allowed)
                    {
                        OAT_LOG(_deps.logger,
                                DEBUG_STEPPERS | DEBUG_GUIDE,
                                "[GUIDE]: guidePulse:   DEC is at (%ld) which is at or below lower limit (%ld), ignoring guide pulse",
                                _axes.dec()->currentPosition(),
                                _settings.decLowerLimitSteps());
                        OAT_LOG(_deps.logger,
                                DEBUG_STEPPERS | DEBUG_GUIDE,
                                "[GUIDE]: guidePulse:   DEC is at lower limit, ignoring guide pulse");
                    }
                    else
                    {
                        OAT_LOG(
                            _deps.logger, DEBUG_STEPPERS | DEBUG_GUIDE, "[GUIDE]: guidePulse:   DEC base speed      : %f", pulse.baseSpeed);
                        OAT_LOG(_deps.logger,
                                DEBUG_STEPPERS | DEBUG_GUIDE,
                                "[GUIDE]: guidePulse:   DEC guide speed     : %f",
                                pulse.targetSpeed);
                        _axes.guide()->setSpeed(pulse.targetSpeed);
                        setStatusFlag(STATUS_GUIDE_PULSE | STATUS_GUIDE_PULSE_DEC);
                        _coordinator.guiding().startDec(duration);
                    }
                }
                break;

            case WEST:
                {
                    // We were in tracking mode before guiding, so no need to update microstepping mode on RA driver
                    const core::RaPulse pulse
                        = core::GuidingController::raPulse(true, raGeometry, siderealDegreesInHour, _coordinator.tracking().calibration());
                    OAT_LOG(_deps.logger, DEBUG_STEPPERS | DEBUG_GUIDE, "[GUIDE]: guidePulse:   RA  base speed      : %f", pulse.baseSpeed);
                    OAT_LOG(_deps.logger,
                            DEBUG_STEPPERS | DEBUG_GUIDE,
                            "[GUIDE]: guidePulse:   RA  speed factor    : %f",
                            _coordinator.tracking().calibration());
                    OAT_LOG(
                        _deps.logger, DEBUG_STEPPERS | DEBUG_GUIDE, "[GUIDE]: guidePulse:   RA  adjusted speed  : %f", pulse.adjustedSpeed);
                    OAT_LOG(_deps.logger,
                            DEBUG_STEPPERS | DEBUG_GUIDE,
                            "[GUIDE]: guidePulse:   RA  guide speed     : %f (%f x adjusted speed)",
                            pulse.targetSpeed,
                            RA_PULSE_MULTIPLIER);
                    _axes.tracking()->setSpeed(pulse.targetSpeed);  // Faster than siderael
                    setStatusFlag(STATUS_GUIDE_PULSE | STATUS_GUIDE_PULSE_RA);
                    _coordinator.guiding().startRa(duration);
                }
                break;

            case EAST:
                {
                    // We were in tracking mode before guiding, so no need to update microstepping mode on RA driver
                    const core::RaPulse pulse
                        = core::GuidingController::raPulse(false, raGeometry, siderealDegreesInHour, _coordinator.tracking().calibration());
                    OAT_LOG(_deps.logger, DEBUG_STEPPERS | DEBUG_GUIDE, "[GUIDE]: guidePulse:   RA  base speed      : %f", pulse.baseSpeed);
                    OAT_LOG(_deps.logger,
                            DEBUG_STEPPERS | DEBUG_GUIDE,
                            "[GUIDE]: guidePulse:   RA  speed factor    : %f",
                            _coordinator.tracking().calibration());
                    OAT_LOG(
                        _deps.logger, DEBUG_STEPPERS | DEBUG_GUIDE, "[GUIDE]: guidePulse:   RA  adjusted speed  : %f", pulse.adjustedSpeed);
                    OAT_LOG(_deps.logger,
                            DEBUG_STEPPERS | DEBUG_GUIDE,
                            "[GUIDE]: guidePulse:   RA  guide speed     : %f (%f x adjusted speed)",
                            (2.0 - RA_PULSE_MULTIPLIER * pulse.adjustedSpeed),
                            (2.0 - RA_PULSE_MULTIPLIER));
                    _axes.tracking()->setSpeed(pulse.targetSpeed);  // Slower than siderael
                    setStatusFlag(STATUS_GUIDE_PULSE | STATUS_GUIDE_PULSE_RA);
                    _coordinator.guiding().startRa(duration);
                }
                break;
        }
    }
    if (!wasGuiding && _coordinator.state().test(STATUS_GUIDE_PULSE))
    {
        publishEvent(core::MountEventType::GuidingStarted);
    }

    OAT_LOG(_deps.logger, DEBUG_STEPPERS | DEBUG_GUIDE, "[GUIDE]: guidePulse: < Guide Pulse");
}

/////////////////////////////////
//
// commandReceived()
//
// Keeps track of how many Meade commands have been processed.
/////////////////////////////////
void MountExecutor::commandReceived()
{
    _commandReceived++;
}

#if SUPPORT_DRIFT_ALIGNMENT == 1
/////////////////////////////////
//
// startDriftAlignment / updateDriftAlignment
//
// The drift-alignment sequence is a phase machine in core/DriftAlignmentController: it used to be
// three synchronous passes with three 1.5 s delays between them, which left the mount answering
// nothing for about 90 seconds. Here the sequence is started, and `loop()` ticks it — every wait is
// either a clock deadline or "the axis stopped", which the loop already knows.
/////////////////////////////////
void MountExecutor::startDriftAlignment(int durationSecs)
{
    _coordinator.drift().begin(durationSecs, _settings.stepsPerDegree(RA_STEPS));
}

void MountExecutor::updateDriftAlignment(unsigned long now)
{
    if (!_coordinator.drift().isActive())
    {
        return;
    }

    const bool raRunning  = _axes.ra()->isRunning();
    const bool decRunning = _axes.dec()->isRunning();

    switch (_coordinator.drift().update(!raRunning && !decRunning, now))
    {
        case core::DriftAction::None:
            break;

        case core::DriftAction::StopAxes:
            stopSlewing(ALL_DIRECTIONS | TRACKING);
            break;

        case core::DriftAction::MoveEastPass:
            _axes.ra()->setMaxSpeed(_coordinator.drift().passSpeed());
            _axes.ra()->move(_coordinator.drift().passSteps());
            break;

        case core::DriftAction::MoveEastGap:
            _axes.ra()->setMaxSpeed(core::DriftAlignmentController::kGapSpeedStepsSec);
            _axes.ra()->move(-core::DriftAlignmentController::kGearingGapSteps);
            break;

        case core::DriftAction::MoveWestPass:
            _axes.ra()->setMaxSpeed(_coordinator.drift().passSpeed());
            _axes.ra()->move(-_coordinator.drift().passSteps());
            break;

        case core::DriftAction::ResetGap:
            _axes.ra()->setMaxSpeed(core::DriftAlignmentController::kGapSpeedStepsSec);
            _axes.ra()->move(core::DriftAlignmentController::kGearingGapSteps);
            break;

        case core::DriftAction::ResumeTracking:
            // Put the stepper back into its configured slew parameters before tracking resumes.
            _axes.ra()->setMaxSpeed(_axes.limits().raSpeed);
            startSlewing(TRACKING);
            break;
    }
}
#endif

/////////////////////////////////
//
// setManualSlewMode
//
/////////////////////////////////
void MountExecutor::setManualSlewMode(bool state)
{
    if (state)
    {
        stopSlewing(ALL_DIRECTIONS);
        stopSlewing(TRACKING);
        waitUntilStopped(ALL_DIRECTIONS);
        setStatusFlag(STATUS_SLEWING | STATUS_SLEWING_MANUAL);
        setRaMicrosteps(_config.raSlewMicrosteps);
    }
    else
    {
        clearStatusFlag(STATUS_SLEWING_MANUAL);
        stopSlewing(ALL_DIRECTIONS);
        waitUntilStopped(ALL_DIRECTIONS);
        OAT_LOG(_deps.logger,
                DEBUG_STEPPERS,
                "[STEPPERS]: setManualSlewMode: Set RA  speed/accel:  %ld  / %ld",
                _axes.limits().raSpeed,
                _axes.limits().raAcceleration);
        OAT_LOG(_deps.logger,
                DEBUG_STEPPERS,
                "[STEPPERS]: setManualSlewMode: Set DEC speed/accel:  %ld  / %ld",
                _axes.limits().decSpeed,
                _axes.limits().decAcceleration);
        _axes.ra()->setAcceleration(_axes.limits().raAcceleration);
        _axes.ra()->setMaxSpeed(_axes.limits().raSpeed);
        _axes.dec()->setMaxSpeed(_axes.limits().decSpeed);
        _axes.dec()->setAcceleration(_axes.limits().decAcceleration);
        startSlewing(TRACKING);
    }
}

/////////////////////////////////
//
// setSpeed
//
/////////////////////////////////
void MountExecutor::setSpeed(StepperAxis which, float speedDegsPerSec)
{
    if (which == RA_STEPS)
    {
        float stepsPerSec = speedDegsPerSec * _settings.stepsPerDegree(RA_STEPS);  // deg/sec * u-steps/deg = u-steps/sec
        OAT_LOG(_deps.logger,
                DEBUG_STEPPERS,
                "[STEPPERS]: setSpeed: Set RA speed %f degs/s, which is %f steps/s",
                speedDegsPerSec,
                stepsPerSec);
        // TODO: Are we already in slew mode?
        _axes.ra()->setSpeed(stepsPerSec);
    }
    else if (which == DEC_STEPS)
    {
        float stepsPerSec = speedDegsPerSec * _settings.stepsPerDegree(DEC_STEPS);  // deg/sec * u-steps/deg = u-steps/sec
        OAT_LOG(_deps.logger,
                DEBUG_STEPPERS,
                "[STEPPERS]: setSpeed: Set DEC speed %f degs/s, which is %f steps/s",
                speedDegsPerSec,
                stepsPerSec);
        // TODO: Are we already in slew mode?
        _axes.dec()->setSpeed(stepsPerSec);
    }
    else if (kHasAzStepper && which == AZIMUTH_STEPS)
    {
        float stepsPerSec = speedDegsPerSec * _axes.azStepsPerDegree();  // deg/sec * u-steps/deg = u-steps/sec
        OAT_LOG(_deps.logger,
                DEBUG_STEPPERS,
                "[STEPPERS]: setSpeed: Set AZ speed %f degs/s, which is %f steps/s",
                speedDegsPerSec,
                stepsPerSec);
        _axes.az()->setSpeed(stepsPerSec);
    }
    else if (kHasAltStepper && which == ALTITUDE_STEPS)
    {
        float stepsPerSec = speedDegsPerSec * _axes.altStepsPerDegree();  // deg/sec * u-steps/deg = u-steps/sec
        OAT_LOG(_deps.logger,
                DEBUG_STEPPERS,
                "[STEPPERS]: setSpeed: Set ALT speed %f degs/s, which is %f steps/s",
                speedDegsPerSec,
                stepsPerSec);
        _axes.alt()->setSpeed(stepsPerSec);
    }
    else if (kHasFocusStepper && which == FOCUS_STEPS)
    {
        OAT_LOG(_deps.logger, DEBUG_MOUNT | DEBUG_FOCUS, "[FOCUS]: setSpeed() Focuser setSpeed %f", speedDegsPerSec);
        if (speedDegsPerSec != 0)
        {
            OAT_LOG(_deps.logger,
                    DEBUG_STEPPERS | DEBUG_FOCUS,
                    "[FOCUS]: Mount:setSpeed(): Enabling motor, setting speed to %f. Continuous",
                    speedDegsPerSec);
            enableFocusMotor();
            _coordinator.focus().runAtSpeed(speedDegsPerSec);
            publishEvent(core::MountEventType::FocusMoving);
        }
        else
        {
            OAT_LOG(_deps.logger, DEBUG_STEPPERS | DEBUG_FOCUS, "[FOCUS]: setSpeed(): Stopping motor.");
            _axes.focus()->stop();
        }
    }
}

void MountExecutor::getAZALTPositions(long &azPos, long &altPos)
{
    azPos  = kHasAzStepper ? _axes.az()->currentPosition() : 0;
    altPos = kHasAltStepper ? _axes.alt()->currentPosition() : 0;
}

void MountExecutor::moveAZALTToHome()
{
    if (kHasAzStepper)
    {
        enableAzMotor();
        _axes.az()->moveTo(0);
    }
    if (kHasAltStepper)
    {
        enableAltMotor();
        _axes.alt()->moveTo(0);
    }
}

void MountExecutor::setAZALTHome()
{
    if (kHasAzStepper)
    {
        _axes.az()->setCurrentPosition(0);
        _deps.settings->storeAZPosition(0);
    }
    if (kHasAltStepper)
    {
        _axes.alt()->setCurrentPosition(0);
        _deps.settings->storeALTPosition(0);
    }
}

/////////////////////////////////
//
// park
//
// Targets the mount to move to the home position and
// turns off all motors once it gets there.
/////////////////////////////////
void MountExecutor::park()
{
    stopGuiding();
    stopSlewing(ALL_DIRECTIONS | TRACKING);
    waitUntilStopped(ALL_DIRECTIONS);
    _coordinator.parking().beginHoming();
    startSlewingToHome();
    applyParkingStatus();
    publishEvent(core::MountEventType::ParkingStarted);
}

/////////////////////////////////
//
// isRunningAZ
//
/////////////////////////////////
bool MountExecutor::isRunningAZ() const
{
    return kHasAzStepper && _axes.az()->isRunning();
}

/////////////////////////////////
//
// isRunningALT
//
/////////////////////////////////
bool MountExecutor::isRunningALT() const
{
    return kHasAltStepper && _axes.alt()->isRunning();
}

/////////////////////////////////
//
// moveBy
//
/////////////////////////////////
void MountExecutor::moveBy(int direction, float arcMinutes)
{
    if (kHasAzStepper && direction == AZIMUTH_STEPS)
    {
        enableAzMotor();
        long stepsToMove = arcMinutes * AZIMUTH_STEPS_PER_ARC_MINUTE;
        _axes.az()->move(stepsToMove);
    }
    if (kHasAltStepper && direction == ALTITUDE_STEPS)
    {
        enableAltMotor();
        long stepsToMove = arcMinutes * ALTITUDE_STEPS_PER_ARC_MINUTE;
        _axes.alt()->move(stepsToMove);
    }
}

/////////////////////////////////
//
// disableAzAltMotors
//
/////////////////////////////////
void MountExecutor::disableAzAltMotors()
{
    if (kHasAzStepper)
    {
        _axes.az()->stop();
    }
    if (kHasAltStepper)
    {
        _axes.alt()->stop();
    }

    if (kHasAzStepper)
    {
        while (_axes.az()->isRunning())
        {
            loop();
        }
    }

    if (kHasAltStepper)
    {
        while (_axes.alt()->isRunning())
        {
            loop();
        }
    }

    if (AZ_ALWAYS_ON == 0 && kHasAzStepper)
    {
        _deps.azEnablePin->write(hal::PinLevel::High);  // Logic HIGH to disable driver
    }

    if (ALT_ALWAYS_ON == 0 && kHasAltStepper)
    {
        _deps.altEnablePin->write(hal::PinLevel::High);  // Logic HIGH to disable driver
    }
}

/////////////////////////////////
//
// enableAzAltMotors
//
/////////////////////////////////
void MountExecutor::enableAzMotor()
{
    if (!kHasAzStepper)
    {
        return;
    }
    _deps.azEnablePin->write(hal::PinLevel::Low);  // Logic LOW to enable driver
    // Let loop() disable the driver again, even if the move turns out to be zero steps
    // (e.g. move to home while already at home), so it never stays powered.
    _coordinator.azAlt().update(true);
}

void MountExecutor::enableAltMotor()
{
    if (!kHasAltStepper)
    {
        return;
    }
    _deps.altEnablePin->write(hal::PinLevel::Low);  // Logic LOW to enable driver
    // See enableAzMotor()
    _coordinator.azAlt().update(true);
}

/////////////////////////////////
//
// isRunningFocus
//
/////////////////////////////////
bool MountExecutor::isRunningFocus() const
{
    return _coordinator.focus().isRunning();
}

float MountExecutor::getFocusSpeed() const
{
    return _coordinator.focus().speed();
}

/////////////////////////////////
//
// focusSetSpeedByRate
//
/////////////////////////////////
void MountExecutor::focusSetSpeedByRate(int rate)
{
    _coordinator.focus().setRate(rate, _axes.limits().focusSpeed);
    OAT_LOG(_deps.logger,
            DEBUG_FOCUS,
            "[FOCUS]: focusSetSpeedByRate: rate is %d, factor %f, maxspeed %f -> %f",
            _coordinator.focus().rate(),
            _coordinator.focus().rateFactor(),
            _axes.limits().focusSpeed,
            _coordinator.focus().maxRateSpeed());

    if (_coordinator.focus().isRunning())
    {
        OAT_LOG(_deps.logger, DEBUG_FOCUS, "[FOCUS]: focusSetSpeedByRate: stepper is already running so should adjust speed?");
        //_axes.focus()->setSpeed(_coordinator.focus().rateFactor() * _axes.limits().focusSpeed);
    }
}

/////////////////////////////////
//
// focusContinuousMove
//
/////////////////////////////////
void MountExecutor::focusContinuousMove(FocuserDirection direction)
{
    // maxSpeed is set to what the rate dictates
    OAT_LOG(_deps.logger,
            DEBUG_FOCUS,
            "[FOCUS]: focusContinuousMove: direction is %d, maxspeed %f",
            direction,
            _coordinator.focus().maxRateSpeed());
    setSpeed(FOCUS_STEPS, static_cast<int>(direction) * _coordinator.focus().maxRateSpeed());
}

/////////////////////////////////
//
// focusMoveBy
//
/////////////////////////////////
void MountExecutor::focusMoveBy(long steps)
{
    OAT_LOG(_deps.logger,
            DEBUG_FOCUS,
            "[FOCUS]: focusMoveBy: move by %ld steps to %ld. Target Mode.",
            steps,
            _coordinator.focus().position() + steps);
    enableFocusMotor();
    _coordinator.focus().moveBy(steps);
    publishEvent(core::MountEventType::FocusMoving);
}

/////////////////////////////////
//
// focusGetPosition
//
/////////////////////////////////
long MountExecutor::focusGetStepperPosition()
{
    return _coordinator.focus().position();
}

/////////////////////////////////
//
// focusSetPosition
//
/////////////////////////////////
void MountExecutor::focusSetStepperPosition(long steps)
{
    _coordinator.focus().setPosition(steps);
}

/////////////////////////////////
//
// disableFocusMotor
//
/////////////////////////////////
void MountExecutor::disableFocusMotor()
{
    OAT_LOG(_deps.logger, DEBUG_FOCUS, "[FOCUS]: disableFocusMotor: stopping motor and waiting.");
    _axes.focus()->stop();
    waitUntilStopped(FOCUSING);

    if (FOCUSER_ALWAYS_ON == 0 && kHasFocusStepper)
    {
        _deps.focusEnablePin->write(hal::PinLevel::High);  // Logic HIGH to disable driver
    }
}

/////////////////////////////////
//
// enableFocusMotor
//
/////////////////////////////////
void MountExecutor::enableFocusMotor()
{
    OAT_LOG(_deps.logger, DEBUG_FOCUS, "[FOCUS]: enableFocusMotor: enabling driver pin.");
    _deps.focusEnablePin->write(hal::PinLevel::Low);  // Logic LOW to enable driver
}

/////////////////////////////////
//
// focusStop
//
/////////////////////////////////
void MountExecutor::focusStop()
{
    OAT_LOG(_deps.logger, DEBUG_FOCUS, "[FOCUS]: focusStop: stopping motor.");
    _coordinator.focus().stop();
}

/////////////////////////////////
//
// setTrackingStepperPos
//
/////////////////////////////////
void MountExecutor::setTrackingStepperPos(long stepPos)
{
    _axes.tracking()->setCurrentPosition(stepPos);
}

/// Records a fact for the tick boundary. The next `loop()` delivers it, so a subscriber
/// is never called from inside the code that just changed the mount's state.
void MountExecutor::publishEvent(core::MountEventType type, bool succeeded)
{
    core::MountEvent event;
    event.type      = type;
    event.succeeded = succeeded;
    _coordinator.events().publish(event);
}

void MountExecutor::setStatus(int state)
{
    // The bitfield is a read-modify-write; hal's guard is what makes it safe against the step
    // generator. See hal/CriticalSection.hpp.
    const hal::CriticalSection lock;
    _coordinator.state().reset((uint16_t) state);
}

void MountExecutor::setStatusFlag(int flag)
{
    // The bitfield is a read-modify-write; hal's guard is what makes it safe against the step
    // generator. See hal/CriticalSection.hpp.
    const hal::CriticalSection lock;
    _coordinator.state().set((uint16_t) flag);
}

void MountExecutor::clearStatusFlag(int flag)
{
    // The bitfield is a read-modify-write; hal's guard is what makes it safe against the step
    // generator. See hal/CriticalSection.hpp.
    const hal::CriticalSection lock;
    _coordinator.state().clear((uint16_t) flag);
}

/////////////////////////////////
//
// slewStatus
//
// Returns the current state of the motors and is a bitfield with these flags:
// NOT_SLEWING is all zero. SLEWING_DEC, SLEWING_RA, SLEWING_BOTH, SLEWING_TRACKING are bits.
/////////////////////////////////

bool MountExecutor::isSlewingRAorDEC() const
{
    if (isParking())
        return true;
    return (slewStatus() & (SLEWING_DEC | SLEWING_RA)) != 0;
}

bool MountExecutor::isSlewingIdle() const
{
    if (isParking())
        return false;
    return (slewStatus() & (SLEWING_DEC | SLEWING_RA)) == 0;
}

/////////////////////////////////
//
// isParking
//
/////////////////////////////////
bool MountExecutor::isParking() const
{
    return _coordinator.parking().isParking();
}

void MountExecutor::applyParkingStatus()
{
    clearStatusFlag(STATUS_PARKING | STATUS_PARKING_POS);
    const uint16_t bit = core::ParkingController::statusBitFor(_coordinator.parking().phase());
    if (bit != 0)
    {
        setStatusFlag(bit);
    }
}

core::MountActivityFacts MountExecutor::activityFacts() const
{
    return _coordinator.activityFacts();
}

bool MountExecutor::admit(core::MountRequest request) const
{
    return _coordinator.admit(request) == core::Admission::Allowed;
}

// A board whose driver takes its microsteps from pin strapping holds the null driver, whose
// `microsteps()` does nothing, so these are the same code on every board.
void MountExecutor::setRaMicrosteps(int microsteps)
{
    OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: RA driver setMicrosteps(%d)", microsteps);
    _axes.raDriver()->microsteps(core::microstepRegisterValue(microsteps));
}

void MountExecutor::setDecMicrosteps(int microsteps)
{
    OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: DEC driver setMicrosteps(%d)", microsteps);
    _axes.decDriver()->microsteps(core::microstepRegisterValue(microsteps));
}

/////////////////////////////////
//
// startSlewing
//
// Starts manual slewing in one of eight directions or
// tracking, but only if not currently parking!
/////////////////////////////////
void MountExecutor::startSlewing(int direction)
{
    // Parking owns both axes until it finishes; every request in this function is
    // refused while it runs. See core/MountRequests.hpp for the whole table.
    if (!admit((direction & TRACKING) ? core::MountRequest::Track : core::MountRequest::ManualSlew))
    {
        return;
    }

    stopGuiding();

    if (direction & TRACKING)
    {
        setRaMicrosteps(_config.raTrackingMicrosteps());
        _coordinator.tracking().applySpeed();

        setStatusFlag(STATUS_TRACKING);
        publishEvent(core::MountEventType::TrackingStarted);
        return;
    }

    // A manual slew runs at the last commanded rate.
    setSlewRate(_settings.moveRate());
    if (isSlewingTRK())
    {
        stopSlewing(TRACKING);
        _coordinator.tracking().noteStopped(_deps.clock->now());
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: startSlewing: stopped TRK at %ld", _coordinator.tracking().stoppedAt());
    }

    // Both axes run at slew microsteps while a manual move is in flight.
    setRaMicrosteps(_config.raSlewMicrosteps);
    setDecMicrosteps(_config.decSlewMicrosteps);

    core::ManualSlewFacts facts;
    facts.raStepsPerDegree       = _settings.stepsPerDegree(RA_STEPS);
    facts.decStepsPerDegree      = _settings.stepsPerDegree(DEC_STEPS);
    facts.raPhysicalLimitDegrees = RA_PHYSICAL_LIMIT;
    facts.raTrackingLimitDegrees = RA_TRACKING_LIMIT;
    facts.decLimitUpDegrees      = DEC_LIMIT_UP;
    facts.decLimitDownDegrees    = DEC_LIMIT_DOWN;
    facts.decUpperLimitSteps     = _settings.decUpperLimitSteps();
    facts.decLowerLimitSteps     = _settings.decLowerLimitSteps();
    facts.trackedHours           = getTrackedHours();
    facts.northernHemisphere     = inNorthernHemisphere;

    const core::ManualSlewTargets targets = core::manualSlewTargets(static_cast<uint8_t>(direction), facts);

    if (targets.movesDec)
    {
        OAT_LOG(_deps.logger,
                DEBUG_STEPPERS,
                "[STEPPERS]: startSlewing: DEC targetMoveTo is %ld (limits %ld -> %ld)",
                targets.decSteps,
                _settings.decLowerLimitSteps(),
                _settings.decUpperLimitSteps());
        _axes.dec()->moveTo(targets.decSteps);
        setStatusFlag(STATUS_SLEWING);
    }

    if (targets.movesRa)
    {
        OAT_LOG(_deps.logger,
                DEBUG_STEPPERS,
                "[STEPPERS]: startSlewing: RA targetMoveTo is %ld (adjusted for %fh tracked)",
                targets.raSteps,
                facts.trackedHours);
        _axes.ra()->moveTo(targets.raSteps);
        setStatusFlag(STATUS_SLEWING);
    }
}

/////////////////////////////////
//
// stopSlewing
//
// Stop manual slewing in one of two directions or Tracking. NS is the same. EW is the same
/////////////////////////////////
void MountExecutor::stopSlewing(int direction)
{
    if (direction & TRACKING)
    {
        // Turn off tracking
        clearStatusFlag(STATUS_TRACKING);
        publishEvent(core::MountEventType::TrackingStopped);

        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: stopSlewing: TRK stepper stop()");
        _coordinator.tracking().stop();
    }

    if ((direction & (NORTH | SOUTH)) != 0)
    {
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: stopSlewing: DEC stepper stop()");
        _axes.dec()->stop();
    }

    if ((direction & (WEST | EAST)) != 0)
    {
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: stopSlewing: RA stepper stop()");
        _axes.ra()->stop();
        if (isFindingHome())
        {
            clearStatusFlag(STATUS_FINDING_HOME);
        }
    }
}

void MountExecutor::stopSlewing(StepperAxis axis)
{
    if (axis == RA_STEPS)
    {
        _axes.ra()->stop();
    }
    else if (axis == DEC_STEPS)
    {
        _axes.dec()->stop();
    }
    else if (axis == AZIMUTH_STEPS)
    {
        if (kHasAzStepper)
        {
            _axes.az()->stop();
        }
    }
    else if (axis == ALTITUDE_STEPS)
    {
        if (kHasAltStepper)
        {
            _axes.alt()->stop();
        }
    }
    else if (axis == FOCUS_STEPS)
    {
        if (kHasFocusStepper)
        {
            _axes.focus()->stop();
        }
    }
}

/////////////////////////////////
//
// waitUntilStopped
//
/////////////////////////////////
// Block until the RA and DEC motors are stopped
void MountExecutor::waitUntilStopped(byte direction)
{
    while (((direction & (EAST | WEST)) && _axes.ra()->isRunning()) || ((direction & (NORTH | SOUTH)) && _axes.dec()->isRunning())
           || ((direction & TRACKING) && ((!_coordinator.state().test(STATUS_TRACKING)) && _axes.tracking()->isRunning()))
           || ((direction & FOCUSING) && kHasFocusStepper && _axes.focus()->isRunning()))
    {
        loop();
        yield();
    }
}

/////////////////////////////////
//
// waitUntilAllStopped
//
/////////////////////////////////
// Block until all steppers are stopped
void MountExecutor::waitUntilAllStopped()
{
    while (_axes.ra()->isRunning() || _axes.dec()->isRunning()
           || ((!_coordinator.state().test(STATUS_TRACKING)) && _axes.tracking()->isRunning())
           || (kHasFocusStepper && _axes.focus()->isRunning()) || (kHasAzStepper && _axes.az()->isRunning())
           || (kHasAltStepper && _axes.alt()->isRunning()))
    {
        loop();
        yield();
    }
}

long MountExecutor::getCurrentStepperPosition(int direction)
{
    if (direction & TRACKING)
    {
        return _axes.tracking()->currentPosition();
    }
    if (direction & (NORTH | SOUTH))
    {
        return _axes.dec()->currentPosition();
    }
    if (direction & (EAST | WEST))
    {
        return _axes.ra()->currentPosition();
    }
    return 0;
}

long MountExecutor::getCurrentStepperPosition(StepperAxis axis)
{
    if (axis == StepperAxis::DEC_STEPS)
    {
        return _axes.dec()->currentPosition();
    }
    if (axis == StepperAxis::RA_STEPS)
    {
        return _axes.ra()->currentPosition();
    }
    if (axis == StepperAxis::FOCUS_STEPS)
    {
        if (kHasFocusStepper)
        {
            return _axes.focus()->currentPosition();
        }
    }
    if (axis == StepperAxis::ALTITUDE_STEPS)
    {
        if (kHasAltStepper)
        {
            return _axes.alt()->currentPosition();
        }
    }
    if (axis == StepperAxis::AZIMUTH_STEPS)
    {
        if (kHasAzStepper)
        {
            return _axes.az()->currentPosition();
        }
    }
    return 0;
}

/////////////////////////////////
//
// setHomingOffset
//
/////////////////////////////////
void MountExecutor::setHomingOffset(StepperAxis axis, long offset)
{
    if (axis == StepperAxis::RA_STEPS)
    {
        _deps.settings->storeRAHomingOffset(offset);
        OAT_LOG(_deps.logger, DEBUG_MOUNT, "[MOUNT]: setHomingOffset(RA): Offset: %ld", offset);
    }
    if (axis == StepperAxis::DEC_STEPS)
    {
        _deps.settings->storeDECHomingOffset(offset);
        OAT_LOG(_deps.logger, DEBUG_MOUNT, "[MOUNT]: setDECHomingOffset(DEC): Offset: %ld", offset);
    }
}

long MountExecutor::getHomingOffset(StepperAxis axis)
{
    if (axis == StepperAxis::RA_STEPS)
    {
        return _deps.settings->getRAHomingOffset();
    }
    else if (axis == StepperAxis::DEC_STEPS)
    {
        return _deps.settings->getDECHomingOffset();
    }
    return 0;
}

/////////////////////////////////
//
// findHomeByHallSensor
//
/////////////////////////////////
#if (USE_HALL_SENSOR_RA_AUTOHOME == 1) || (USE_HALL_SENSOR_DEC_AUTOHOME == 1)
bool MountExecutor::findHomeByHallSensor(StepperAxis axis, int initialDirection, int searchDistance)
{
    #if USE_HALL_SENSOR_RA_AUTOHOME == 1
    if (axis == StepperAxis::RA_STEPS)
    {
        if (_axes.raHoming() == nullptr)
        {
            return false;
        }
        const bool started = _axes.raHoming()->findHomeByHallSensor(initialDirection, searchDistance);
        if (started)
        {
            publishEvent(core::MountEventType::HomingStarted);
        }
        return started;
    }
    #endif

    #if USE_HALL_SENSOR_DEC_AUTOHOME == 1
    if (axis == StepperAxis::DEC_STEPS)
    {
        if (_axes.decHoming() == nullptr)
        {
            return false;
        }
        const bool started = _axes.decHoming()->findHomeByHallSensor(initialDirection, searchDistance);
        if (started)
        {
            publishEvent(core::MountEventType::HomingStarted);
        }
        return started;
    }
    #endif
    return false;
}
#endif

/////////////////////////////////
//
// processHomingProgress
//
/////////////////////////////////
/// A board with no hall sensor compiles both branches out, so this becomes the no-op the
/// mount loop calls every tick.
void MountExecutor::processHomingProgress()
{
    if (kHasRaHoming && (_axes.raHoming() != nullptr) && (!_axes.raHoming()->isIdleOrComplete()))
    {
        _axes.raHoming()->processHomingProgress();
        if (_axes.raHoming()->isIdleOrComplete())
        {
            publishEvent(core::MountEventType::HomingFinished, _axes.raHoming()->homingResult() == core::HomingResult::Succeeded);
        }
    }

    if (kHasDecHoming && (_axes.decHoming() != nullptr) && (!_axes.decHoming()->isIdleOrComplete()))
    {
        _axes.decHoming()->processHomingProgress();
        if (_axes.decHoming()->isIdleOrComplete())
        {
            publishEvent(core::MountEventType::HomingFinished, _axes.decHoming()->homingResult() == core::HomingResult::Succeeded);
        }
    }
}

/// End switches are polled twice: once to debounce the raw pin, once to act on the
/// debounced state. On a board without them both calls compile out.
void MountExecutor::pollEndSwitches()
{
    if (kHasRaEndSwitch && (_axes.raEndSwitch() != nullptr))
    {
        _axes.raEndSwitch()->processEndSwitchState();
        _axes.raEndSwitch()->checkSwitchState();
    }

    if (kHasDecEndSwitch && (_axes.decEndSwitch() != nullptr))
    {
        _axes.decEndSwitch()->processEndSwitchState();
        _axes.decEndSwitch()->checkSwitchState();
    }
}

/// The Meade status fragment for one axis' sensor. A board without the sensor reports the
/// same "NEVER RUN"/empty shape a sensor that has never homed does.

#if (USE_RA_END_SWITCH == 1 || USE_DEC_END_SWITCH == 1)
/////////////////////////////////
//
// End Switches RA/DEC
//
#endif

/////////////////////////////////
//
// delay
//
/////////////////////////////////
void MountExecutor::delay(int ms)
{
    // A bounded wait: the deadline comes from the clock port and the yield from the scheduler
    // port, so this needs no direct millis()/yield().
    //
    // It deliberately does **not** call `loop()` any more. Every caller is a menu screen, i.e.
    // already inside a tick, so re-entering the update path from here ran the arrival sequence and
    // the coordinator's tick from inside a handler — re-entrancy nobody could reason about, and
    // the reason a menu calibration could interleave with a slew arriving. The steppers still step
    // (the AVR timer ISR, or the ESP32 stepper task), so a wait does not stop motion; what it no
    // longer does is run the mount's bookkeeping behind the menu's back.
    const unsigned long start = _deps.clock->now();
    while (_deps.clock->now() - start < (unsigned long) ms)
    {
        _deps.scheduler->yield();
    }
}

/////////////////////////////////
//
// loop
//
// Process any stepper changes.
/////////////////////////////////
void MountExecutor::loop()
{
    bool raStillRunning  = false;
    bool decStillRunning = false;

    unsigned long now = _deps.clock->now();

#if (DEBUG_LEVEL & DEBUG_MOUNT) && (DEBUG_LEVEL & DEBUG_VERBOSE)
    if (now - _lastMountPrint > 2000)
    {
        OAT_LOG(_deps.logger, DEBUG_MOUNT, "[MOUNT]: Status -> %s", getStatusString());
        _lastMountPrint = now;
    }
#endif

    // LOG(DEBUG_MOUNT, "[MOUNT]: Focuser running:  %d", _axes.focus()->isRunning());

    if (_coordinator.focus().isRunning())
    {
        OAT_LOG(_deps.logger, DEBUG_FOCUS, "[MOUNT]: Loop: Focuser running at speed %f", _coordinator.focus().speed());
    }

    bool oneIsRunning = false;
    if (kHasAzStepper)
    {
        oneIsRunning |= _axes.az()->isRunning();
    }
    if (kHasAltStepper)
    {
        oneIsRunning |= _axes.alt()->isRunning();
    }

    // The controller half of a tick: dispatch published facts, poll the axes, then the guide
    // pulse. Everything it decides is reported back, because the pins and the EEPROM live
    // here. See `core::MountCoordinator::tick()`.
    const core::TickOutcome tick = _coordinator.tick(oneIsRunning);

    if (tick.azAltJustStopped)
    {
        // One of the motors was running last time through the loop, but not anymore, so shutdown the outputs.
        disableAzAltMotors();
        if (kHasAzStepper)
        {
            _deps.settings->storeAZPosition(_axes.az()->currentPosition());
        }
        if (kHasAltStepper)
        {
            _deps.settings->storeALTPosition(_axes.alt()->currentPosition());
        }
    }

    if (tick.focusStopped)
    {
        OAT_LOG(_deps.logger, DEBUG_FOCUS, "[MOUNT]: Loop: Focuser is stopped, but was running ");
        OAT_LOG(_deps.logger, DEBUG_FOCUS, "[MOUNT]: Loop: Focuser is stopped, but was running, disabling");
        disableFocusMotor();
        publishEvent(core::MountEventType::FocusStopped);
    }

    if (tick.guiding)
    {
        now = _deps.clock->now();
        if (tick.guidingRaExpired || tick.guidingDecExpired)
        {
            OAT_LOG(_deps.logger,
                    DEBUG_GUIDE,
                    "[MOUNT]: Loop: StopGuiding. Now: %ld, RA End: %ld, DEC End: %ld",
                    now,
                    _coordinator.guiding().raDeadline(),
                    _coordinator.guiding().decDeadline());
            stopGuiding(tick.guidingRaExpired, tick.guidingDecExpired);
        }
        return;
    }

    if (_axes.dec()->isRunning())
    {
        decStillRunning = true;
    }

    if (_axes.ra()->isRunning())
    {
        raStillRunning = true;
    }

    {
        // Check whether we should stop tracking every 5 seconds
        if (now - _lastTRKCheck > 5000)
        {
            checkRALimit();
        }

        //
        // Arrived at target after Slew!
        //
        clearStatusFlag(STATUS_SLEWING | STATUS_SLEWING_TO_TARGET | STATUS_SLEWING_MANUAL);

        if (_coordinator.slew().wasRunning())
        {
            publishEvent(core::MountEventType::SlewArrived);
            OAT_LOG(_deps.logger,
                    DEBUG_MOUNT | DEBUG_STEPPERS,
                    "[MOUNT]: Loop: Reached target. RA:%ld, DEC:%ld",
                    _axes.ra()->currentPosition(),
                    _axes.dec()->currentPosition());

            // The rules for what happens at the end of a slew live in
            // core/SlewCompletion.hpp, as a plan this loop executes in order. The facts they
            // are decided from belong to the controllers, so the coordinator assembles them.
            const core::CompletionPlan plan = _coordinator.completionPlan();

            // Set once the park leg is programmed: true when there is nowhere to go.
            bool parkLegIsNoOp = false;

            // A park just reached home: stop tracking, and record the home position
            // unless this is the second leg (which starts from it).
            if (plan.stopTracking)
            {
                stopSlewing(TRACKING);
            }
            if (plan.setHome)
            {
                // setHome() resets the slew, and the rest of this block still needs to
                // know what the slew was for.
                const core::SlewSnapshot savedSlew = _coordinator.slew().snapshot();
                OAT_LOG(_deps.logger,
                        DEBUG_MOUNT | DEBUG_STEPPERS,
                        "[MOUNT]: Loop:   Was Parking, not slewing to park so stop tracking and set home. SlewToHome: %d",
                        _coordinator.slew().intent() == core::SlewIntent::Home);
                setHome(false);
                _coordinator.slew().restore(savedSlew);
            }

            _currentRAStepperPosition = _axes.ra()->currentPosition();
            if (plan.raTrackingMicrosteps)  // RA stays in slew configuration while homing
            {
                setRaMicrosteps(_config.raTrackingMicrosteps());
            }

            if (plan.compensateTracker)
            {
                now                          = _deps.clock->now();
                const long compensationSteps = _coordinator.tracking().compensationSteps(now);
                OAT_LOG(_deps.logger,
                        DEBUG_STEPPERS,
                        "[STEPPERS]: loop: Arrived at %ldms. Tracking was off for %ldms (%ld steps), compensating.",
                        now,
                        now - _coordinator.tracking().stoppedAt(),
                        compensationSteps);
                _coordinator.tracking().applyCompensation(compensationSteps);

                OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: loop: compensation complete.");
            }

            if (plan.startTracking)
            {
                OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: Loop: Not parking and not finding home, so start tracking");
                startSlewing(TRACKING);
            }

            // Guiding is always ready again once the slew is over.
            if (plan.decGuideMicrosteps)
            {
                setDecMicrosteps(_config.decGuidingMicrosteps());
            }

            if (plan.correctBacklash)
            {
                OAT_LOG(_deps.logger,
                        DEBUG_MOUNT | DEBUG_STEPPERS,
                        "[MOUNT]: Loop:   Reached target at %d. Compensating for backlash by %d",
                        (int) _currentRAStepperPosition,
                        _settings.backlashCorrectionSteps());
                _currentRAStepperPosition += _settings.backlashCorrectionSteps();
                _axes.ra()->runToNewPosition(_currentRAStepperPosition);
                _coordinator.slew().clearBacklashCorrection();
                OAT_LOG(_deps.logger,
                        DEBUG_MOUNT | DEBUG_STEPPERS,
                        "[MOUNT]: Loop:   Backlash correction done. Pos: %d",
                        _currentRAStepperPosition);
            }
            else
            {
                OAT_LOG(_deps.logger,
                        DEBUG_MOUNT | DEBUG_STEPPERS,
                        "[MOUNT]: Loop:   Reached target at %d, no backlash compensation needed",
                        _currentRAStepperPosition);
            }

            if (plan.retargetRa)
            {
                OAT_LOG(_deps.logger, DEBUG_MOUNT | DEBUG_STEPPERS, "[MOUNT]: Loop:   Was Slewing home...");
                _targetRA = currentRA();
            }

            if (plan.startParkPositionLeg)
            {
                // Home is known; now go to the configured park position.
                setDecMicrosteps(_config.decSlewMicrosteps);
                OAT_LOG(_deps.logger,
                        DEBUG_MOUNT | DEBUG_STEPPERS,
                        "[MOUNT]: Loop:   Was parking, so no tracking. Proceeding to park position...");
                _coordinator.parking().beginPositionLeg();
                applyParkingStatus();
                _axes.ra()->moveTo(-getHomingOffset(StepperAxis::RA_STEPS));
                _axes.dec()->moveTo(-getHomingOffset(StepperAxis::DEC_STEPS));
                _coordinator.slew().setTotalMove(1.0f * _axes.ra()->distanceToGo(), 1.0f * _axes.dec()->distanceToGo());
                OAT_LOG(_deps.logger,
                        DEBUG_MOUNT | DEBUG_STEPPERS,
                        "[MOUNT]: Loop:   Park Position is R:%ld  D:%ld, TotalMove is R:%f, D:%f",
                        -getHomingOffset(StepperAxis::RA_STEPS),
                        -getHomingOffset(StepperAxis::DEC_STEPS),
                        _coordinator.slew().totalRaSteps(),
                        _coordinator.slew().totalDecSteps());
                parkLegIsNoOp = core::SlewCompletion::parkLegIsNoOp(_axes.ra()->distanceToGo(), _axes.dec()->distanceToGo());
                if (parkLegIsNoOp)
                {
                    OAT_LOG(_deps.logger, DEBUG_MOUNT | DEBUG_STEPPERS, "[MOUNT]: Loop:   Already at Parking pos, so done.");
                }
                else
                {
                    OAT_LOG(_deps.logger,
                            DEBUG_MOUNT | DEBUG_STEPPERS,
                            "[MOUNT]: Loop:   Distance to Parking is non-zero, slewing to park position...");
                    setStatusFlag(STATUS_PARKING_POS | STATUS_SLEWING);
                }
            }

            if (plan.restartTrackingAfterHome)
            {
                OAT_LOG(_deps.logger, DEBUG_MOUNT | DEBUG_STEPPERS, "[MOUNT]: Loop:   Slewed home, not parking, so restart tracking.");
                startSlewing(TRACKING);
            }

            if (plan.decGuideAfterHome)
            {
                setDecMicrosteps(_config.decGuidingMicrosteps());
            }

            if (plan.parkComplete)
            {
                OAT_LOG(_deps.logger, DEBUG_MOUNT | DEBUG_STEPPERS, "[MOUNT]: Loop:   Arrived at park position...");
            }

            // Both ways of finishing a park end in the same two writes.
            if (plan.parkComplete || parkLegIsNoOp)
            {
                noInterrupts();
                setStatus(STATUS_PARKED);
                interrupts();
                publishEvent(core::MountEventType::Parked);
                _coordinator.parking().markParked();
                applyParkingStatus();
            }

            if (plan.endSlew)
            {
                _coordinator.slew().end();
            }
            _coordinator.slew().clearTotalMove();
        }
    }

#if SUPPORT_DRIFT_ALIGNMENT == 1
    updateDriftAlignment(now);
#endif

    if (_coordinator.state().test(STATUS_FINDING_HOME))
    {
        processHomingProgress();
    }

    pollEndSwitches();

    _coordinator.finishTick(raStillRunning, decStillRunning);
}

/////////////////////////////////
//
// bootComplete
//
/////////////////////////////////
void MountExecutor::bootComplete()
{
    _bootComplete = true;
}

/////////////////////////////////
//
// isBootComplete
//
/////////////////////////////////
bool MountExecutor::isBootComplete()
{
    return _bootComplete;
}

/////////////////////////////////
//
// setDecLimitPosition
//
// If limitAngle is 0, no offset is given, so the current position is used.
/////////////////////////////////
void MountExecutor::setDecLimitPosition(bool upper, float limitAngle)
{
    if (upper)
    {
        if (limitAngle == 0)
        {
            _settings.setDecUpperLimitSteps(_axes.dec()->currentPosition());
            _deps.settings->storeDECUpperLimit(fabsf(_settings.decUpperLimitSteps() / _settings.stepsPerDegree(DEC_STEPS)));
        }
        else
        {
            _settings.setDecUpperLimitSteps((limitAngle * _settings.stepsPerDegree(DEC_STEPS)));
            _deps.settings->storeDECUpperLimit(limitAngle);
        }
        OAT_LOG(_deps.logger,
                DEBUG_MOUNT,
                "[MOUNT]: setDecLimitPosition(Upper) to %f: limit DEC: %ld -> %ld",
                limitAngle,
                _settings.decLowerLimitSteps(),
                _settings.decUpperLimitSteps());
    }
    else
    {
        if (limitAngle == 0)
        {
            _settings.setDecLowerLimitSteps(_axes.dec()->currentPosition());
            _deps.settings->storeDECLowerLimit(fabsf(_settings.decLowerLimitSteps() / _settings.stepsPerDegree(DEC_STEPS)));
        }
        else
        {
            _settings.setDecLowerLimitSteps(-(limitAngle * _settings.stepsPerDegree(DEC_STEPS)));
            _deps.settings->storeDECLowerLimit(limitAngle);
        }
        OAT_LOG(_deps.logger,
                DEBUG_MOUNT,
                "[MOUNT]: setDecLimitPosition(Lower) to %f: limit DEC: %ld -> %ld",
                limitAngle,
                _settings.decLowerLimitSteps(),
                _settings.decUpperLimitSteps());
    }
}

/////////////////////////////////
//
// clearDecLimitPosition
//
/////////////////////////////////
void MountExecutor::clearDecLimitPosition(bool upper)
{
    if (upper)
    {
        _settings.setDecUpperLimitSteps(DEC_LIMIT_UP * _settings.stepsPerDegree(DEC_STEPS));
        _deps.settings->storeDECUpperLimit(DEC_LIMIT_UP);
        OAT_LOG(_deps.logger,
                DEBUG_MOUNT,
                "[MOUNT]: clearDecLimitPosition(Upper): limit DEC: %ld -> %ld",
                _settings.decLowerLimitSteps(),
                _settings.decUpperLimitSteps());
    }
    else
    {
        _settings.setDecLowerLimitSteps(-(DEC_LIMIT_DOWN * _settings.stepsPerDegree(DEC_STEPS)));
        _deps.settings->storeDECLowerLimit(DEC_LIMIT_DOWN);
        OAT_LOG(_deps.logger,
                DEBUG_MOUNT,
                "[MOUNT]: clearDecLimitPosition(Lower): limit DEC: %ld -> %ld",
                _settings.decLowerLimitSteps(),
                _settings.decUpperLimitSteps());
    }
}

void MountExecutor::getDecLimitPositions(float &lowerLimit, float &upperLimit)
{
    lowerLimit = -1.0f * _settings.decLowerLimitSteps() / _settings.stepsPerDegree(DEC_STEPS);
    upperLimit = 1.0f * _settings.decUpperLimitSteps() / _settings.stepsPerDegree(DEC_STEPS);
    OAT_LOG(_deps.logger,
            DEBUG_MOUNT,
            "[MOUNT]: getDecLimitPositions: limit DEC: %ld -> %ld (%f -> %f)",
            _settings.decLowerLimitSteps(),
            _settings.decUpperLimitSteps(),
            lowerLimit,
            upperLimit);
}

/////////////////////////////////
//
// setHome
//
/////////////////////////////////
void MountExecutor::setHome(bool clearZeroPos)
{
    OAT_LOG(_deps.logger, DEBUG_MOUNT, "[MOUNT]: setHome() called. Stopping steppers");
    bool wasTracking = isSlewingTRK();
    stopSlewing(ALL_DIRECTIONS);
    waitUntilStopped(ALL_DIRECTIONS);
    if (wasTracking)
    {
        OAT_LOG(_deps.logger, DEBUG_MOUNT, "[MOUNT]: setHome: Tracking was on, so start it again.");
        startSlewing(TRACKING);
    }

    //LOG(DEBUG_MOUNT_VERBOSE, "[MOUNT]: setHomePre: currentRA is %s", currentRA().ToString());
    //LOG(DEBUG_MOUNT_VERBOSE, "[MOUNT]: setHomePre: targetRA is %s", targetRA().ToString());
    //LOG(DEBUG_MOUNT_VERBOSE, "[MOUNT]: setHomePre: zeroPos is %s", DayTime(_siteTime.raZeroPoint()).ToString());
    _siteTime.setRaZeroPoint(clearZeroPos ? DayTime(POLARIS_RA_HOUR, POLARIS_RA_MINUTE, POLARIS_RA_SECOND) : calculateLst());
#if defined(OAM) || defined(OAE)
    _siteTime.adjustRaZeroPoint(6.0f);  // shift all coordinates by 90° for EQ mount movement
#endif
    _zeroPosDEC = 0.0f;

    _axes.ra()->setCurrentPosition(0);
    _axes.dec()->setCurrentPosition(0);
    _axes.tracking()->setCurrentPosition(0);
    _axes.guide()->setCurrentPosition(0);

    _targetRA = currentRA();
    _coordinator.slew().end();
    // `setHome()` is also called by the park sequence itself, which re-establishes its
    // phase straight afterwards; clearing here only affects a hand-driven home.
    if (_coordinator.parking().isParked())
    {
        _coordinator.parking().clear();
        applyParkingStatus();
    }

    //LOG(DEBUG_MOUNT_VERBOSE, "[MOUNT]: setHomePost: currentRA is %s", currentRA().ToString());
    //LOG(DEBUG_MOUNT_VERBOSE, "[MOUNT]: setHomePost: zeroPos is %s", DayTime(_siteTime.raZeroPoint()).ToString());
    //LOG(DEBUG_MOUNT_VERBOSE, "[MOUNT]: setHomePost: targetRA is %s", targetRA().ToString());
}

/////////////////////////////////
//
// getSpeed
//
// Get the current speed of the stepper. NORTH, WEST, TRACKING
/////////////////////////////////
float MountExecutor::getSpeed(int direction)
{
    if (direction & TRACKING)
    {
        return _coordinator.tracking().speed();
    }
    if (direction & (NORTH | SOUTH))
    {
        return _axes.dec()->speed();
    }
    if (direction & (EAST | WEST))
    {
        return _axes.ra()->speed();
    }
    return 0;
}

/////////////////////////////////
//
// calculateStepperPositions
//
// This code calculates the stepper locations to move to, given the right ascension and declination
/////////////////////////////////
void MountExecutor::calculateStepperPositions(float raCoord, float decCoord, long &raPos, long &decPos)
{
    DayTime savedRA      = _targetRA;
    Declination savedDec = _targetDEC;
    _targetRA            = DayTime(raCoord);
    _targetDEC           = Declination::FromSeconds(decCoord * 3600.0f);
    calculateRAandDECSteppers(raPos, decPos);
    _targetRA  = savedRA;
    _targetDEC = savedDec;
}

/////////////////////////////////
//
// calculateRAandDECSteppers
//
// This code tells the steppers to what location to move to, given the select right ascension and declination
/////////////////////////////////
void MountExecutor::calculateRAandDECSteppers(long &targetRASteps, long &targetDECSteps, long pSolutions[6]) const
{
    OAT_LOG(_deps.logger,
            DEBUG_COORD_CALC,
            "[MOUNT]: CalcSteppersPre: Current : RA: %s, DEC: %s",
            DayTime(currentRA()).ToString(),
            Declination(currentDEC()).ToString());
    OAT_LOG(
        _deps.logger, DEBUG_COORD_CALC, "[MOUNT]: CalcSteppersPre: Target  : RA: %s, DEC: %s", _targetRA.ToString(), _targetDEC.ToString());
    OAT_LOG(_deps.logger, DEBUG_COORD_CALC, "[MOUNT]: CalcSteppersPre: ZeroRA  : %s", DayTime(_siteTime.raZeroPoint()).ToString());
    OAT_LOG(_deps.logger, DEBUG_COORD_CALC, "[MOUNT]: CalcSteppersPre: ZeroDEC : %f", _zeroPosDEC);
    OAT_LOG(_deps.logger,
            DEBUG_COORD_CALC,
            "[MOUNT]: CalcSteppersPre: Stepper: RA: %ld, DEC: %ld, TRK: %ld",
            _axes.ra()->currentPosition(),
            _axes.dec()->currentPosition(),
            _axes.tracking()->currentPosition());
    DayTime raTarget      = _targetRA;
    Declination decTarget = _targetDEC;

    // Calculate how far from the home position this new target is.
    raTarget.subtractTime(DayTime(_siteTime.raZeroPoint()));
    OAT_LOG(_deps.logger,
            DEBUG_COORD_CALC,
            "[MOUNT]: CalcSteppersIn: Adjust RA by ZeroPosRA. New Target RA: %s, DEC: %s",
            raTarget.ToString(),
            _targetDEC.ToString());

    // Total hours of tracking-to-date
    const float trackedHours = getTrackedHours();
    OAT_LOG(_deps.logger,
            DEBUG_COORD_CALC,
            "[MOUNT]: CalcSteppersIn: Tracked time is %ld steps (%f h).",
            _axes.tracking()->currentPosition(),
            trackedHours);

    core::SlewGeometryFacts facts;
    facts.targetRaHours      = _targetRA.getTotalHours();
    facts.targetDecDegrees   = decTarget.getTotalDegrees();
    facts.zeroPosRaHours     = DayTime(_siteTime.raZeroPoint()).getTotalHours();
    facts.zeroPosDecDegrees  = _zeroPosDEC;
    facts.trackedHours       = trackedHours;
    facts.raStepsPerDegree   = _settings.stepsPerDegree(RA_STEPS);
    facts.decStepsPerDegree  = _settings.stepsPerDegree(DEC_STEPS);
    facts.raLimitLeftHours   = RA_LIMIT_LEFT;
    facts.raLimitRightHours  = RA_LIMIT_RIGHT;
    facts.northernHemisphere = inNorthernHemisphere;

    const core::SlewGeometry geometry = core::buildSlewGeometry(facts);

    if (!inNorthernHemisphere)
    {
        OAT_LOG(
            _deps.logger, DEBUG_COORD_CALC, "[MOUNT]: CalcSteppersIn: moveDEC inverted for southern Hemisphere => %f", geometry.moveDEC);
    }

    OAT_LOG(_deps.logger,
            DEBUG_COORD_CALC,
            "[MOUNT]: CalcSteppersIn: Target hrs pos RA: Delta:%f (moveRA: %f), DEC: %s (moveDEC: %f)",
            geometry.homeTargetDeltaRA,
            geometry.moveRA,
            decTarget.ToString(),
            geometry.moveDEC);

    /*
  * Current RA wheel has a rotation limit of around 7 hours in each direction from home position.
  * Since tracking does not trigger the meridian flip, we try to extend the possible tracking time 
  * without reaching the RA ring end by executing the meridian flip before slewing to the target.
  * For this flip the RA and DEC rings have to be flipped by 180° (which is 12 RA hours). Based
  * on the physical RA ring limits, this means that the flip can only be executed during +/-[5h to 7h]
  * sections around the home position of RA. The tracking time will still be limited to around 2h in
  * worst case if the target is located right before the 5h mark during slewing. 
  */
    OAT_LOG(_deps.logger, DEBUG_COORD_CALC, "[MOUNT]: CalcSteppersIn: Limits are : %f to %f", geometry.raLimitL, geometry.raLimitR);

    const core::SlewResult result = core::calculateStepperPositions(geometry);

    if (pSolutions != nullptr)
    {
        for (int i = 0; i < 6; i++)
        {
            pSolutions[i] = result.solutions[i];
        }
    }

    OAT_LOG(_deps.logger, DEBUG_COORD_CALC, "[MOUNT]: CalcSteppersIn: Solution 1: %f, %f", -geometry.moveRA, geometry.moveDEC);
    OAT_LOG(_deps.logger, DEBUG_COORD_CALC, "[MOUNT]: CalcSteppersIn: Solution 2: %f, %f", -(geometry.moveRA - 12.0f), -geometry.moveDEC);
    OAT_LOG(_deps.logger, DEBUG_COORD_CALC, "[MOUNT]: CalcSteppersIn: Solution 3: %f, %f", -(geometry.moveRA + 12.0f), -geometry.moveDEC);

    switch (result.selected)
    {
        case core::SlewSolution::FlipUpper:
            OAT_LOG(
                _deps.logger,
                DEBUG_COORD_CALC,
                "[MOUNT]: CalcSteppersIn: Using Solution 2, since hometargetDeltaRA %f (RA:%f) is past max limit %f, inverting both axes",
                geometry.homeTargetDeltaRA,
                geometry.moveRA,
                geometry.raLimitR);
            break;
        case core::SlewSolution::FlipLower:
            OAT_LOG(
                _deps.logger,
                DEBUG_COORD_CALC,
                "[MOUNT]: CalcSteppersIn: Using solution 3 since homeTargetDeltaRA %f (RA:%f) is past min limit: %f, inverting both axes",
                geometry.homeTargetDeltaRA,
                geometry.moveRA,
                geometry.raLimitL);
            break;
        case core::SlewSolution::Direct:
            OAT_LOG(_deps.logger,
                    DEBUG_COORD_CALC,
                    "[MOUNT]: CalcSteppersIn: Using solution 1 since targetRA %f is in range. RA: %f, DEC: %f",
                    geometry.homeTargetDeltaRA,
                    geometry.moveRA,
                    geometry.moveDEC);
            break;
    }

    // zeroPosDEC is the accumulated offset from zero (home) in degrees that one or more Sync commands have applied.
    OAT_LOG(
        _deps.logger, DEBUG_COORD_CALC, "[MOUNT]: CalcSteppersIn: Adjusted Target. RA: %f, DEC: %f", result.adjustedRA, result.adjustedDEC);
    OAT_LOG(_deps.logger,
            DEBUG_COORD_CALC,
            "[MOUNT]: CalcSteppersIn: adjusted DEC by _zeroPosDEC: %f => DEC: %f",
            _zeroPosDEC,
            result.adjustedDEC - _zeroPosDEC);

    targetRASteps  = result.targetRASteps;
    targetDECSteps = result.targetDECSteps;
    OAT_LOG(
        _deps.logger, DEBUG_COORD_CALC, "[MOUNT]: CalcSteppersPost: ResultTarget Steps RA: %ld, DEC: %ld", targetRASteps, targetDECSteps);
}

/////////////////////////////////
//
// moveSteppersTo
//
/////////////////////////////////
void MountExecutor::moveSteppersTo(float targetRASteps, float targetDECSteps, StepperAxis direction)
{  // Units are u-steps (in slew mode)
    // Show time: tell the steppers where to go!
    _coordinator.slew().clearBacklashCorrection();
    OAT_LOG(
        _deps.logger, DEBUG_STEPPERS, "[STEPPERS]: MoveSteppersTo: RA  From: %ld  To: %f", _axes.ra()->currentPosition(), targetRASteps);
    OAT_LOG(
        _deps.logger, DEBUG_STEPPERS, "[STEPPERS]: MoveSteppersTo: DEC From: %ld  To: %f", _axes.dec()->currentPosition(), targetDECSteps);

    if ((direction == RA_AND_DEC_STEPS) || (direction == RA_STEPS))
    {
        if ((_settings.backlashCorrectionSteps() != 0) && ((_axes.ra()->currentPosition() - targetRASteps) > 0))
        {
            OAT_LOG(_deps.logger,
                    DEBUG_STEPPERS,
                    "[STEPPERS]: MoveSteppersTo: Needs backlash correction of %d!",
                    _settings.backlashCorrectionSteps());
            targetRASteps -= _settings.backlashCorrectionSteps();
            _coordinator.slew().requestBacklashCorrection();
        }

        _axes.ra()->moveTo(targetRASteps);
    }

    if ((direction == RA_AND_DEC_STEPS) || (direction == DEC_STEPS))
    {
        const core::DecLimitResult limited
            = core::clampDecStepsToLimits(targetDECSteps, _settings.decLowerLimitSteps(), _settings.decUpperLimitSteps());
        if (limited.clampedToUpper)
        {
            OAT_LOG(_deps.logger,
                    DEBUG_STEPPERS,
                    "[STEPPERS]: MoveSteppersTo: DEC Upper Limit enforced. To: %ld",
                    _settings.decUpperLimitSteps());
        }
        if (limited.clampedToLower)
        {
            OAT_LOG(_deps.logger,
                    DEBUG_STEPPERS,
                    "[STEPPERS]: MoveSteppersTo: DEC Lower Limit enforced. To: %ld",
                    _settings.decLowerLimitSteps());
        }

        _axes.dec()->moveTo(limited.steps);
    }
}

/////////////////////////////////
//
// moveStepperBy
//
/////////////////////////////////
void MountExecutor::moveStepperBy(StepperAxis direction, long steps)
{
    OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: moveStepperBy: %ld", steps);

    const core::RelativeMovePlan plan = core::planRelativeMove(direction, steps, isSlewingTRK());

    if (!plan.accepted)
    {
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: moveStepperBy: MoveStepperBy does not support multiple axes.");
        return;
    }

    if (plan.stopTracking)
    {
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: moveStepperBy: Stop tracking (NEMA steppers)");
        stopSlewing(TRACKING);
        _coordinator.tracking().noteStopped(_deps.clock->now());
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: moveStepperBy: TRK stopped at %ldms", _coordinator.tracking().stoppedAt());
    }

    switch (direction)
    {
        case RA_STEPS:
            // Slew microsteps once the TRK stepper has stopped, before the status bit says a
            // slew is in flight: the step generator reads that bit.
            setRaMicrosteps(_config.raSlewMicrosteps);
            setStatusFlag(STATUS_SLEWING | STATUS_SLEWING_TO_TARGET);
            publishEvent(core::MountEventType::SlewStarted);
            _coordinator.slew().noteRunning(true);
            moveSteppersTo(_axes.ra()->currentPosition() + steps, 0, direction);
            _coordinator.slew().setTotalMove(1.0f * _axes.ra()->distanceToGo(), _coordinator.slew().totalDecSteps());
            break;

        case DEC_STEPS:
            setStatusFlag(STATUS_SLEWING | STATUS_SLEWING_TO_TARGET);
            publishEvent(core::MountEventType::SlewStarted);
            // Normal state for DEC is guide microstepping, so switch to slew microstepping here.
            setDecMicrosteps(_config.decSlewMicrosteps);
            _coordinator.slew().noteRunning(true);
            moveSteppersTo(0, _axes.dec()->currentPosition() + steps, direction);
            _coordinator.slew().setTotalMove(_coordinator.slew().totalRaSteps(), 1.0f * _axes.dec()->distanceToGo());
            break;

        case FOCUS_STEPS:
            if (kHasFocusStepper)
            {
                focusMoveBy(steps);
            }
            break;

        case AZIMUTH_STEPS:
            if (kHasAzStepper)
            {
                enableAzMotor();
                OAT_LOG(_deps.logger,
                        DEBUG_STEPPERS,
                        "[STEPPERS]: moveStepperBy: AZ from %ld to %ld",
                        _axes.az()->currentPosition(),
                        _axes.az()->currentPosition() + steps);
                _axes.az()->moveTo(_axes.az()->currentPosition() + steps);
            }
            break;

        case ALTITUDE_STEPS:
            if (kHasAltStepper)
            {
                enableAltMotor();
                _axes.alt()->moveTo(_axes.alt()->currentPosition() + steps);
            }
            break;

        case RA_AND_DEC_STEPS:
            break;  // rejected above
    }
}

/////////////////////////////////
//
// moveStepperTo
//
/////////////////////////////////
void MountExecutor::moveStepperTo(StepperAxis axis, long position)
{
    long curPos = getCurrentStepperPosition(axis);
    long delta  = position - curPos;
    moveStepperBy(axis, delta);
}

/////////////////////////////////
//
// IMountReadModel — the UI-facing reads
//
/////////////////////////////////
core::DayTime MountExecutor::hourAngle()
{
    return HA();
}

/////////////////////////////////
//
// IInfoDisplaySource
//
/////////////////////////////////

void MountExecutor::fillInfoDisplayState(ports::InfoDisplayState &state)
{
    state.raSteps           = getCurrentStepperPosition(StepperAxis::RA_STEPS);
    state.decSteps          = getCurrentStepperPosition(StepperAxis::DEC_STEPS);
    state.trkSteps          = getCurrentStepperPosition(TRACKING);
    state.raStepsPerDegree  = getStepsPerDegree(StepperAxis::RA_STEPS);
    state.decStepsPerDegree = getStepsPerDegree(StepperAxis::DEC_STEPS);

    state.raRunning  = isAxisRunning(RA_STEPS);
    state.decRunning = isAxisRunning(DEC_STEPS);
    state.altRunning = isRunningALT();
    state.azRunning  = isRunningAZ();
    state.trkRunning = isSlewingTRK();
    state.guiding    = isGuiding();

    state.tracking = _coordinator.state().test(STATUS_TRACKING);
    state.slewing  = _coordinator.state().test(STATUS_SLEWING);
    state.parked   = _coordinator.state().isParked();

    state.commandsReceived = getNumCommandsReceived();

    state.progressActive = getStepperProgress(state.raProgressPercent, state.decProgressPercent);

    state.latitudeHours  = _siteTime.latitude().getTotalHours();
    state.longitudeHours = _siteTime.longitude().getTotalHours();
    state.hoursToLimit   = checkRALimit();
    state.lst            = calculateLst();

    // The panel's coordinate text is the LCD shape the renderer drew before, produced here so the
    // renderer needs no formatter and no mount.
    const String raText  = RAString(LCD_STRING | CURRENT_STRING);
    const String decText = DECString(LCD_STRING | CURRENT_STRING);
    snprintf(state.raText, sizeof(state.raText), "%s", raText.c_str());
    snprintf(state.decText, sizeof(state.decText), "%s", decText.c_str());

    // The status line outlives this call because it is copied into the readout's scratch.
    state.statusText = _readout.panelStatusText();
}

float MountExecutor::plannedRaSteps() const
{
    return _coordinator.slew().totalRaSteps();
}

float MountExecutor::plannedDecSteps() const
{
    return _coordinator.slew().totalDecSteps();
}

// Return a string of RA in the given format. For LCDSTRING, active determines where the cursor is

/////////////////////////////////
//
// calculateLst
//
/////////////////////////////////
core::DayTime MountExecutor::calculateLst()
{
    return _siteTime.lstAt(_deps.clock->now());
}

/////////////////////////////////
//
// calculateHa
//
/////////////////////////////////
core::DayTime MountExecutor::calculateHa()
{
    DayTime lst = calculateLst();
    OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: Calculating HA from LST: %s", lst.ToString());
    DayTime ha = core::SiderealClock::hourAngle(lst.getTotalHours(), POLARIS_RA_HOUR, POLARIS_RA_MINUTE, POLARIS_RA_SECOND);
    OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: HA is: %s", ha.ToString());
    return ha;
}

/////////////////////////////////
//
// testUART
//
/////////////////////////////////
#if UART_CONNECTION_TEST_TX == 1
    #if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
void MountExecutor::testRA_UART_TX()
{
    //microsteps per driver clock tick (From TMC2209 datasheet: v[Hz] (microstep/s) = VACTUAL[2209] * 0.715Hz)
    const int speed = (RA_STEPPER_SPEED / 2) / 0.715255737f;
    //Duration in ms to move X degrees at half of the max speed
    const int duration = UART_CONNECTION_TEST_TX_DEG * (_settings.stepsPerDegree(RA_STEPS) / (RA_STEPPER_SPEED / 2)) * 1000;
    OAT_LOG(_deps.logger,
            DEBUG_STEPPERS,
            "[STEPPERS]: uartTest: Switching RA driver to microsteps(%d) for UART test",
            _config.raSlewMicrosteps);
    setRaMicrosteps(_config.raSlewMicrosteps);
    testUART_vactual(_axes.raDriver(), speed, duration);
    OAT_LOG(_deps.logger,
            DEBUG_STEPPERS,
            "[STEPPERS]: uartTest: Switching RA driver to microsteps(%d) after UART test",
            _config.raTrackingMicrosteps());
    setRaMicrosteps(_config.raTrackingMicrosteps());
}
    #endif

    #if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
void MountExecutor::testDEC_UART_TX()
{
    //microsteps per driver clock tick (From TMC2209 datasheet: v[Hz] (microstep/s) = VACTUAL[2209] * 0.715Hz)
    const int speed = (DEC_STEPPER_SPEED / 2) / 0.715255737f;
    //Duration in ms to move X degrees at half of the max speed
    const int duration = UART_CONNECTION_TEST_TX_DEG * (_settings.stepsPerDegree(DEC_STEPS) / (DEC_STEPPER_SPEED / 2)) * 1000;
    OAT_LOG(_deps.logger,
            DEBUG_STEPPERS,
            "[STEPPERS]: uartTest: Switching DEC driver to microsteps(%d) for UART test",
            _config.decSlewMicrosteps);
    setDecMicrosteps(_config.decSlewMicrosteps);
    testUART_vactual(_axes.decDriver(), speed, duration);
    OAT_LOG(_deps.logger,
            DEBUG_STEPPERS,
            "[STEPPERS]: uartTest: Switching DEC driver to microsteps(%d) after UART test",
            _config.decGuidingMicrosteps());
    setDecMicrosteps(_config.decGuidingMicrosteps());
}
    #endif

    #if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART || DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
/**
 * Runs motor at specified speed for specified duration in each direction, allowing 0.5s to stop motion after each move
 * @param[in,out] driver The stepper driver instance
 * @param[in] speed The speed to run the stepper motor at (in microsteps per driver clock tick)
 * @param[in] duration The amount of time to turn the stepper (in milliseconds)
 */
void MountExecutor::testUART_vactual(ports::IMotorDriver *driver, int _speed, int _duration)
{
    driver->VACTUAL(_speed);
    delay(_duration);
    driver->VACTUAL(0);
    driver->shaft(1);
    delay(500);
    driver->VACTUAL(_speed);
    delay(_duration);
    driver->VACTUAL(0);
    driver->shaft(0);
    delay(500);
}
    #endif
#endif

float MountExecutor::getTrackedHours() const
{
    return _coordinator.tracking().trackedHours();
}

/////////////////////////////////
//
// checkRALimit
//
/////////////////////////////////
float MountExecutor::checkRALimit()
{
    // The arithmetic and the folding live in core::RaTrackingLimit; the side effect — stopping the
    // tracker — stays here, because it owns the axes.
    core::RaLimitFacts facts;
    facts.homeRaHours        = DayTime(_siteTime.raZeroPoint()).getTotalHours() + getTrackedHours();
    facts.currentRaHours     = currentRA().getTotalHours();
    facts.decAxisDegrees     = (_axes.dec()->currentPosition() / _settings.stepsPerDegree(DEC_STEPS)) + _zeroPosDEC;
    facts.northernHemisphere = inNorthernHemisphere;
    facts.limitHours         = RA_TRACKING_LIMIT;

    const core::RaLimitResult result = core::RaTrackingLimit::evaluate(facts);

    OAT_LOG(_deps.logger,
            DEBUG_MOUNT_VERBOSE,
            "[MOUNT]: checkRALimit: homeRA: %f (ZeroPos: %f + TrkHrs: %f), remaining: %f",
            facts.homeRaHours,
            DayTime(_siteTime.raZeroPoint()).getTotalHours(),
            getTrackedHours(),
            result.remainingHours);

    if (result.limitReached)
    {
        OAT_LOG(_deps.logger,
                DEBUG_MOUNT,
                "[MOUNT]: checkRALimit: Tracking limit reached. TrkHrs:%f, HomeRA:%f, remaining:%f",
                getTrackedHours(),
                facts.homeRaHours,
                result.remainingHours);
        stopSlewing(TRACKING);
    }
    _lastTRKCheck = _deps.clock->now();

    return result.remainingHours;
}

/////////////////////////////////
//
// The read-model forwards
//
// Everything below is `app::MountReadout`'s. The executor keeps the port entry points so the
// command layer, the menus and its own motion code keep calling one object, and so that the
// class that decides when to move stays separate from the class that describes where things are.
//
/////////////////////////////////
core::DayTime MountExecutor::currentRA() const
{
    return _readout.currentRA();
}

core::Declination MountExecutor::currentDEC() const
{
    return _readout.currentDEC();
}

String MountExecutor::RAString(byte type, byte active)
{
    return _readout.RAString(type, active);
}

String MountExecutor::DECString(byte type, byte active)
{
    return _readout.DECString(type, active);
}

const char *MountExecutor::raText(uint8_t type, uint8_t active)
{
    return _readout.raText(type, active);
}

const char *MountExecutor::decText(uint8_t type, uint8_t active)
{
    return _readout.decText(type, active);
}

byte MountExecutor::slewStatus() const
{
    return _readout.slewStatus();
}

bool MountExecutor::isAxisRunning(StepperAxis axis)
{
    return _readout.isAxisRunning(axis);
}

bool MountExecutor::isSlewingTRK() const
{
    return _readout.isSlewingTRK();
}

bool MountExecutor::isGuiding() const
{
    return _readout.isGuiding();
}

bool MountExecutor::isFindingHome() const
{
    return _readout.isFindingHome();
}

bool MountExecutor::getStepperProgress(int &raPercentage, int &decPercentage)
{
    return _readout.getStepperProgress(raPercentage, decPercentage);
}

ports::InfoDisplayActivity MountExecutor::activity()
{
    return _readout.activity();
}

String MountExecutor::getStatusStateString()
{
    return _readout.getStatusStateString();
}

const char *MountExecutor::getStatusString()
{
    return _readout.getStatusString();
}

const char *MountExecutor::getStepperInfo()
{
    return _readout.getStepperInfo();
}

const char *MountExecutor::getMountHardwareInfo()
{
    return _readout.getMountHardwareInfo();
}

const char *MountExecutor::getAutoHomingStates() const
{
    return _readout.getAutoHomingStates();
}

}  // namespace app
