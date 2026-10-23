#include "../Configuration.hpp"
#include "Utility.hpp"
#include "EPROMStore.hpp"
#include "LcdMenu.hpp"
#include "HallSensorHoming.hpp"
#include "EndSwitches.hpp"
#include "Mount.hpp"
#include "Sidereal.hpp"
#include "core/CalendarMath.hpp"
#include "core/CoordinateFormatter.hpp"
#include "core/CoordinateMath.hpp"
#include "core/RaTrackingLimit.hpp"
#include "core/SlewProgressReport.hpp"
#include "libs/MappedDict/MappedDict.hpp"

#include "adapters/GpioEndSwitch.hpp"
#include "adapters/HallHomingSensor.hpp"
#include "adapters/NullMotorDriver.hpp"
#include "adapters/StepperAxis.hpp"
#include "adapters/Tmc2209Driver.hpp"

#include "hal/arduino/ArduinoGpioPin.hpp"

#ifdef ARDUINO_AVR_ATmega2560
    #include "hal/avr/InterruptStepperFactory.hpp"
#else
    #include "hal/arduino/AccelStepperMotor.hpp"
#endif

namespace
{
/// Stands in for a driver that has no registers to talk to (A4988, or a TMC2209 strapped to
/// standalone), so every `_driverX` is non-null and its call sites stay unguarded.
adapters::NullMotorDriver nullDriver;

/// Wraps a motion engine in its axis adapter. Both objects live for the lifetime of
/// the program, exactly like the concrete steppers they replace.
ports::IStepperAxis *makeAxis(hal::IStepperMotor &motor)
{
    return new adapters::StepperAxis(motor);
}

/// One input pin as a domain switch. The pin object lives for the lifetime of the program,
/// like the engines above; polarity is the adapter's business.
hal::PinLevel toPinLevel(int activeState)
{
    return (activeState == LOW) ? hal::PinLevel::Low : hal::PinLevel::High;
}

#if (USE_RA_END_SWITCH == 1) || (USE_DEC_END_SWITCH == 1)
ports::IEndSwitch &makeEndSwitch(uint8_t pin, int activeState)
{
    hal::ArduinoGpioPin *gpio = new hal::ArduinoGpioPin(pin);
    gpio->configure(hal::PinMode::InputPullup);
    return *new adapters::GpioEndSwitch(*gpio, toPinLevel(activeState));
}
#endif

#if (USE_HALL_SENSOR_RA_AUTOHOME == 1) || (USE_HALL_SENSOR_DEC_AUTOHOME == 1)
ports::IHomingSensor &makeHomingSensor(uint8_t pin, int activeState)
{
    hal::ArduinoGpioPin *gpio = new hal::ArduinoGpioPin(pin);
    gpio->configure(hal::PinMode::InputPullup);
    return *new adapters::HallHomingSensor(*gpio, toPinLevel(activeState));
}
#endif
}  // namespace

PUSH_NO_WARNINGS
#include <AccelStepper.h>

#if (RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART) || (DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)                                          \
    || (AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART) || (ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)                                       \
    || (FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
    #include <TMCStepper.h>  // If you get an error here, download the TMCstepper library from "Tools > Manage Libraries"
#endif
POP_NO_WARNINGS

// slewStatus()
#define SLEWING_DEC      B00000010
#define SLEWING_RA       B00000001
#define SLEWING_BOTH     B00000011
#define SLEWING_TRACKING B00001000
#define NOT_SLEWING      B00000000
#define SLEW_MASK_ANY    B1111

#define UART_CONNECTION_TEST_RETRIES 5

const char *formatStringsDEC[] = {
    "",
    " {d}@ {m}' {s}\"",  // LCD Menu w/ cursor
    "{d}*{m}'{s}#",      // Meade
    "{d} {m}'{s}\"",     // Print
    "{d}@{m}'{s}\"",     // LCD display only
    "{d}{m}{s}",         // Compact
};

const char *formatStringsRA[] = {
    "",
    " %02dh %02dm %02ds",  // LCD Menu w/ cursor
    "%02d:%02d:%02d#",     // Meade
    "%02dh %02dm %02ds",   // Print
    "%02dh%02dm%02ds",     // LCD display only
    "%02d%02d%02d",        // Compact
};

const float siderealDegreesInHour = 14.95904348958;

/////////////////////////////////
//
// CTOR
//
/////////////////////////////////
Mount::Mount(LcdMenu *lcdMenu, const MountDeps &deps, const core::MountConfig &config)
    : _deps(deps), _config(config),
      // Tracking runs at the finer microstep rate while the steps-per-degree value is
      // the slew-mode one, hence the integer ratio (as the inline expression had it).
      _coordinator(deps.clock, SIDEREAL_SECONDS_PER_DAY, config.trackingMicrostepRatio()), _stepsPerAZDegree(AZIMUTH_STEPS_PER_REV / 360),
      _stepsPerALTDegree(ALTITUDE_STEPS_PER_REV / 360)

{
    _commandReceived = 0;

    _lcdMenu = lcdMenu;
    initializeVariables();
}

void Mount::initializeVariables()
{
    // We are now defaulting to northern hemisphere at 45deg. Switching is now supported
    // at runtime when the Latitude is received via Meade command.
    inNorthernHemisphere = NORTHERN_HEMISPHERE == 1;

    _stepsPerRADegree  = _config.defaultRaStepsPerDegree;   // u-steps per degree when slewing
    _stepsPerDECDegree = _config.defaultDecStepsPerDegree;  // u-steps per degree when slewing

    _coordinator.state().reset(0);
    _coordinator.slew().noteRunning(false);

    // Created by the configure* methods below; null until then.
    _stepperRA    = nullptr;
    _stepperTRK   = nullptr;
    _stepperDEC   = nullptr;
    _stepperGUIDE = nullptr;
    _coordinator.tracking().detach();
    _stepperAZ    = nullptr;
    _stepperALT   = nullptr;
    _stepperFocus = nullptr;
    _coordinator.focus().detach();

    // Replaced by configure*Driver() when the build actually has UART drivers.
    _driverRA    = &nullDriver;
    _driverDEC   = &nullDriver;
    _driverAZ    = &nullDriver;
    _driverALT   = &nullDriver;
    _driverFocus = &nullDriver;

    _latitude   = Latitude(inNorthernHemisphere ? 45.0f : -45.0f);
    _longitude  = Longitude(100.0);
    _zeroPosDEC = 0.0f;

    _coordinator.slew().clearTotalMove();
    _moveRate                = 4;
    _backlashCorrectionSteps = 0;
    _coordinator.slew().clearBacklashCorrection();
    _coordinator.slew().end();
    _coordinator.parking().clear();
    applyParkingStatus();
    _decLowerLimit = 0;
    _decUpperLimit = 0;

#if USE_GYRO_LEVEL == 1
    _pitchCalibrationAngle = 0;
    _rollCalibrationAngle  = 0;
#endif

    _localUtcOffset          = 0;
    _localStartDate.year     = 2021;
    _localStartDate.month    = 1;
    _localStartDate.day      = 1;
    _localStartTimeSetMillis = -1;
    _lastTRKCheck            = 0;
}

/////////////////////////////////
//
// clearConfiguration
//
/////////////////////////////////
void Mount::clearConfiguration()
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
void Mount::readConfiguration()
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
void Mount::readPersistentData()
{
    // EEPROMStore will always return valid data, even if no data is present in the store
    int16_t lastFlashed = _deps.settings->getLastFlashedVersion();

    // Calculate this running firmwares version.
    String sVersion = String(VERSION);
    int firstDot    = sVersion.indexOf(".");
    int secondDot   = sVersion.indexOf(".", firstDot + 1);
    int16_t version = 10000 * sVersion.substring(1, firstDot).toInt();
    version += 100 * sVersion.substring(firstDot + 1, secondDot).toInt();
    version += sVersion.substring(secondDot + 1).toInt();

    if (lastFlashed != version)
    {
        OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: New Flash detected! Flashed from %d to %d.", lastFlashed, version);
        // Write upgrade code here if needed. lastFlashed is 0 if we have never flashed V1.14.x and beyond
        _deps.settings->storeLastFlashedVersion(version);
        if (lastFlashed < 11307)
        {
            OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: First time post 1.13.6, setting AZ and ALT home to 0");
            // Introduced these two in 1.13.7
            _deps.settings->storeALTPosition(0);
            _deps.settings->storeAZPosition(0);
        }
    }
    else
    {
        OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: Same firmware version as last boot %d.", version);
    }

    _stepsPerRADegree = _deps.settings->getRAStepsPerDegree();
    OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: RA steps/deg is %f", _stepsPerRADegree);

    _stepsPerDECDegree = _deps.settings->getDECStepsPerDegree();
    OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: DEC steps/deg is %f", _stepsPerDECDegree);

    if (kHasAzStepper)
    {
        _stepsPerAZDegree = _deps.settings->getAZStepsPerDegree();
        OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: AZ steps/deg is %f", _stepsPerAZDegree);
    }

    if (kHasAltStepper)
    {
        _stepsPerALTDegree = _deps.settings->getALTStepsPerDegree();
        OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: ALT steps/deg is %f", _stepsPerALTDegree);
    }
    float speed = _deps.settings->getSpeedFactor();
    OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: Speed factor is %f", speed);
    setSpeedCalibration(speed, false);

    _backlashCorrectionSteps = _deps.settings->getBacklashCorrectionSteps();
    OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: Backlash correction is %d", _backlashCorrectionSteps);

    _latitude = _deps.settings->getLatitude();
    OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: Latitude is %s", _latitude.ToString());

    _longitude = _deps.settings->getLongitude();
    OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: Longitude is %s", _longitude.ToString());

    _localUtcOffset = _deps.settings->getUtcOffset();
    OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: UTC offset is %d", _localUtcOffset);

#if USE_GYRO_LEVEL == 1
    _pitchCalibrationAngle = _deps.settings->getPitchCalibrationAngle();
    OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: Pitch Offset is %f", _pitchCalibrationAngle);

    _rollCalibrationAngle = _deps.settings->getRollCalibrationAngle();
    OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: Roll Offset is %f", _rollCalibrationAngle);
#endif

    _decLowerLimit = static_cast<long>(-(_deps.settings->getDECLowerLimit() * _stepsPerDECDegree));
    if (_decLowerLimit == 0 && DEC_LIMIT_DOWN != 0)
    {
        _decLowerLimit = static_cast<long>(-(DEC_LIMIT_DOWN * _stepsPerDECDegree));
    }
    _decUpperLimit = static_cast<long>((_deps.settings->getDECUpperLimit() * _stepsPerDECDegree));
    if (_decUpperLimit == 0 && DEC_LIMIT_UP != 0)
    {
        _decUpperLimit = static_cast<long>(DEC_LIMIT_UP * _stepsPerDECDegree);
    }
    OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: DEC limits read as %ld -> %ld", _decLowerLimit, _decUpperLimit);

    if (kHasAltStepper)
    {
        long altPos = _deps.settings->getALTPosition();
        _stepperALT->setCurrentPosition(altPos);
    }

    if (kHasAzStepper)
    {
        long azPos = _deps.settings->getAZPosition();
        _stepperAZ->setCurrentPosition(azPos);
    }

    configureHemisphere(_latitude.getTotalHours() > 0);
}

/////////////////////////////////
//
// configureHemisphere
//
/////////////////////////////////
void Mount::configureHemisphere(bool inNorthern, bool force)
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
        _stepperRA->setPinsInverted(invertDir, false, false);
        _stepperTRK->setPinsInverted(invertDir, false, false);

        OAT_LOG(_deps.logger, DEBUG_ANY, "[SYSTEM]: Reset RA and TRK positions to 0");
        _stepperTRK->setCurrentPosition(0);
        _stepperRA->setCurrentPosition(0);
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
// configureRAStepper
//
/////////////////////////////////
void Mount::configureRAStepper(byte pin1, byte pin2, uint32_t maxSpeed, uint32_t maxAcceleration)
{
#ifdef ARDUINO_AVR_ATmega2560
    _stepperRA  = makeAxis(*hal::avr::makeRaSlewMotor(AccelStepper::DRIVER, pin1, pin2));
    _stepperTRK = makeAxis(*hal::avr::makeRaTrkMotor(AccelStepper::DRIVER, pin1, pin2));
#else
    _stepperRA = makeAxis(*new hal::AccelStepperMotor(*new AccelStepper(AccelStepper::DRIVER, pin1, pin2)));

    // Use another axis instance to run the RA motor as well. This one tracks earths rotation.
    _stepperTRK = makeAxis(*new hal::AccelStepperMotor(*new AccelStepper(AccelStepper::DRIVER, pin1, pin2)));
#endif

    _stepperRA->setMaxSpeed(maxSpeed);
    _stepperRA->setAcceleration(maxAcceleration);
    _maxRASpeed        = maxSpeed;
    _maxRAAcceleration = maxAcceleration;

    _stepperTRK->setMaxSpeed(5000);
    _stepperTRK->setAcceleration(15000);

    _coordinator.tracking().attach(_stepperTRK);
}

/////////////////////////////////
//
// configureDECStepper
//
/////////////////////////////////
void Mount::configureDECStepper(byte pin1, byte pin2, uint32_t maxSpeed, uint32_t maxAcceleration)
{
#ifdef ARDUINO_AVR_ATmega2560
    _stepperDEC   = makeAxis(*hal::avr::makeDecSlewMotor(AccelStepper::DRIVER, pin1, pin2));
    _stepperGUIDE = makeAxis(*hal::avr::makeDecTrkMotor(AccelStepper::DRIVER, pin1, pin2));
#else
    _stepperDEC = makeAxis(*new hal::AccelStepperMotor(*new AccelStepper(AccelStepper::DRIVER, pin1, pin2)));

    // Use another axis instance to run the DEC motor as well. This one is used for guiding.
    _stepperGUIDE = makeAxis(*new hal::AccelStepperMotor(*new AccelStepper(AccelStepper::DRIVER, pin1, pin2)));
#endif
    _stepperDEC->setMaxSpeed(maxSpeed);
    _stepperDEC->setAcceleration(maxAcceleration);
    _maxDECSpeed        = maxSpeed;
    _maxDECAcceleration = maxAcceleration;

    _stepperGUIDE->setMaxSpeed(maxSpeed);
    _stepperGUIDE->setAcceleration(maxAcceleration);

#if DEC_INVERT_DIR == 1
    _stepperDEC->setPinsInverted(true, false, false);
    _stepperGUIDE->setPinsInverted(true, false, false);
#endif
    _stepperGUIDE->setCurrentPosition(0);
}

/////////////////////////////////
//
// configureAZStepper / configureALTStepper
//
/////////////////////////////////
#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
void Mount::configureAZStepper(byte pin1, byte pin2, int maxSpeed, int maxAcceleration)
{
    #ifdef ARDUINO_AVR_ATmega2560
    _stepperAZ = makeAxis(*hal::avr::makeAzSlewMotor(AccelStepper::DRIVER, pin1, pin2));
    #else
    _stepperAZ    = makeAxis(*new hal::AccelStepperMotor(*new AccelStepper(AccelStepper::DRIVER, pin1, pin2)));
    #endif
    _stepperAZ->setMaxSpeed(maxSpeed);
    _stepperAZ->setAcceleration(maxAcceleration);
    _maxAZSpeed        = maxSpeed;
    _maxAZAcceleration = maxAcceleration;
    #if AZ_INVERT_DIR == 1
    _stepperAZ->setPinsInverted(true, false, false);
    #endif
}
#endif

#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
void Mount::configureALTStepper(byte pin1, byte pin2, int maxSpeed, int maxAcceleration)
{
    #ifdef ARDUINO_AVR_ATmega2560
    _stepperALT = makeAxis(*hal::avr::makeAltSlewMotor(AccelStepper::DRIVER, pin1, pin2));
    #else
    _stepperALT   = makeAxis(*new hal::AccelStepperMotor(*new AccelStepper(AccelStepper::DRIVER, pin1, pin2)));
    #endif
    _stepperALT->setMaxSpeed(maxSpeed);
    _stepperALT->setAcceleration(maxAcceleration);
    _maxALTSpeed        = maxSpeed;
    _maxALTAcceleration = maxAcceleration;
    #if ALT_INVERT_DIR == 1
    _stepperALT->setPinsInverted(true, false, false);
    #endif
}
#endif

/////////////////////////////////
//
// configureFocusStepper
//
/////////////////////////////////
#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
void Mount::configureFocusStepper(byte pin1, byte pin2, int maxSpeed, int maxAcceleration)
{
    #ifdef ARDUINO_AVR_ATmega2560
    _stepperFocus = makeAxis(*hal::avr::makeFocusSlewMotor(AccelStepper::DRIVER, pin1, pin2));
    #else
    _stepperFocus = makeAxis(*new hal::AccelStepperMotor(*new AccelStepper(AccelStepper::DRIVER, pin1, pin2)));
    #endif
    _stepperFocus->setMaxSpeed(maxSpeed);
    _stepperFocus->setAcceleration(maxAcceleration);
    _stepperFocus->setSpeed(0);
    _stepperFocus->setCurrentPosition(50000);
    _maxFocusSpeed        = maxSpeed;
    _maxFocusAcceleration = maxAcceleration;
    _coordinator.focus().attach(_stepperFocus);
    _coordinator.focus().initialize(maxSpeed);
}
#endif

#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART || DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART                                              \
    || AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART || ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART                                           \
    || FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    #if (UART_CONNECTION_TEST_TXRX == 1) || (TEST_VERIFY_MODE == 1)
bool Mount::connectToDriver(const String &driverKind, uint16_t *rmsCurrent)
{
    MappedDict<String, ports::IMotorDriver *>::DictEntry_t lookupTable[] = {
        #if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
        {"RA", _driverRA},
        #endif
        #if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
        {"DEC", _driverDEC},
        #endif
        #if ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
        {"ALT", _driverALT},
        #endif
        #if AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
        {"AZ", _driverAZ},
        #endif
        #if FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
        {"FOC", _driverFocus},
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

/////////////////////////////////
//
// configureRAdriver
// TMC2209 UART only
/////////////////////////////////
#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    #if SW_SERIAL_UART == 0
void Mount::configureRAdriver(Stream *serial, float rsense, byte driveraddress, int rmscurrent, int stallvalue)
{
    _driverRA = new adapters::Tmc2209Driver(serial, rsense, driveraddress);
    _driverRA->begin();
    bool UART_Rx_connected = false;
        #if UART_CONNECTION_TEST_TXRX == 1
    UART_Rx_connected = connectToDriver("RA");
    if (!UART_Rx_connected)
    {
        // Disable motor for safety reasons if UART connection fails to avoid operating at incorrect rms_current
        _deps.raEnablePin->write(hal::PinLevel::High);
    }
        #endif
    _driverRA->toff(0);
        #if USE_VREF == 0  //By default, Vref is ignored when using UART to specify rms current.
    _driverRA->I_scale_analog(false);
        #endif
    OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[MOUNT]: Requested RA motor rms_current: %d mA", rmscurrent);
    _driverRA->rms_current(rmscurrent, 1.0f);  //holdMultiplier = 1 to set ihold = irun
    _driverRA->pdn_disable(1);
    _driverRA->toff(adapters::kTmc2209Toff);
    _driverRA->en_spreadCycle(RA_UART_STEALTH_MODE == 0);
    _driverRA->blank_time(24);
    _driverRA->microsteps(core::microstepRegisterValue(_config.raTrackingMicrosteps()));  // System starts in tracking mode
    _driverRA->fclktrim(4);
    _driverRA->TCOOLTHRS(0xFFFFF);  //xFFFFF);
    _driverRA->semin(0);            //disable CoolStep so that current is consistent
    _driverRA->SGTHRS(stallvalue);
    if (UART_Rx_connected)
    {
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[MOUNT]: Actual RA motor rms_current: %d mA", _driverRA->rms_current());
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[MOUNT]: Actual RA CS value: %d", _driverRA->cs_actual());
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[MOUNT]: Actual RA vsense: %d", _driverRA->vsense());
    }
}

    #elif SW_SERIAL_UART == 1

void Mount::configureRAdriver(uint16_t RA_SW_RX, uint16_t RA_SW_TX, float rsense, byte driveraddress, int rmscurrent, int stallvalue)
{
    _driverRA = new adapters::Tmc2209Driver(RA_SW_RX, RA_SW_TX, rsense, driveraddress);
    _driverRA->beginSerial(19200);
    _driverRA->mstep_reg_select(true);
    _driverRA->pdn_disable(true);
    bool UART_Rx_connected = false;
        #if UART_CONNECTION_TEST_TXRX == 1
    UART_Rx_connected = connectToDriver("RA");
    if (!UART_Rx_connected)
    {
        // Disable motor for safety reasons if UART connection fails to avoid operating at incorrect rms_current
        _deps.raEnablePin->write(hal::PinLevel::High);
    }
        #endif
    _driverRA->toff(0);
        #if USE_VREF == 0  //By default, Vref is ignored when using UART to specify rms current.
    _driverRA->I_scale_analog(false);
        #endif
    OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[MOUNT]: Requested RA motor rms_current: %d mA", rmscurrent);
    _driverRA->rms_current(rmscurrent, 1.0f);  //holdMultiplier = 1 to set ihold = irun
    _driverRA->toff(adapters::kTmc2209Toff);
    _driverRA->en_spreadCycle(RA_UART_STEALTH_MODE == 0);
    _driverRA->blank_time(24);
    _driverRA->semin(0);                                                                  //disable CoolStep so that current is consistent
    _driverRA->microsteps(core::microstepRegisterValue(_config.raTrackingMicrosteps()));  // System starts in tracking mode
    _driverRA->fclktrim(4);
    _driverRA->TCOOLTHRS(0xFFFFF);  //xFFFFF);
    _driverRA->SGTHRS(stallvalue);
    if (UART_Rx_connected)
    {
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[MOUNT]: Actual RA motor rms_current: %d mA", _driverRA->rms_current());
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[MOUNT]: Actual RA CS value: %d", _driverRA->cs_actual());
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[MOUNT]: Actual RA vsense: %d", _driverRA->vsense());
    }
}
    #endif
#endif

/////////////////////////////////
//
// configureDECdriver
// TMC2209 UART only
/////////////////////////////////
#if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    #if SW_SERIAL_UART == 0
void Mount::configureDECdriver(Stream *serial, float rsense, byte driveraddress, int rmscurrent, int stallvalue)
{
    _driverDEC = new adapters::Tmc2209Driver(serial, rsense, driveraddress);
    _driverDEC->begin();
    bool UART_Rx_connected = false;
        #if UART_CONNECTION_TEST_TXRX == 1
    UART_Rx_connected = connectToDriver("DEC");
    if (!UART_Rx_connected)
    {
        // Disable motor for safety reasons if UART connection fails to avoid operating at incorrect rms_current
        _deps.decEnablePin->write(hal::PinLevel::High);
    }
        #endif
    _driverDEC->toff(0);
        #if USE_VREF == 0  //By default, Vref is ignored when using UART to specify rms current.
    _driverDEC->I_scale_analog(false);
        #endif
    OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[MOUNT]: Requested DEC motor rms_current: %d mA", rmscurrent);
    _driverDEC->rms_current(rmscurrent, 1.0f);  //holdMultiplier = 1 to set ihold = irun
    _driverDEC->pdn_disable(1);
    _driverDEC->iholddelay(15);
    _driverDEC->TPOWERDOWN(255);
    _driverDEC->toff(adapters::kTmc2209Toff);
    _driverDEC->en_spreadCycle(DEC_UART_STEALTH_MODE == 0);
    _driverDEC->blank_time(24);
    _driverDEC->microsteps(
        core::microstepRegisterValue(_config.decGuidingMicrosteps()));  // If 1 then disable microstepping. Start with Guide microsteps.
    _driverDEC->TCOOLTHRS(0xFFFFF);
    _driverDEC->semin(0);  //disable CoolStep so that current is consistent
    _driverDEC->SGTHRS(stallvalue);
    if (UART_Rx_connected)
    {
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[MOUNT]: Actual DEC motor rms_current: %d mA", _driverDEC->rms_current());
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[MOUNT]: Actual DEC CS value: %d", _driverDEC->cs_actual());
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[MOUNT]: Actual DEC vsense: %d", _driverDEC->vsense());
    }
}

    #elif SW_SERIAL_UART == 1

void Mount::configureDECdriver(uint16_t DEC_SW_RX, uint16_t DEC_SW_TX, float rsense, byte driveraddress, int rmscurrent, int stallvalue)
{
    _driverDEC = new adapters::Tmc2209Driver(DEC_SW_RX, DEC_SW_TX, rsense, driveraddress);
    _driverDEC->beginSerial(19200);
    _driverDEC->mstep_reg_select(true);
    _driverDEC->pdn_disable(true);
    bool UART_Rx_connected = false;
        #if UART_CONNECTION_TEST_TXRX == 1
    UART_Rx_connected = connectToDriver("DEC");
    if (!UART_Rx_connected)
    {
        // Disable motor for safety reasons if UART connection fails to avoid operating at incorrect rms_current
        _deps.decEnablePin->write(hal::PinLevel::High);
    }
        #endif
    _driverDEC->toff(0);
        #if USE_VREF == 0  //By default, Vref is ignored when using UART to specify rms current.
    _driverDEC->I_scale_analog(false);
        #endif
    OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[MOUNT]: Requested DEC motor rms_current: %d mA", rmscurrent);
    _driverDEC->rms_current(rmscurrent, 1.0f);  //holdMultiplier = 1 to set ihold = irun
    _driverDEC->toff(adapters::kTmc2209Toff);
    _driverDEC->en_spreadCycle(DEC_UART_STEALTH_MODE == 0);
    _driverDEC->blank_time(24);
    _driverDEC->microsteps(
        core::microstepRegisterValue(_config.decGuidingMicrosteps()));  // If 1 then disable microstepping. Start with Guide microsteps
    _driverDEC->TCOOLTHRS(0xFFFFF);
    _driverDEC->semin(0);  //disable CoolStep so that current is consistent
    _driverDEC->SGTHRS(stallvalue);
    if (UART_Rx_connected)
    {
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[MOUNT]: Actual DEC motor rms_current: %d mA", _driverDEC->rms_current());
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[MOUNT]: Actual DEC CS value: %d", _driverDEC->cs_actual());
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[MOUNT]: Actual DEC vsense: %d", _driverDEC->vsense());
    }
}
    #endif
#endif

/////////////////////////////////
//
// configureAZdriver
// TMC2209 UART only
/////////////////////////////////
#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE) && (AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
    #if SW_SERIAL_UART == 0
void Mount::configureAZdriver(Stream *serial, float rsense, byte driveraddress, int rmscurrent, int stallvalue)
{
    _driverAZ = new adapters::Tmc2209Driver(serial, rsense, driveraddress);
    _driverAZ->begin();
    bool UART_Rx_connected = false;
        #if UART_CONNECTION_TEST_TXRX == 1
    UART_Rx_connected = connectToDriver("AZ");
    if (!UART_Rx_connected)
    {
        // Disable motor for safety reasons if UART connection fails to avoid operating at incorrect rms_current
        _deps.azEnablePin->write(hal::PinLevel::High);
    }
        #endif
    _driverAZ->toff(0);
        #if USE_VREF == 0  //By default, Vref is ignored when using UART to specify rms current.
    _driverAZ->I_scale_analog(false);
        #endif
    OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[MOUNT]: Requested AZ motor rms_current: %d mA", rmscurrent);
    _driverAZ->rms_current(rmscurrent, AZ_MOTOR_HOLD_SETTING / 100.0);  //holdMultiplier = 1 to set ihold = irun
    _driverAZ->toff(adapters::kTmc2209Toff);
    _driverAZ->en_spreadCycle(0);
    _driverAZ->blank_time(24);
    _driverAZ->microsteps(core::microstepRegisterValue(AZ_MICROSTEPPING));  // If 1 then disable microstepping
    _driverAZ->TCOOLTHRS(0xFFFFF);                                          //xFFFFF);
    _driverAZ->semin(0);                                                    //disable CoolStep so that current is consistent
    _driverAZ->SGTHRS(stallvalue);
    if (UART_Rx_connected)
    {
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[MOUNT]: Actual AZ motor rms_current: %d mA", _driverAZ->rms_current());
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[MOUNT]: Actual AZ CS value: %d", _driverAZ->cs_actual());
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[MOUNT]: Actual AZ vsense: %d", _driverAZ->vsense());
    }
}

    #elif SW_SERIAL_UART == 1

void Mount::configureAZdriver(uint16_t AZ_SW_RX, uint16_t AZ_SW_TX, float rsense, byte driveraddress, int rmscurrent, int stallvalue)
{
    _driverAZ = new adapters::Tmc2209Driver(AZ_SW_RX, AZ_SW_TX, rsense, driveraddress);
    _driverAZ->beginSerial(19200);
    _driverAZ->mstep_reg_select(true);
    _driverAZ->pdn_disable(true);
    bool UART_Rx_connected = false;
        #if UART_CONNECTION_TEST_TXRX == 1
    UART_Rx_connected = connectToDriver("AZ");
    if (!UART_Rx_connected)
    {
        // Disable motor for safety reasons if UART connection fails to avoid operating at incorrect rms_current
        _deps.azEnablePin->write(hal::PinLevel::High);
    }
        #endif
    _driverAZ->toff(0);
        #if USE_VREF == 0  //By default, Vref is ignored when using UART to specify rms current.
    _driverAZ->I_scale_analog(false);
        #endif
    OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[MOUNT]: Requested AZ motor rms_current: %d mA", rmscurrent);
    _driverAZ->rms_current(rmscurrent, AZ_MOTOR_HOLD_SETTING / 100.0);  //holdMultiplier = 1 to set ihold = irun
    _driverAZ->toff(adapters::kTmc2209Toff);
    _driverAZ->en_spreadCycle(0);
    _driverAZ->blank_time(24);
    _driverAZ->microsteps(core::microstepRegisterValue(AZ_MICROSTEPPING));  // If 1 then disable microstepping
    _driverAZ->TCOOLTHRS(0xFFFFF);                                          //xFFFFF);
    _driverAZ->semin(0);                                                    //disable CoolStep so that current is consistent
    _driverAZ->SGTHRS(stallvalue);
    if (UART_Rx_connected)
    {
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[MOUNT]: Actual AZ motor rms_current: %d mA", _driverAZ->rms_current());
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[MOUNT]: Actual AZ CS value: %d", _driverAZ->cs_actual());
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[MOUNT]: Actual AZ vsense: %d", _driverAZ->vsense());
    }
}
    #endif
#endif

/////////////////////////////////
//
// configureALTdriver
// TMC2209 UART only
/////////////////////////////////
#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE) && (ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
    #if SW_SERIAL_UART == 0
void Mount::configureALTdriver(Stream *serial, float rsense, byte driveraddress, int rmscurrent, int stallvalue)
{
    _driverALT = new adapters::Tmc2209Driver(serial, rsense, driveraddress);
    _driverALT->begin();
    bool UART_Rx_connected = false;
        #if UART_CONNECTION_TEST_TXRX == 1
    UART_Rx_connected = connectToDriver("ALT");
    if (!UART_Rx_connected)
    {
        // Disable motor for safety reasons if UART connection fails to avoid operating at incorrect rms_current
        _deps.altEnablePin->write(hal::PinLevel::High);
    }
        #endif
    _driverALT->toff(0);
        #if USE_VREF == 0  //By default, Vref is ignored when using UART to specify rms current.
    _driverALT->I_scale_analog(false);
        #endif
    OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[MOUNT]: Requested ALT motor rms_current: %d mA", rmscurrent);
    _driverALT->rms_current(rmscurrent, ALT_MOTOR_HOLD_SETTING / 100.0);  //holdMultiplier = 1 to set ihold = irun
    _driverALT->toff(adapters::kTmc2209Toff);
    _driverALT->en_spreadCycle(0);
    _driverALT->blank_time(24);
    _driverALT->microsteps(core::microstepRegisterValue(ALT_MICROSTEPPING));  // If 1 then disable microstepping
    _driverALT->TCOOLTHRS(0xFFFFF);                                           //xFFFFF);
    _driverALT->semin(0);                                                     //disable CoolStep so that current is consistent
    _driverALT->SGTHRS(stallvalue);
    if (UART_Rx_connected)
    {
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[MOUNT]: Actual ALT motor rms_current: %d mA", _driverALT->rms_current());
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[MOUNT]: Actual ALT CS value: %d", _driverALT->cs_actual());
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[MOUNT]: Actual ALT vsense: %d", _driverALT->vsense());
    }
}

    #elif SW_SERIAL_UART == 1

void Mount::configureALTdriver(uint16_t ALT_SW_RX, uint16_t ALT_SW_TX, float rsense, byte driveraddress, int rmscurrent, int stallvalue)
{
    _driverALT = new adapters::Tmc2209Driver(ALT_SW_RX, ALT_SW_TX, rsense, driveraddress);
    _driverALT->beginSerial(19200);
    _driverALT->mstep_reg_select(true);
    _driverALT->pdn_disable(true);
        #if UART_CONNECTION_TEST_TXRX == 1
    bool UART_Rx_connected = false;
    UART_Rx_connected = connectToDriver("ALT");
    if (!UART_Rx_connected)
    {
        // Disable motor for safety reasons if UART connection fails to avoid operating at incorrect rms_current
        _deps.altEnablePin->write(hal::PinLevel::High);
    }
        #endif
    _driverALT->toff(0);
        #if USE_VREF == 0  //By default, Vref is ignored when using UART to specify rms current.
    _driverALT->I_scale_analog(false);
        #endif
    OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[MOUNT]: Requested ALT motor rms_current: %d mA", rmscurrent);
    _driverALT->rms_current(rmscurrent, ALT_MOTOR_HOLD_SETTING / 100.0);  //holdMultiplier = 1 to set ihold = irun
    _driverALT->toff(adapters::kTmc2209Toff);
    _driverALT->en_spreadCycle(0);
    _driverALT->blank_time(24);
    _driverALT->microsteps(core::microstepRegisterValue(ALT_MICROSTEPPING));  // If 1 then disable microstepping
    _driverALT->TCOOLTHRS(0xFFFFF);                                           //xFFFFF);
    _driverALT->semin(0);                                                     //disable CoolStep so that current is consistent
    _driverALT->SGTHRS(stallvalue);
        #if UART_CONNECTION_TEST_TXRX == 1
    if (UART_Rx_connected)
    {
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[MOUNT]: Actual ALT motor rms_current: %d mA", _driverALT->rms_current());
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[MOUNT]: Actual ALT CS value: %d", _driverALT->cs_actual());
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[MOUNT]: Actual ALT vsense: %d", _driverALT->vsense());
    }
        #endif
}
    #endif
#endif

/////////////////////////////////
//
// configureFocusdriver
// TMC2209 UART only
/////////////////////////////////
#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE) && (FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
    #if SW_SERIAL_UART == 0
void Mount::configureFocusDriver(Stream *serial, float rsense, byte driveraddress, int rmscurrent, int stallvalue)
{
    _driverFocus = new adapters::Tmc2209Driver(serial, rsense, driveraddress);
    _driverFocus->begin();
        #if UART_CONNECTION_TEST_TXRX == 1
    bool UART_Rx_connected = false;
    UART_Rx_connected      = connectToDriver("FOC");
    if (!UART_Rx_connected)
    {
        // Disable motor for safety reasons if UART connection fails to avoid operating at incorrect rms_current
        _deps.focusEnablePin->write(hal::PinLevel::High);
    }
        #endif
    _driverFocus->toff(0);
        #if USE_VREF == 0  //By default, Vref is ignored when using UART to specify rms current.
    _driverFocus->I_scale_analog(false);
        #endif
    OAT_LOG(_deps.logger, DEBUG_STEPPERS | DEBUG_FOCUS, "[FOCUS]: Requested Focus motor rms_current: %d mA", rmscurrent);
    _driverFocus->rms_current(rmscurrent, FOCUSER_MOTOR_HOLD_SETTING / 100.f);  //holdMultiplier = 1 to set ihold = irun
    _driverFocus->toff(adapters::kTmc2209Toff);
    _driverFocus->en_spreadCycle(FOCUS_UART_STEALTH_MODE == 0);
    _driverFocus->blank_time(24);
    _driverFocus->microsteps(core::microstepRegisterValue(FOCUS_MICROSTEPPING));  // If 1 then disable microstepping
    _driverFocus->TCOOLTHRS(0xFFFFF);                                             //xFFFFF);
    _driverFocus->semin(0);                                                       //disable CoolStep so that current is consistent
    _driverFocus->SGTHRS(stallvalue);
        #if UART_CONNECTION_TEST_TXRX == 1
    if (UART_Rx_connected)
    {
        OAT_LOG(_deps.logger, DEBUG_STEPPERS | DEBUG_FOCUS, "[FOCUS]: Actual Focus motor rms_current: %d mA", _driverFocus->rms_current());
        OAT_LOG(_deps.logger, DEBUG_STEPPERS | DEBUG_FOCUS, "[FOCUS]: Actual Focus CS value: %d", _driverFocus->cs_actual());
        OAT_LOG(_deps.logger, DEBUG_STEPPERS | DEBUG_FOCUS, "[FOCUS]: Actual Focus vsense: %d", _driverFocus->vsense());
    }
        #endif

        #if FOCUSER_ALWAYS_ON == 1
    OAT_LOG(_deps.logger, DEBUG_FOCUS, "[FOCUS]: Always on -> TMC2209U enabling driver pin.");
    _deps.focusEnablePin->write(hal::PinLevel::Low);  // Logic LOW to enable driver
        #endif
}

    #elif SW_SERIAL_UART == 1

void Mount::configureFocusDriver(
    uint16_t FOCUS_SW_RX, uint16_t FOCUS_SW_TX, float rsense, byte driveraddress, int rmscurrent, int stallvalue)
{
    _driverFocus = new adapters::Tmc2209Driver(FOCUS_SW_RX, FOCUS_SW_TX, rsense, driveraddress);
    _driverFocus->beginSerial(19200);
    _driverFocus->mstep_reg_select(true);
    _driverFocus->pdn_disable(true);
        #if UART_CONNECTION_TEST_TXRX == 1
    bool UART_Rx_connected = false;
    UART_Rx_connected = connectToDriver("FOC");
    if (!UART_Rx_connected)
    {
        // Disable motor for safety reasons if UART connection fails to avoid operating at incorrect rms_current
        _deps.focusEnablePin->write(hal::PinLevel::High);
    }
        #endif
    _driverFocus->toff(0);
        #if USE_VREF == 0  //By default, Vref is ignored when using UART to specify rms current.
    _driverFocus->I_scale_analog(false);
        #endif
    OAT_LOG(_deps.logger, DEBUG_STEPPERS | DEBUG_FOCUS, "[FOCUS]: Requested Focus motor rms_current: %d mA", rmscurrent);
    _driverFocus->rms_current(rmscurrent, FOCUSER_MOTOR_HOLD_SETTING / 100.f);  //holdMultiplier = 1 to set ihold = irun
    _driverFocus->toff(adapters::kTmc2209Toff);
    _driverFocus->en_spreadCycle(FOCUS_UART_STEALTH_MODE == 0);
    _driverFocus->blank_time(24);
    _driverFocus->microsteps(core::microstepRegisterValue(FOCUS_MICROSTEPPING));  // If 1 then disable microstepping
    _driverFocus->TCOOLTHRS(0xFFFFF);                                             //xFFFFF);
    _driverFocus->semin(0);                                                       //disable CoolStep so that current is consistent
    _driverFocus->SGTHRS(stallvalue);
        #if UART_CONNECTION_TEST_TXRX == 1
    if (UART_Rx_connected)
    {
        OAT_LOG(_deps.logger, DEBUG_STEPPERS | DEBUG_FOCUS, "[FOCUS]: Actual Focus motor rms_current: %d mA", _driverFocus->rms_current());
        OAT_LOG(_deps.logger, DEBUG_STEPPERS | DEBUG_FOCUS, "[FOCUS]: Actual Focus CS value: %d", _driverFocus->cs_actual());
        OAT_LOG(_deps.logger, DEBUG_STEPPERS | DEBUG_FOCUS, "[FOCUS]: Actual Focus vsense: %d", _driverFocus->vsense());
    }
        #endif
        #if FOCUSER_ALWAYS_ON == 1
    OAT_LOG(_deps.logger, DEBUG_FOCUS, "[FOCUS]: Always on -> TMC2209U enabling driver pin.");
    _deps.focusEnablePin->write(hal::PinLevel::Low);  // Logic LOW to enable driver
        #endif
}
    #endif
#endif

/////////////////////////////////
//
// getSpeedCalibration
//
/////////////////////////////////
float Mount::getSpeedCalibration()
{
    return _coordinator.tracking().calibration();
}

/////////////////////////////////
//
// setSpeedCalibration
//
/////////////////////////////////
void Mount::setSpeedCalibration(float val, bool saveToStorage)
{
    OAT_LOG(_deps.logger, DEBUG_MOUNT, "[MOUNT]: Updating speed calibration from %f to %f", _coordinator.tracking().calibration(), val);
    _coordinator.tracking().setCalibration(val);

    OAT_LOG(_deps.logger, DEBUG_MOUNT, "[MOUNT]: Current tracking speed is %f steps/sec", _coordinator.tracking().speed());

    // The tracking rate is derived from the slewing geometry; see core/TrackingController.hpp.
    _coordinator.tracking().setStepsPerDegree(_stepsPerRADegree);
    OAT_LOG(_deps.logger, DEBUG_MOUNT, "[MOUNT]: RA steps per degree is %f steps/deg", _stepsPerRADegree);
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
float Mount::getPitchCalibrationAngle()
{
    return _pitchCalibrationAngle;
}

/////////////////////////////////
//
// setPitchCalibrationAngle
//
/////////////////////////////////
void Mount::setPitchCalibrationAngle(float angle)
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
float Mount::getRollCalibrationAngle()
{
    return _rollCalibrationAngle;
}

/////////////////////////////////
//
// setRollCalibration
//
/////////////////////////////////
void Mount::setRollCalibrationAngle(float angle)
{
    _rollCalibrationAngle = angle;
    _deps.settings->storeRollCalibrationAngle(_rollCalibrationAngle);
}
#endif

/////////////////////////////////
//
// getStepsPerDegree
//
/////////////////////////////////
float Mount::getStepsPerDegree(StepperAxis which)
{
    if (which == RA_STEPS)
    {
        return _stepsPerRADegree;  // u-steps/degree
    }
    if (which == DEC_STEPS)
    {
        return _stepsPerDECDegree;  // u-steps/degree
    }
    if (which == AZIMUTH_STEPS)
    {
        return kHasAzStepper ? _stepsPerAZDegree : 1;  // u-steps/degree
    }
    if (which == ALTITUDE_STEPS)
    {
        return kHasAltStepper ? _stepsPerALTDegree : 1;  // u-steps/degree
    }

    return 0;
}

/////////////////////////////////
//
// setStepsPerDegree
//
/////////////////////////////////
// Function to set steps per degree for each axis. This function stores the value in persistent storage.
void Mount::setStepsPerDegree(StepperAxis which, float steps)
{
    if (which == DEC_STEPS)
    {
        _stepsPerDECDegree = steps;
        _deps.settings->storeDECStepsPerDegree(_stepsPerDECDegree);
    }
    else if (which == RA_STEPS)
    {
        _stepsPerRADegree = steps;
        _deps.settings->storeRAStepsPerDegree(_stepsPerRADegree);
        setSpeedCalibration(_coordinator.tracking().calibration(), false);
    }
    else if (kHasAzStepper && which == AZIMUTH_STEPS)
    {
        _stepsPerAZDegree = steps;
        _deps.settings->storeAZStepsPerDegree(_stepsPerAZDegree);
    }
    else if (kHasAltStepper && which == ALTITUDE_STEPS)
    {
        _stepsPerALTDegree = steps;
        _deps.settings->storeALTStepsPerDegree(_stepsPerALTDegree);
    }
}

/////////////////////////////////
//
// getBacklashCorrection
//
/////////////////////////////////
int Mount::getBacklashCorrection()
{
    return _backlashCorrectionSteps;
}

/////////////////////////////////
//
// getMountHardwareInfo
//
/////////////////////////////////
const char *Mount::getStepperInfo()
{
    String ret = "";

#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    ret += "TU";
#elif RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE
    ret += "TS";
#elif RA_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC
    ret += "A";
#else
    ret += "?";
#endif

    ret += ",";
    ret += String(_config.raSlewMicrosteps);
    ret += ",";
    ret += String(_config.raTrackingMicrosteps());

    ret += "|";

#if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    ret += "TU";
#elif DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE
    ret += "TS";
#elif DEC_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC
    ret += "A";
#else
    ret += "?";
#endif

    ret += ",";
    ret += String(_config.decSlewMicrosteps);
    ret += ",";
    ret += String(_config.decGuidingMicrosteps());

    ret += "|";

    _readoutScratch = ret;
    return _readoutScratch.c_str();
}

/////////////////////////////////
//
// getMountHardwareInfo
//
/////////////////////////////////
const char *Mount::getMountHardwareInfo()
{
    String ret = F("Unknown,");
#if defined(ESP32)
    ret = F("ESP32,");
#elif defined(__AVR_ATmega2560__)
    ret = F("Mega,");
#elif defined(ARDUINO_ARCH_RP2040)
    ret = F("RP2040,");
#endif

    ret += F("NEMA|");

    ret += String(RA_PULLEY_TEETH) + "|";
    ret += String(RA_STEPPER_SPR) + ",";

    ret += F("NEMA|");

    ret += String(DEC_PULLEY_TEETH) + "|";
    ret += String(DEC_STEPPER_SPR) + ",";

#if USE_GPS == 1
    ret += F("GPS,");
#else
    ret += F("NO_GPS,");
#endif

#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE) && (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    ret += F("AUTO_AZ_ALT,");
#elif (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    ret += F("AUTO_AZ,");
#elif (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    ret += F("AUTO_ALT,");
#else
    ret += F("NO_AZ_ALT,");
#endif

#if USE_GYRO_LEVEL == 1
    ret += F("GYRO,");
#else
    ret += F("NO_GYRO,");
#endif

#if DISPLAY_TYPE == DISPLAY_TYPE_NONE
    ret += F("NO_LCD,");
#elif DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD
    ret += F("LCD_KEYPAD,");
#elif DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23008
    ret += F("LCD_I2C_MCP23008,");
#elif DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23017
    ret += F("LCD_I2C_MCP23017,");
#elif DISPLAY_TYPE == DISPLAY_TYPE_LCD_JOY_I2C_SSD1306
    ret += F("LCD_JOY_I2C_SSD1306,");
#endif

#if INFO_DISPLAY_TYPE == DISPLAY_TYPE_NONE
    ret += F("NO_INFO_DISP,");
#elif INFO_DISPLAY_TYPE == INFO_DISPLAY_TYPE_I2C_SSD1306_128x64
    ret += F("INFO_I2C_SSD1306_128x64,");
    // To add new display types, format this string in the same way: INFO_<interface>_<chip>_<resolution>
#else
    ret += F("INFO_UNKNOWN,");
#endif

#if FOCUS_STEPPER_TYPE == STEPPER_TYPE_NONE
    ret += F("NO_FOC,");
#else
    ret += F("FOC,");
#endif

#if USE_HALL_SENSOR_RA_AUTOHOME == 1
    ret += F("HSAH,");
#else
    ret += F("NO_HSAH,");
#endif
#if USE_HALL_SENSOR_DEC_AUTOHOME == 1
    ret += F("HSAV,");
#else
    ret += F("NO_HSAV,");
#endif

#if (USE_RA_END_SWITCH == 1) || (USE_DEC_END_SWITCH == 1)
    ret += F("ENDSW");
    #if (USE_RA_END_SWITCH == 1)
    ret += F("_RA");
    #endif
    #if (USE_DEC_END_SWITCH == 1)
    ret += F("_DEC");
    #endif
    ret += F(",");
#else
    ret += F("NO_ENDSW,");
#endif

    _readoutScratch = ret;
    return _readoutScratch.c_str();
}

/////////////////////////////////
//
// setBacklashCorrection
//
/////////////////////////////////
// Function to set steps per degree for each axis. This function stores the value in persistent storage.
void Mount::setBacklashCorrection(int steps)
{
    _backlashCorrectionSteps = steps;
    _deps.settings->storeBacklashCorrectionSteps(_backlashCorrectionSteps);
}

/////////////////////////////////
//
// setSlewRate
//
/////////////////////////////////
void Mount::setSlewRate(int rate)
{
    _moveRate           = clamp(rate, 1, 4);
    float speedFactor[] = {0, 0.05, 0.2, 0.5, 1.0};
    OAT_LOG(_deps.logger, DEBUG_MOUNT, "[MOUNT]: setSlewRate, rate is %d -> %f", _moveRate, speedFactor[_moveRate]);
    _stepperDEC->setMaxSpeed(speedFactor[_moveRate] * _maxDECSpeed);
    _stepperRA->setMaxSpeed(speedFactor[_moveRate] * _maxRASpeed);
    OAT_LOG(
        _deps.logger, DEBUG_MOUNT, "[MOUNT]: setSlewRate, new speeds are RA: %f  DEC: %f", _stepperRA->maxSpeed(), _stepperDEC->maxSpeed());
}

/////////////////////////////////
//
// setSlewRate
//
/////////////////////////////////
int Mount::getSlewRate()
{
    return _moveRate;
}

/////////////////////////////////
//
// setHA
//
/////////////////////////////////
void Mount::setHA(const core::DayTime &haTime)
{
    OAT_LOG(_deps.logger, DEBUG_MOUNT, "[MOUNT]: setHA:  HA is %s", DayTime(haTime).ToString());
    DayTime lst = DayTime(POLARIS_RA_HOUR, POLARIS_RA_MINUTE, POLARIS_RA_SECOND);
    lst.addTime(haTime);
    setLST(lst);
}

/////////////////////////////////
//
// HA
//
/////////////////////////////////
const DayTime Mount::HA() const
{
    // LOG(DEBUG_MOUNT_VERBOSE, "[MOUNT]: Get HA.");
    // LOG(DEBUG_MOUNT_VERBOSE, "[MOUNT]: Polaris adjust: %s", DayTime(POLARIS_RA_HOUR, POLARIS_RA_MINUTE, POLARIS_RA_SECOND).ToString());
    DayTime ha = _LST;
    // LOG(DEBUG_MOUNT_VERBOSE, "[MOUNT]: LST: %s", _LST.ToString());
    ha.subtractTime(DayTime(POLARIS_RA_HOUR, POLARIS_RA_MINUTE, POLARIS_RA_SECOND));
    // LOG(DEBUG_MOUNT, "[MOUNT]: GetHA: LST-Polaris is HA %s", ha.ToString());
    return ha;
}

/////////////////////////////////
//
// setLST
//
/////////////////////////////////
void Mount::setLST(const core::DayTime &lst)
{
    _LST       = lst;
    _zeroPosRA = lst;
#if defined(OAM) || defined(OAE)
    _zeroPosRA.addHours(6);  // shift allcoordinates by 90° for EQ mount movement
#endif
    OAT_LOG(_deps.logger, DEBUG_MOUNT, "[MOUNT]: Set LST and ZeroPosRA to: %s", _LST.ToString());
}

/////////////////////////////////
//
// setLatitude
//
/////////////////////////////////
void Mount::setLatitude(const core::Latitude &latitude)
{
    OAT_LOG(_deps.logger, DEBUG_GENERAL, "[MOUNT]: Setting longitude to %fs", latitude.getTotalHours());
    _latitude = latitude;
    configureHemisphere(_latitude.getTotalHours() > 0);
    _deps.settings->storeLatitude(_latitude);
}

/////////////////////////////////
//
// setLongitude
//
/////////////////////////////////
void Mount::setLongitude(const core::Longitude &longitude)
{
    OAT_LOG(_deps.logger, DEBUG_GENERAL, "[MOUNT]: Setting longitude to %fs", longitude.getTotalHours());
    _longitude = longitude;
    _deps.settings->storeLongitude(_longitude);

    autoCalcHa();
}

/////////////////////////////////
//
// latitude
//
/////////////////////////////////
core::Latitude Mount::latitude() const
{
    return _latitude;
}
/////////////////////////////////
//
// longitude
//
/////////////////////////////////
core::Longitude Mount::longitude() const
{
    return _longitude;
}

/////////////////////////////////
//
// targetRA
//
/////////////////////////////////
// Get a reference to the target RA value.
core::DayTime &Mount::targetRA()
{
    return _targetRA;
}

/////////////////////////////////
//
// targetDEC
//
/////////////////////////////////
// Get a reference to the target DEC value.
core::Declination &Mount::targetDEC()
{
    return _targetDEC;
}

/////////////////////////////////
//
// currentRA
//
/////////////////////////////////
// Get current RA value.
core::DayTime Mount::currentRA() const
{
    // How many steps moves the RA ring one sidereal hour along. One sidereal hour moves just shy of 15 degrees
    float stepsPerSiderealHour = _stepsPerRADegree * siderealDegreesInHour;              // u-steps/degree * degrees/hr = u-steps/hr
    float hourPos              = -_stepperRA->currentPosition() / stepsPerSiderealHour;  // u-steps / u-steps/hr = hr

    hourPos += _zeroPosRA.getTotalHours();

    const float degreePos = (_stepperDEC->currentPosition() / _stepsPerDECDegree) + _zeroPosDEC;
    if (degreePos < 0)
    {
        hourPos += 12;
        if (hourPos > 24)
            hourPos -= 24;
    }

    // Make sure we are normalized
    if (hourPos < 0)
        hourPos += 24;
    if (hourPos > 24)
        hourPos -= 24;

    return hourPos;
}

/////////////////////////////////
//
// currentDEC
//
/////////////////////////////////
// Get current DEC value.
core::Declination Mount::currentDEC() const
{
    // u-steps / u-steps/deg = deg
    const float degreePos = (_stepperDEC->currentPosition() / _stepsPerDECDegree) + _zeroPosDEC;  // u-steps / u-steps/deg = deg
    Declination dec(degreePos);

    return dec;
}

/////////////////////////////////
//
// syncPosition
//
/////////////////////////////////
// Set the current RA and DEC position to be the given coordinate. We do this by setting the stepper motor coordinate
// to be at the calculated positions (that they would be if we were slewing there).
void Mount::syncPosition(const core::DayTime &ra, const core::Declination &dec)
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
            _zeroPosRA.getTotalHours(),
            _zeroPosDEC);

    // Adjust the home RA position by the delta sync position.
    float raAdjust = ra.getTotalHours() - currentRA().getTotalHours();
    OAT_LOG(_deps.logger,
            DEBUG_COORD_CALC,
            "[MOUNT]: syncPosition: AdjustRA is %f  (%f - %f)",
            raAdjust,
            ra.getTotalHours(),
            currentRA().getTotalHours());
    while (raAdjust > 12)
    {
        raAdjust -= 24;
    }
    while (raAdjust < -12)
    {
        raAdjust += 24;
    }
    OAT_LOG(_deps.logger, DEBUG_COORD_CALC, "[MOUNT]: syncPosition: AdjustRA is %f", raAdjust);
    _zeroPosRA.addHours(raAdjust);
    OAT_LOG(_deps.logger, DEBUG_COORD_CALC, "[MOUNT]: syncPosition: ZeroPosRA is now %f", _zeroPosRA.getTotalHours());

    // Adjust the home DEC position by the delta between the sync'd target and current position.
    const float degreePos = (_stepperDEC->currentPosition() / _stepsPerDECDegree) + _zeroPosDEC;  // u-steps / u-steps/deg = deg
    OAT_LOG(_deps.logger, DEBUG_COORD_CALC, "[MOUNT]: syncPosition: DEC degreePos is: %f", degreePos);

    // Dec totalhours can be plus or minus the distance from the pole (because it keeps track of whether we are upwards or downwards from the pole)
    // So we use the abs of both values to find their difference
    float decAdjust = fabsf(dec.getTotalDegrees()) - fabsf(currentDEC().getTotalDegrees());
    OAT_LOG(_deps.logger,
            DEBUG_COORD_CALC,
            "[MOUNT]: syncPosition: DecAdjust is: %f ( |%f| - |%f| )",
            decAdjust,
            dec.getTotalDegrees(),
            currentDEC().getTotalDegrees());
    if (degreePos < 0)
    {
        OAT_LOG(_deps.logger, DEBUG_COORD_CALC, "[MOUNT]: syncPosition: Inverted DecAdjust to: %f (below home pos)", decAdjust);
        decAdjust = -decAdjust;
    }

    _zeroPosDEC += decAdjust;
    OAT_LOG(_deps.logger, DEBUG_COORD_CALC, "[MOUNT]: syncPosition: _zeroPosDEC adjusted by: %f", decAdjust);
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
void Mount::startSlewingToTarget()
{
    if (!admit(core::MountRequest::GoTo))
    {
        // A go-to used to reprogram both axes underneath a park sequence that was still
        // homing. PLAN.md lists this refusal as the one intentional behaviour change.
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: startSlewingToTarget: refused while parking");
        return;
    }

    stopGuiding();

    // Make sure we're slewing at full speed on a GoTo
    OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: startSlewingToTarget: Set DEC to MaxSpeed(%ld)", _maxDECSpeed);
    _stepperDEC->setMaxSpeed(_maxDECSpeed);
    OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: startSlewingToTarget: Set RA  to MaxSpeed(%ld)", _maxRASpeed);
    _stepperRA->setMaxSpeed(_maxRASpeed);

    // Calculate new RA stepper target (and DEC). We are never in guiding mode here.
    _currentRAStepperPosition = _stepperRA->currentPosition();
    long targetRAPosition, targetDECPosition;
    calculateRAandDECSteppers(targetRAPosition, targetDECPosition);

    if (targetRAPosition != _stepperRA->currentPosition())
    {
        // Only stop tracking if we're tracking and actually going to slew somewhere else, otherwise the
        // mount::loop() code won't detect the end of the slewing operation...
        if (isSlewingTRK())
        {
            OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[MOUNT]: Stop tracking (NEMA steppers)");
            stopSlewing(TRACKING);
            _coordinator.tracking().noteStopped(_deps.clock->now());
        }

        // Set RA to slew microsteps for TMC2209 UART once the TRK stepper has stopped
        setRaMicrosteps(_config.raSlewMicrosteps);

        OAT_LOG(
            _deps.logger, DEBUG_STEPPERS, "[STEPPERS]: startSlewingToTarget: TRK stopped at %ldms", _coordinator.tracking().stoppedAt());
    }

    _coordinator.slew().begin(core::SlewIntent::Target);
    setStatusFlag(STATUS_SLEWING | STATUS_SLEWING_TO_TARGET);
    publishEvent(core::MountEventType::SlewStarted);
    // Normal state for DEC is guide microstepping, so switch to slew microstepping here.
    setDecMicrosteps(_config.decSlewMicrosteps);
    _coordinator.slew().noteRunning(true);
    moveSteppersTo(targetRAPosition, targetDECPosition, RA_AND_DEC_STEPS);  // u-steps (in slew mode)
    _coordinator.slew().setTotalMove(static_cast<float>(_stepperRA->distanceToGo()), static_cast<float>(_stepperDEC->distanceToGo()));
    OAT_LOG(_deps.logger, DEBUG_MOUNT, "[MOUNT]: RA Dist: %ld,   DEC Dist: %ld", _stepperRA->distanceToGo(), _stepperDEC->distanceToGo());
}

/////////////////////////////////
//
// startSlewingToHome
//
/////////////////////////////////
// Calculates movement parameters and program steppers to move to home position.
// Takes any sync operations that have happened and tracking into account.
void Mount::startSlewingToHome()
{
    stopGuiding();

    // Make sure we're slewing at full speed on a GoTo
    OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: startSlewingToHome: Set DEC to MaxSpeed(%ld)", _maxDECSpeed);
    _stepperDEC->setMaxSpeed(_maxDECSpeed);
    OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: startSlewingToHome: Set RA  to MaxSpeed(%ld)", _maxRASpeed);
    _stepperRA->setMaxSpeed(_maxRASpeed);

    _currentRAStepperPosition = _stepperRA->currentPosition();

    long targetRAPosition        = 0;
    const long targetDECPosition = 0;

    _coordinator.slew().begin(core::SlewIntent::Home);
    // Take tracking into account
    const long trackingOffset = _stepperTRK->currentPosition() * _config.raSlewMicrosteps / _config.raTrackingMicrosteps();
    targetRAPosition -= trackingOffset;
    OAT_LOG(_deps.logger,
            DEBUG_STEPPERS,
            "[STEPPERS]: startSlewingToHome: Adjusted with tracking distance: %ld (adjusted for MS: %ld), result: %ld",
            _stepperTRK->currentPosition(),
            trackingOffset,
            targetRAPosition);

    long raStepsToGo = targetRAPosition - _stepperRA->currentPosition();
    if (raStepsToGo != 0)
    {
        // Only stop tracking if we're actually going to slew somewhere else, otherwise the
        // mount::loop() code won't detect the end of the slewing operation...
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[MOUNT]: Stop tracking (NEMA steppers)");
        stopSlewing(TRACKING);
        _coordinator.tracking().noteStopped(_deps.clock->now());

        // set Slew microsteps for TMC2209 UART once the TRK stepper has stopped
        setRaMicrosteps(_config.raSlewMicrosteps);

        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: startSlewingToHome: TRK stopped at %ldms", _coordinator.tracking().stoppedAt());
    }

    setStatusFlag(STATUS_SLEWING | STATUS_SLEWING_TO_TARGET);
    publishEvent(core::MountEventType::SlewStarted);
    // Normal state for DEC is guide microstepping, so switch to slew microstepping here.
    setDecMicrosteps(_config.decSlewMicrosteps);
    _coordinator.slew().noteRunning(true);
    moveSteppersTo(targetRAPosition, targetDECPosition, RA_AND_DEC_STEPS);  // u-steps (in slew mode)
    _coordinator.slew().setTotalMove(static_cast<float>(_stepperRA->distanceToGo()), static_cast<float>(_stepperDEC->distanceToGo()));
    OAT_LOG(_deps.logger, DEBUG_MOUNT, "[MOUNT]: RA Dist: %ld,   DEC Dist: %ld", _stepperRA->distanceToGo(), _stepperDEC->distanceToGo());
}

/////////////////////////////////
//
// stopGuiding
//
/////////////////////////////////
void Mount::stopGuiding(bool ra, bool dec)
{
    if (!isGuiding())
        return;

    // Stop RA guide first, since it's just a speed change back to tracking speed
    if (ra && _coordinator.state().test(STATUS_GUIDE_PULSE_RA))
    {
        OAT_LOG(
            _deps.logger, DEBUG_STEPPERS | DEBUG_GUIDE, "[GUIDE]: stopGuide:    TRK stop guide at  : %ld", _stepperTRK->currentPosition());
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
                _stepperGUIDE->currentPosition());

        // Stop DEC guiding and wait for it to stop.
        _stepperGUIDE->stop();

#if !defined(ESP32BOARD)
        while (_stepperGUIDE->isRunning())
        {
            _stepperGUIDE->run();
            _stepperTRK->runSpeed();
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
void Mount::guidePulse(byte direction, int duration)
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
                    _stepperGUIDE->currentPosition());
            OAT_LOG(_deps.logger, DEBUG_STEPPERS | DEBUG_GUIDE, "[GUIDE]: guidePulse:   DEC steps/deg       : %f", _stepsPerDECDegree);
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
                    _stepperTRK->currentPosition());
            OAT_LOG(_deps.logger, DEBUG_STEPPERS | DEBUG_GUIDE, "[GUIDE]: guidePulse:   RA  steps/deg       : %f", _stepsPerRADegree);
            OAT_LOG(_deps.logger,
                    DEBUG_STEPPERS | DEBUG_GUIDE,
                    "[GUIDE]: guidePulse:   RA  Microstep ratio : %f",
                    (1.0 * _config.raTrackingMicrosteps() / _config.raSlewMicrosteps));
        }

        // DEC stepper moves at sidereal rate in both directions
        // RA stepper moves at either 2.5x sidereal rate or 0.5x sidereal rate.
        // Also compensate for microstepping mode change between slew & guiding/tracking
        const core::GuideAxisGeometry raGeometry
            = {_stepsPerRADegree, _config.raTrackingMicrosteps(), _config.raSlewMicrosteps, RA_PULSE_MULTIPLIER};
        const core::GuideAxisGeometry decGeometry
            = {_stepsPerDECDegree, _config.decGuidingMicrosteps(), _config.decSlewMicrosteps, DEC_PULSE_MULTIPLIER};

        // TODO: Do we need to track how many steps the steppers took and add them to the GoHome calculation?
        // If so, we need to remember where we were when we started the guide pulse. Then at the end,
        // we can calculate the difference. Ignore DEC Guide for now.
        // TODO: Take guide pulses on DEC into account

        switch (direction)
        {
            case NORTH:
                {
                    // If a upper limit is set, check we're not there. If we are, refuse the guide pulse.
                    const core::DecPulse pulse = core::GuidingController::decPulse(
                        true, decGeometry, siderealDegreesInHour, _stepperDEC->currentPosition(), _decLowerLimit, _decUpperLimit);
                    if (!pulse.allowed)
                    {
                        // TODO: Make sure Southern hemisphere does not need a sign/direction inversion.
                        OAT_LOG(_deps.logger,
                                DEBUG_STEPPERS | DEBUG_GUIDE,
                                "[GUIDE]: guidePulse:   DEC is at (%ld) which is at or above upper limit (%ld), ignoring guide pulse",
                                _stepperDEC->currentPosition(),
                                _decUpperLimit);
                    }
                    else
                    {
                        OAT_LOG(
                            _deps.logger, DEBUG_STEPPERS | DEBUG_GUIDE, "[GUIDE]: guidePulse:   DEC base speed      : %f", pulse.baseSpeed);
                        OAT_LOG(_deps.logger,
                                DEBUG_STEPPERS | DEBUG_GUIDE,
                                "[GUIDE]: guidePulse:   DEC guide speed     : %f",
                                pulse.targetSpeed);
                        _stepperGUIDE->setSpeed(pulse.targetSpeed);
                        setStatusFlag(STATUS_GUIDE_PULSE | STATUS_GUIDE_PULSE_DEC);
                        _coordinator.guiding().startDec(duration);
                    }
                }
                break;

            case SOUTH:
                {
                    const core::DecPulse pulse = core::GuidingController::decPulse(
                        false, decGeometry, siderealDegreesInHour, _stepperDEC->currentPosition(), _decLowerLimit, _decUpperLimit);
                    if (!pulse.allowed)
                    {
                        OAT_LOG(_deps.logger,
                                DEBUG_STEPPERS | DEBUG_GUIDE,
                                "[GUIDE]: guidePulse:   DEC is at (%ld) which is at or below lower limit (%ld), ignoring guide pulse",
                                _stepperDEC->currentPosition(),
                                _decLowerLimit);
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
                        _stepperGUIDE->setSpeed(pulse.targetSpeed);
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
                    _stepperTRK->setSpeed(pulse.targetSpeed);  // Faster than siderael
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
                    _stepperTRK->setSpeed(pulse.targetSpeed);  // Slower than siderael
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
void Mount::commandReceived()
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
void Mount::startDriftAlignment(int durationSecs)
{
    _coordinator.drift().begin(durationSecs, _stepsPerRADegree);
}

void Mount::updateDriftAlignment(unsigned long now)
{
    if (!_coordinator.drift().isActive())
    {
        return;
    }

    const bool raRunning  = _stepperRA->isRunning();
    const bool decRunning = _stepperDEC->isRunning();

    switch (_coordinator.drift().update(!raRunning && !decRunning, now))
    {
        case core::DriftAction::None:
            break;

        case core::DriftAction::StopAxes:
            stopSlewing(ALL_DIRECTIONS | TRACKING);
            break;

        case core::DriftAction::MoveEastPass:
            _stepperRA->setMaxSpeed(_coordinator.drift().passSpeed());
            _stepperRA->move(_coordinator.drift().passSteps());
            break;

        case core::DriftAction::MoveEastGap:
            _stepperRA->setMaxSpeed(core::DriftAlignmentController::kGapSpeedStepsSec);
            _stepperRA->move(-core::DriftAlignmentController::kGearingGapSteps);
            break;

        case core::DriftAction::MoveWestPass:
            _stepperRA->setMaxSpeed(_coordinator.drift().passSpeed());
            _stepperRA->move(-_coordinator.drift().passSteps());
            break;

        case core::DriftAction::ResetGap:
            _stepperRA->setMaxSpeed(core::DriftAlignmentController::kGapSpeedStepsSec);
            _stepperRA->move(core::DriftAlignmentController::kGearingGapSteps);
            break;

        case core::DriftAction::ResumeTracking:
            // Put the stepper back into its configured slew parameters before tracking resumes.
            _stepperRA->setMaxSpeed(_maxRASpeed);
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
void Mount::setManualSlewMode(bool state)
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
                _maxRASpeed,
                _maxRAAcceleration);
        OAT_LOG(_deps.logger,
                DEBUG_STEPPERS,
                "[STEPPERS]: setManualSlewMode: Set DEC speed/accel:  %ld  / %ld",
                _maxDECSpeed,
                _maxDECAcceleration);
        _stepperRA->setAcceleration(_maxRAAcceleration);
        _stepperRA->setMaxSpeed(_maxRASpeed);
        _stepperDEC->setMaxSpeed(_maxDECSpeed);
        _stepperDEC->setAcceleration(_maxDECAcceleration);
        startSlewing(TRACKING);
    }
}

/////////////////////////////////
//
// setSpeed
//
/////////////////////////////////
void Mount::setSpeed(StepperAxis which, float speedDegsPerSec)
{
    if (which == RA_STEPS)
    {
        float stepsPerSec = speedDegsPerSec * _stepsPerRADegree;  // deg/sec * u-steps/deg = u-steps/sec
        OAT_LOG(_deps.logger,
                DEBUG_STEPPERS,
                "[STEPPERS]: setSpeed: Set RA speed %f degs/s, which is %f steps/s",
                speedDegsPerSec,
                stepsPerSec);
        // TODO: Are we already in slew mode?
        _stepperRA->setSpeed(stepsPerSec);
    }
    else if (which == DEC_STEPS)
    {
        float stepsPerSec = speedDegsPerSec * _stepsPerDECDegree;  // deg/sec * u-steps/deg = u-steps/sec
        OAT_LOG(_deps.logger,
                DEBUG_STEPPERS,
                "[STEPPERS]: setSpeed: Set DEC speed %f degs/s, which is %f steps/s",
                speedDegsPerSec,
                stepsPerSec);
        // TODO: Are we already in slew mode?
        _stepperDEC->setSpeed(stepsPerSec);
    }
    else if (kHasAzStepper && which == AZIMUTH_STEPS)
    {
        float stepsPerSec = speedDegsPerSec * _stepsPerAZDegree;  // deg/sec * u-steps/deg = u-steps/sec
        OAT_LOG(_deps.logger,
                DEBUG_STEPPERS,
                "[STEPPERS]: setSpeed: Set AZ speed %f degs/s, which is %f steps/s",
                speedDegsPerSec,
                stepsPerSec);
        _stepperAZ->setSpeed(stepsPerSec);
    }
    else if (kHasAltStepper && which == ALTITUDE_STEPS)
    {
        float stepsPerSec = speedDegsPerSec * _stepsPerALTDegree;  // deg/sec * u-steps/deg = u-steps/sec
        OAT_LOG(_deps.logger,
                DEBUG_STEPPERS,
                "[STEPPERS]: setSpeed: Set ALT speed %f degs/s, which is %f steps/s",
                speedDegsPerSec,
                stepsPerSec);
        _stepperALT->setSpeed(stepsPerSec);
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
            _stepperFocus->stop();
        }
    }
}

void Mount::getAZALTPositions(long &azPos, long &altPos)
{
    azPos  = kHasAzStepper ? _stepperAZ->currentPosition() : 0;
    altPos = kHasAltStepper ? _stepperALT->currentPosition() : 0;
}

void Mount::moveAZALTToHome()
{
    if (kHasAzStepper)
    {
        enableAzMotor();
        _stepperAZ->moveTo(0);
    }
    if (kHasAltStepper)
    {
        enableAltMotor();
        _stepperALT->moveTo(0);
    }
}

void Mount::setAZALTHome()
{
    if (kHasAzStepper)
    {
        _stepperAZ->setCurrentPosition(0);
        _deps.settings->storeAZPosition(0);
    }
    if (kHasAltStepper)
    {
        _stepperALT->setCurrentPosition(0);
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
void Mount::park()
{
    stopGuiding();
    stopSlewing(ALL_DIRECTIONS | TRACKING);
    waitUntilStopped(ALL_DIRECTIONS);
    _coordinator.parking().beginHoming();
    startSlewingToHome();
    applyParkingStatus();
    publishEvent(core::MountEventType::ParkingStarted);
}

bool Mount::isAxisRunning(StepperAxis axis)
{
    switch (axis)
    {
        case RA_STEPS:
            return _stepperRA->isRunning();
        case DEC_STEPS:
            return _stepperDEC->isRunning();
            if (kHasAltStepper)
            {
                case ALTITUDE_STEPS:
                    return _stepperALT->isRunning();
            }
            if (kHasAzStepper)
            {
                case AZIMUTH_STEPS:
                    return _stepperAZ->isRunning();
            }
            if (kHasFocusStepper)
            {
                case FOCUS_STEPS:
                    return _stepperFocus->isRunning();
            }
        default:
            break;
    }
    return false;
}

/////////////////////////////////
//
// isRunningAZ
//
/////////////////////////////////
bool Mount::isRunningAZ() const
{
    return kHasAzStepper && _stepperAZ->isRunning();
}

/////////////////////////////////
//
// isRunningALT
//
/////////////////////////////////
bool Mount::isRunningALT() const
{
    return kHasAltStepper && _stepperALT->isRunning();
}

/////////////////////////////////
//
// moveBy
//
/////////////////////////////////
void Mount::moveBy(int direction, float arcMinutes)
{
    if (kHasAzStepper && direction == AZIMUTH_STEPS)
    {
        enableAzMotor();
        long stepsToMove = arcMinutes * AZIMUTH_STEPS_PER_ARC_MINUTE;
        _stepperAZ->move(stepsToMove);
    }
    if (kHasAltStepper && direction == ALTITUDE_STEPS)
    {
        enableAltMotor();
        long stepsToMove = arcMinutes * ALTITUDE_STEPS_PER_ARC_MINUTE;
        _stepperALT->move(stepsToMove);
    }
}

/////////////////////////////////
//
// disableAzAltMotors
//
/////////////////////////////////
void Mount::disableAzAltMotors()
{
    if (kHasAzStepper)
    {
        _stepperAZ->stop();
    }
    if (kHasAltStepper)
    {
        _stepperALT->stop();
    }

    if (kHasAzStepper)
    {
        while (_stepperAZ->isRunning())
        {
            loop();
        }
    }

    if (kHasAltStepper)
    {
        while (_stepperALT->isRunning())
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
void Mount::enableAzMotor()
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

void Mount::enableAltMotor()
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
bool Mount::isRunningFocus() const
{
    return _coordinator.focus().isRunning();
}

/////////////////////////////////
//
// getFocusSpeed
//
/////////////////////////////////
float Mount::getFocusSpeed() const
{
    return _coordinator.focus().speed();
}

/////////////////////////////////
//
// focusSetSpeedByRate
//
/////////////////////////////////
void Mount::focusSetSpeedByRate(int rate)
{
    _coordinator.focus().setRate(rate, _maxFocusSpeed);
    OAT_LOG(_deps.logger,
            DEBUG_FOCUS,
            "[FOCUS]: focusSetSpeedByRate: rate is %d, factor %f, maxspeed %f -> %f",
            _coordinator.focus().rate(),
            _coordinator.focus().rateFactor(),
            _maxFocusSpeed,
            _coordinator.focus().maxRateSpeed());

    if (_coordinator.focus().isRunning())
    {
        OAT_LOG(_deps.logger, DEBUG_FOCUS, "[FOCUS]: focusSetSpeedByRate: stepper is already running so should adjust speed?");
        //_stepperFocus->setSpeed(_coordinator.focus().rateFactor() * _maxFocusSpeed);
    }
}

/////////////////////////////////
//
// focusContinuousMove
//
/////////////////////////////////
void Mount::focusContinuousMove(FocuserDirection direction)
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
void Mount::focusMoveBy(long steps)
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
long Mount::focusGetStepperPosition()
{
    return _coordinator.focus().position();
}

/////////////////////////////////
//
// focusSetPosition
//
/////////////////////////////////
void Mount::focusSetStepperPosition(long steps)
{
    _coordinator.focus().setPosition(steps);
}

/////////////////////////////////
//
// disableFocusMotor
//
/////////////////////////////////
void Mount::disableFocusMotor()
{
    OAT_LOG(_deps.logger, DEBUG_FOCUS, "[FOCUS]: disableFocusMotor: stopping motor and waiting.");
    _stepperFocus->stop();
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
void Mount::enableFocusMotor()
{
    OAT_LOG(_deps.logger, DEBUG_FOCUS, "[FOCUS]: enableFocusMotor: enabling driver pin.");
    _deps.focusEnablePin->write(hal::PinLevel::Low);  // Logic LOW to enable driver
}

/////////////////////////////////
//
// focusStop
//
/////////////////////////////////
void Mount::focusStop()
{
    OAT_LOG(_deps.logger, DEBUG_FOCUS, "[FOCUS]: focusStop: stopping motor.");
    _coordinator.focus().stop();
}

/////////////////////////////////
//
// setTrackingStepperPos
//
/////////////////////////////////
void Mount::setTrackingStepperPos(long stepPos)
{
    _stepperTRK->setCurrentPosition(stepPos);
}

void Mount::setEventSink(core::IEventSink *sink)
{
    _coordinator.events().subscribe(sink);
}

void Mount::clearEventSink(core::IEventSink *sink)
{
    _coordinator.events().unsubscribe(sink);
}

void Mount::subscribe(core::IEventSink *sink)
{
    setEventSink(sink);
}

void Mount::unsubscribe(core::IEventSink *sink)
{
    clearEventSink(sink);
}

/// Records a fact for the tick boundary. The next `loop()` delivers it, so a subscriber
/// is never called from inside the code that just changed the mount's state.
void Mount::publishEvent(core::MountEventType type, bool succeeded)
{
    core::MountEvent event;
    event.type      = type;
    event.succeeded = succeeded;
    _coordinator.events().publish(event);
}

void Mount::setStatus(int state)
{
    noInterrupts();
    _coordinator.state().reset((uint16_t) state);
    interrupts();
}

void Mount::setStatusFlag(int flag)
{
    noInterrupts();
    _coordinator.state().set((uint16_t) flag);
    interrupts();
}

void Mount::clearStatusFlag(int flag)
{
    noInterrupts();
    _coordinator.state().clear((uint16_t) flag);
    interrupts();
}

/////////////////////////////////
//
// getStatusString
//
/////////////////////////////////
String Mount::getStatusStateString()
{
    String status;
    if (_coordinator.state().isParked())
    {
        status = F("Parked");
    }
    else if (_coordinator.state().test(STATUS_PARKING) || _coordinator.state().test(STATUS_PARKING_POS))
    {
        status = F("Parking");
    }
    else if (isFindingHome())
    {
        status = F("Homing");
    }
    else if (isGuiding())
    {
        status = F("Guiding");
    }
    else if (slewStatus() & SLEW_MASK_ANY)
    {
        if (_coordinator.state().test(STATUS_SLEWING_TO_TARGET))
        {
            status = F("SlewToTarget");
        }
        else if (_coordinator.state().test(STATUS_SLEWING_FREE))
        {
            status = F("FreeSlew");
        }
        else if (_coordinator.state().test(STATUS_SLEWING_MANUAL))
        {
            status = F("ManualSlew");
        }
        else if (slewStatus() & SLEWING_TRACKING)
        {
            status = F("Tracking");
        }
    }
    else
    {
        status = F("Idle");
    }
    return status;
}
/////////////////////////////////
//
// getStatusString
//
/////////////////////////////////
const char *Mount::getStatusString()
{
    String status = getStatusStateString() + ",";

    String disp = F("------,");
    if (_coordinator.state().test(STATUS_SLEWING))
    {
        byte slew = slewStatus();
        if (slew & SLEWING_RA)
        {
            disp[0] = _stepperRA->targetPosition() < _stepperRA->currentPosition() ? 'R' : 'r';
        }
        if (slew & SLEWING_DEC)
        {
            disp[1] = _stepperDEC->targetPosition() < _stepperDEC->currentPosition() ? 'D' : 'd';
        }
        if (slew & SLEWING_TRACKING)
        {
            disp[2] = 'T';
        }
    }
    else if (isSlewingTRK())
    {
        disp[2] = 'T';
    }
    if (kHasAzStepper && _stepperAZ->isRunning())
    {
        disp[3] = _stepperAZ->targetPosition() < _stepperAZ->currentPosition() ? 'Z' : 'z';
    }
    if (kHasAltStepper && _stepperALT->isRunning())
    {
        disp[4] = _stepperALT->targetPosition() < _stepperALT->currentPosition() ? 'A' : 'a';
    }

    if (kHasFocusStepper && _stepperFocus->isRunning())
        disp[5] = _stepperFocus->targetPosition() < _stepperFocus->currentPosition() ? 'F' : 'f';

    status += disp;
    status += String(_stepperRA->currentPosition()) + ",";
    status += String(_stepperDEC->currentPosition()) + ",";
    status += String(_stepperTRK->currentPosition()) + ",";

    status += RAString(COMPACT_STRING | CURRENT_STRING) + ",";
    status += DECString(COMPACT_STRING | CURRENT_STRING) + ",";
    status += kHasFocusStepper ? String(_stepperFocus->currentPosition()) + "," : String(",");

    _readoutScratch = status;
    return _readoutScratch.c_str();
}

/////////////////////////////////
//
// slewStatus
//
// Returns the current state of the motors and is a bitfield with these flags:
// NOT_SLEWING is all zero. SLEWING_DEC, SLEWING_RA, SLEWING_BOTH, SLEWING_TRACKING are bits.
/////////////////////////////////
byte Mount::slewStatus() const
{
    if (_coordinator.state().isParked())
    {
        return NOT_SLEWING;
    }
    if (isGuiding())
    {
        return NOT_SLEWING;
    }
    byte slewState = _stepperRA->isRunning() ? SLEWING_RA : NOT_SLEWING;
    slewState |= _stepperDEC->isRunning() ? SLEWING_DEC : NOT_SLEWING;

    slewState |= _coordinator.state().test(STATUS_TRACKING) ? SLEWING_TRACKING : NOT_SLEWING;
    return slewState;
}

byte Mount::mountStatus() const
{
    return (byte) _coordinator.state().value();
}

/////////////////////////////////
//
// isGuiding
//
/////////////////////////////////
bool Mount::isGuiding() const
{
    return _coordinator.state().test(STATUS_GUIDE_PULSE);
}

/////////////////////////////////
//
// isSlewingDECorRA
//
/////////////////////////////////
bool Mount::isSlewingRAorDEC() const
{
    if (isParking())
        return true;
    return (slewStatus() & (SLEWING_DEC | SLEWING_RA)) != 0;
}

/////////////////////////////////
//
// isSlewingIdle
//
/////////////////////////////////
bool Mount::isSlewingIdle() const
{
    if (isParking())
        return false;
    return (slewStatus() & (SLEWING_DEC | SLEWING_RA)) == 0;
}

/////////////////////////////////
//
// isSlewingTRK
//
/////////////////////////////////
bool Mount::isSlewingTRK() const
{
    return (slewStatus() & SLEWING_TRACKING) != 0;
}

/////////////////////////////////
//
// isParking
//
/////////////////////////////////
bool Mount::isParking() const
{
    return _coordinator.parking().isParking();
}

void Mount::applyParkingStatus()
{
    clearStatusFlag(STATUS_PARKING | STATUS_PARKING_POS);
    const uint16_t bit = core::ParkingController::statusBitFor(_coordinator.parking().phase());
    if (bit != 0)
    {
        setStatusFlag(bit);
    }
}

/////////////////////////////////
//
// isFindingHome
//
/////////////////////////////////
bool Mount::isFindingHome() const
{
    return _coordinator.state().test(STATUS_FINDING_HOME);
}

core::MountActivityFacts Mount::activityFacts() const
{
    return _coordinator.activityFacts();
}

bool Mount::admit(core::MountRequest request) const
{
    return _coordinator.admit(request) == core::Admission::Allowed;
}

// A board whose driver takes its microsteps from pin strapping holds the null driver, whose
// `microsteps()` does nothing, so these are the same code on every board.
void Mount::setRaMicrosteps(int microsteps)
{
    OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: RA driver setMicrosteps(%d)", microsteps);
    _driverRA->microsteps(core::microstepRegisterValue(microsteps));
}

void Mount::setDecMicrosteps(int microsteps)
{
    OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: DEC driver setMicrosteps(%d)", microsteps);
    _driverDEC->microsteps(core::microstepRegisterValue(microsteps));
}

/////////////////////////////////
//
// startSlewing
//
// Starts manual slewing in one of eight directions or
// tracking, but only if not currently parking!
/////////////////////////////////
void Mount::startSlewing(int direction)
{
    // Parking owns both axes until it finishes; every request in this function is
    // refused while it runs. See core/MountRequests.hpp for the whole table.
    if (admit((direction & TRACKING) ? core::MountRequest::Track : core::MountRequest::ManualSlew))
    {
        stopGuiding();

        if (direction & TRACKING)
        {
            // Start tracking
            setRaMicrosteps(_config.raTrackingMicrosteps());
            _coordinator.tracking().applySpeed();

            // Turn on tracking
            setStatusFlag(STATUS_TRACKING);
            publishEvent(core::MountEventType::TrackingStarted);
        }
        else
        {
            // Start slewing
            int sign = inNorthernHemisphere ? 1 : -1;

            // Set move rate to last commanded slew rate
            setSlewRate(_moveRate);
            if (isSlewingTRK())
            {
                stopSlewing(TRACKING);
                _coordinator.tracking().noteStopped(_deps.clock->now());
                OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: startSlewing: stopped TRK at %ld", _coordinator.tracking().stoppedAt());
            }

            // Change microstep mode for slewing
            setRaMicrosteps(_config.raSlewMicrosteps);
            // Normal state for DEC is guide microstepping, so switch to slew microstepping here.
            setDecMicrosteps(_config.decSlewMicrosteps);

            if (direction & NORTH)
            {
                long targetLocation = _stepsPerDECDegree * DEC_LIMIT_UP;
                if (_decUpperLimit != 0)
                {
                    targetLocation = _decUpperLimit;
                    OAT_LOG(_deps.logger,
                            DEBUG_STEPPERS,
                            "[STEPPERS]: startSlewing(N): DEC has upper limit of %ld. targetMoveTo is now %ld",
                            _decUpperLimit,
                            targetLocation);
                }
                else
                {
                    OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: startSlewing(N): initial targetMoveTo is %ld", targetLocation);
                }

                _stepperDEC->moveTo(targetLocation);
                setStatusFlag(STATUS_SLEWING);
            }

            if (direction & SOUTH)
            {
                long targetLocation = -_stepsPerDECDegree * DEC_LIMIT_DOWN;
                if (_decLowerLimit != 0)
                {
                    targetLocation = _decLowerLimit;
                    OAT_LOG(_deps.logger,
                            DEBUG_STEPPERS,
                            "[STEPPERS]: startSlewing(S): DEC has lower limit of %ld. targetMoveTo is now %ld",
                            _decLowerLimit,
                            targetLocation);
                }
                else
                {
                    OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: startSlewing(S): initial targetMoveTo is %ld", targetLocation);
                }

                _stepperDEC->moveTo(targetLocation);
                setStatusFlag(STATUS_SLEWING);
            }

            const float trackedHours = getTrackedHours();
            if (direction & EAST)
            {
                // We need to subtract the distance tracked from the physical RA home coordinate
                long targetEastPos = _stepsPerRADegree * 15.0 * (RA_PHYSICAL_LIMIT + trackedHours);
                OAT_LOG(_deps.logger,
                        DEBUG_STEPPERS,
                        "[STEPPERS]: startSlewing(E): initial targetMoveTo is %ld (adjusted for %fh tracked)",
                        -sign * targetEastPos,
                        trackedHours);
                _stepperRA->moveTo(-sign * targetEastPos);
                setStatusFlag(STATUS_SLEWING);
            }
            if (direction & WEST)
            {
                // We need to add the distance tracked from the physical RA home coordinate
                long targetWestPos = _stepsPerRADegree * 15.0 * (min(RA_PHYSICAL_LIMIT, RA_TRACKING_LIMIT) - trackedHours);
                OAT_LOG(_deps.logger,
                        DEBUG_STEPPERS,
                        "[STEPPERS]: startSlewing(W): initial targetMoveTo is %ld (adjusted for %fh tracked)",
                        sign * targetWestPos,
                        trackedHours);
                _stepperRA->moveTo(sign * targetWestPos);
                setStatusFlag(STATUS_SLEWING);
            }
        }
    }
}

/////////////////////////////////
//
// stopSlewing
//
// Stop manual slewing in one of two directions or Tracking. NS is the same. EW is the same
/////////////////////////////////
void Mount::stopSlewing(int direction)
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
        _stepperDEC->stop();
    }

    if ((direction & (WEST | EAST)) != 0)
    {
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: stopSlewing: RA stepper stop()");
        _stepperRA->stop();
        if (isFindingHome())
        {
            clearStatusFlag(STATUS_FINDING_HOME);
        }
    }
}

void Mount::stopSlewing(StepperAxis axis)
{
    if (axis == RA_STEPS)
    {
        _stepperRA->stop();
    }
    else if (axis == DEC_STEPS)
    {
        _stepperDEC->stop();
    }
    else if (axis == AZIMUTH_STEPS)
    {
        if (kHasAzStepper)
        {
            _stepperAZ->stop();
        }
    }
    else if (axis == ALTITUDE_STEPS)
    {
        if (kHasAltStepper)
        {
            _stepperALT->stop();
        }
    }
    else if (axis == FOCUS_STEPS)
    {
        if (kHasFocusStepper)
        {
            _stepperFocus->stop();
        }
    }
}

/////////////////////////////////
//
// waitUntilStopped
//
/////////////////////////////////
// Block until the RA and DEC motors are stopped
void Mount::waitUntilStopped(byte direction)
{
    while (((direction & (EAST | WEST)) && _stepperRA->isRunning()) || ((direction & (NORTH | SOUTH)) && _stepperDEC->isRunning())
           || ((direction & TRACKING) && ((!_coordinator.state().test(STATUS_TRACKING)) && _stepperTRK->isRunning()))
           || ((direction & FOCUSING) && kHasFocusStepper && _stepperFocus->isRunning()))
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
void Mount::waitUntilAllStopped()
{
    while (_stepperRA->isRunning() || _stepperDEC->isRunning()
           || ((!_coordinator.state().test(STATUS_TRACKING)) && _stepperTRK->isRunning())
           || (kHasFocusStepper && _stepperFocus->isRunning()) || (kHasAzStepper && _stepperAZ->isRunning())
           || (kHasAltStepper && _stepperALT->isRunning()))
    {
        loop();
        yield();
    }
}

/////////////////////////////////
//
// getCurrentStepperPosition
//
/////////////////////////////////
long Mount::getCurrentStepperPosition(int direction)
{
    if (direction & TRACKING)
    {
        return _stepperTRK->currentPosition();
    }
    if (direction & (NORTH | SOUTH))
    {
        return _stepperDEC->currentPosition();
    }
    if (direction & (EAST | WEST))
    {
        return _stepperRA->currentPosition();
    }
    return 0;
}

long Mount::getCurrentStepperPosition(StepperAxis axis)
{
    if (axis == StepperAxis::DEC_STEPS)
    {
        return _stepperDEC->currentPosition();
    }
    if (axis == StepperAxis::RA_STEPS)
    {
        return _stepperRA->currentPosition();
    }
    if (axis == StepperAxis::FOCUS_STEPS)
    {
        if (kHasFocusStepper)
        {
            return _stepperFocus->currentPosition();
        }
    }
    if (axis == StepperAxis::ALTITUDE_STEPS)
    {
        if (kHasAltStepper)
        {
            return _stepperALT->currentPosition();
        }
    }
    if (axis == StepperAxis::AZIMUTH_STEPS)
    {
        if (kHasAzStepper)
        {
            return _stepperAZ->currentPosition();
        }
    }
    return 0;
}

/////////////////////////////////
//
// setHomingOffset
//
/////////////////////////////////
void Mount::setHomingOffset(StepperAxis axis, long offset)
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

/////////////////////////////////
//
// getHomingOffset
//
/////////////////////////////////
long Mount::getHomingOffset(StepperAxis axis)
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
bool Mount::findHomeByHallSensor(StepperAxis axis, int initialDirection, int searchDistance)
{
    #if USE_HALL_SENSOR_RA_AUTOHOME == 1
    if (axis == StepperAxis::RA_STEPS)
    {
        if (_raHoming != nullptr)
        {
            delete _raHoming;
        }
        _raHoming = new HallSensorHoming(this, axis, makeHomingSensor(RA_HOMING_SENSOR_PIN, RA_HOMING_SENSOR_ACTIVE_STATE));
        const bool started = _raHoming->findHomeByHallSensor(initialDirection, searchDistance);
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
        if (_decHoming != nullptr)
        {
            delete _decHoming;
        }
        _decHoming = new HallSensorHoming(this, axis, makeHomingSensor(DEC_HOMING_SENSOR_PIN, DEC_HOMING_SENSOR_ACTIVE_STATE));
        const bool started = _decHoming->findHomeByHallSensor(initialDirection, searchDistance);
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
void Mount::processHomingProgress()
{
    if (kHasRaHoming && (_raHoming != nullptr) && (!_raHoming->isIdleOrComplete()))
    {
        _raHoming->processHomingProgress();
        if (_raHoming->isIdleOrComplete())
        {
            publishEvent(core::MountEventType::HomingFinished, _raHoming->homingResult() == core::HomingResult::Succeeded);
        }
    }

    if (kHasDecHoming && (_decHoming != nullptr) && (!_decHoming->isIdleOrComplete()))
    {
        _decHoming->processHomingProgress();
        if (_decHoming->isIdleOrComplete())
        {
            publishEvent(core::MountEventType::HomingFinished, _decHoming->homingResult() == core::HomingResult::Succeeded);
        }
    }
}

/// End switches are polled twice: once to debounce the raw pin, once to act on the
/// debounced state. On a board without them both calls compile out.
void Mount::pollEndSwitches()
{
    if (kHasRaEndSwitch && (_raEndSwitch != nullptr))
    {
        _raEndSwitch->processEndSwitchState();
        _raEndSwitch->checkSwitchState();
    }

    if (kHasDecEndSwitch && (_decEndSwitch != nullptr))
    {
        _decEndSwitch->processEndSwitchState();
        _decEndSwitch->checkSwitchState();
    }
}

const char *Mount::getAutoHomingStates() const
{
    String state = homingStateText(kHasRaHoming && (_raHoming != nullptr), _raHoming);
    state += "|";
    state += homingStateText(kHasDecHoming && (_decHoming != nullptr), _decHoming);
    _readoutScratch = state;
    return _readoutScratch.c_str();
}

/// The Meade status fragment for one axis' sensor. A board without the sensor reports the
/// same "NEVER RUN"/empty shape a sensor that has never homed does.
String Mount::homingStateText(bool present, const HallSensorHoming *sensor) const
{
    if (!present)
    {
        return String();
    }
    if (!sensor->isIdleOrComplete())
    {
        return sensor->getHomingState(sensor->getHomingState());
    }
    return sensor->getLastResult();
}

#if (USE_RA_END_SWITCH == 1 || USE_DEC_END_SWITCH == 1)
/////////////////////////////////
//
// End Switches RA/DEC
//
/////////////////////////////////
void Mount::setupEndSwitches()
{
    #if (USE_RA_END_SWITCH == 1)
    _raEndSwitch = new EndSwitch(this,
                                 StepperAxis::RA_STEPS,
                                 makeEndSwitch(RA_ENDSWITCH_EAST_SENSOR_PIN, RA_END_SWITCH_ACTIVE_STATE),
                                 makeEndSwitch(RA_ENDSWITCH_WEST_SENSOR_PIN, RA_END_SWITCH_ACTIVE_STATE));
    #endif

    #if (USE_DEC_END_SWITCH == 1)
    _decEndSwitch = new EndSwitch(this,
                                  StepperAxis::DEC_STEPS,
                                  makeEndSwitch(DEC_ENDSWITCH_DOWN_SENSOR_PIN, DEC_END_SWITCH_ACTIVE_STATE),
                                  makeEndSwitch(DEC_ENDSWITCH_UP_SENSOR_PIN, DEC_END_SWITCH_ACTIVE_STATE));
    #endif
}
#endif

/////////////////////////////////
//
// delay
//
/////////////////////////////////
void Mount::delay(int ms)
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
// interruptLoop()
//
// This function is only called on run in an ISR. It needs to be fast and do little work.
/////////////////////////////////
#if defined(ESP32) || !defined(NEW_STEPPER_LIB)
void Mount::interruptLoop()
{
    // Only process guide pulses if we are tracking.
    if (_coordinator.state().test(STATUS_GUIDE_PULSE) && _coordinator.state().test(STATUS_TRACKING))
    {
        _stepperTRK->runSpeed();
        if (_coordinator.state().test(STATUS_GUIDE_PULSE_DEC))
        {
            _stepperGUIDE->runSpeed();
        }
        return;
    }

    if (_coordinator.state().test(STATUS_TRACKING))
    {
        _stepperTRK->runSpeed();
    }

    if (_coordinator.state().test(STATUS_SLEWING))
    {
        if (_coordinator.state().test(STATUS_SLEWING_MANUAL))
        {
            _stepperDEC->runSpeed();
            _stepperRA->runSpeed();
        }
        else
        {
            _stepperDEC->run();
            _stepperRA->run();
        }
    }
    else if (!_coordinator.state().test(STATUS_TRACKING) && (_stepperTRK->isRunning()))
    {
        // If we are not tracking, but the tracking stepper is running, we need to let it move.
        // This can happen when we need to compensate for a slew (during which the tracker is
        // stopped). After the slew, the tracker is advanced by the distance it would have
        // travelled during the slew if it had been tracking.
        // That compensation uses goto mode (using runToNewPosition()), so we need to use run(),
        // since runSpeed() only advances the stepper when it is in constant speed mode.
        _stepperTRK->run();
    }

    if (_coordinator.state().test(STATUS_FINDING_HOME))
    {
        if (kHasRaHoming && (_raHoming != nullptr))
        {
            _stepperRA->run();
            _raHoming->processHomingProgress();
        }
        if (kHasDecHoming && (_decHoming != nullptr))
        {
            _stepperDEC->run();
            _decHoming->processHomingProgress();
        }
    }

    if (kHasAzStepper)
    {
        _stepperAZ->run();
    }
    if (kHasAltStepper)
    {
        _stepperALT->run();
    }

    // `FocusMode::Continuous` is kept from the original, which never selected it: a
    // continuous run targets a far position and therefore uses `run()`.
    if (kHasFocusStepper && _coordinator.focus().mode() == core::FocusMode::ToTarget)
    {
        _stepperFocus->run();
    }
    else if (kHasFocusStepper && _coordinator.focus().mode() == core::FocusMode::Continuous)
    {
        _stepperFocus->runSpeed();
    }

    if (kHasRaEndSwitch && (_raEndSwitch != nullptr))
    {
        _raEndSwitch->processEndSwitchState();
    }

    if (kHasDecEndSwitch && (_decEndSwitch != nullptr))
    {
        _decEndSwitch->processEndSwitchState();
    }
}
#endif

/////////////////////////////////
//
// loop
//
// Process any stepper changes.
/////////////////////////////////
void Mount::loop()
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

    // LOG(DEBUG_MOUNT, "[MOUNT]: Focuser running:  %d", _stepperFocus->isRunning());

    if (_coordinator.focus().isRunning())
    {
        OAT_LOG(_deps.logger, DEBUG_FOCUS, "[MOUNT]: Loop: Focuser running at speed %f", _coordinator.focus().speed());
    }

    bool oneIsRunning = false;
    if (kHasAzStepper)
    {
        oneIsRunning |= _stepperAZ->isRunning();
    }
    if (kHasAltStepper)
    {
        oneIsRunning |= _stepperALT->isRunning();
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
            _deps.settings->storeAZPosition(_stepperAZ->currentPosition());
        }
        if (kHasAltStepper)
        {
            _deps.settings->storeALTPosition(_stepperALT->currentPosition());
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

    if (_stepperDEC->isRunning())
    {
        decStillRunning = true;
    }

    if (_stepperRA->isRunning())
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
                    _stepperRA->currentPosition(),
                    _stepperDEC->currentPosition());

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

            _currentRAStepperPosition = _stepperRA->currentPosition();
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
                        _backlashCorrectionSteps);
                _currentRAStepperPosition += _backlashCorrectionSteps;
                _stepperRA->runToNewPosition(_currentRAStepperPosition);
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
                _stepperRA->moveTo(-getHomingOffset(StepperAxis::RA_STEPS));
                _stepperDEC->moveTo(-getHomingOffset(StepperAxis::DEC_STEPS));
                _coordinator.slew().setTotalMove(1.0f * _stepperRA->distanceToGo(), 1.0f * _stepperDEC->distanceToGo());
                OAT_LOG(_deps.logger,
                        DEBUG_MOUNT | DEBUG_STEPPERS,
                        "[MOUNT]: Loop:   Park Position is R:%ld  D:%ld, TotalMove is R:%f, D:%f",
                        -getHomingOffset(StepperAxis::RA_STEPS),
                        -getHomingOffset(StepperAxis::DEC_STEPS),
                        _coordinator.slew().totalRaSteps(),
                        _coordinator.slew().totalDecSteps());
                parkLegIsNoOp = core::SlewCompletion::parkLegIsNoOp(_stepperRA->distanceToGo(), _stepperDEC->distanceToGo());
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
void Mount::bootComplete()
{
    _bootComplete = true;
}

/////////////////////////////////
//
// isBootComplete
//
/////////////////////////////////
bool Mount::isBootComplete()
{
    return _bootComplete;
}

/////////////////////////////////
//
// setDecLimitPosition
//
// If limitAngle is 0, no offset is given, so the current position is used.
/////////////////////////////////
void Mount::setDecLimitPosition(bool upper, float limitAngle)
{
    if (upper)
    {
        if (limitAngle == 0)
        {
            _decUpperLimit = _stepperDEC->currentPosition();
            _deps.settings->storeDECUpperLimit(fabsf(_decUpperLimit / _stepsPerDECDegree));
        }
        else
        {
            _decUpperLimit = (limitAngle * _stepsPerDECDegree);
            _deps.settings->storeDECUpperLimit(limitAngle);
        }
        OAT_LOG(_deps.logger,
                DEBUG_MOUNT,
                "[MOUNT]: setDecLimitPosition(Upper) to %f: limit DEC: %ld -> %ld",
                limitAngle,
                _decLowerLimit,
                _decUpperLimit);
    }
    else
    {
        if (limitAngle == 0)
        {
            _decLowerLimit = _stepperDEC->currentPosition();
            _deps.settings->storeDECLowerLimit(fabsf(_decLowerLimit / _stepsPerDECDegree));
        }
        else
        {
            _decLowerLimit = -(limitAngle * _stepsPerDECDegree);
            _deps.settings->storeDECLowerLimit(limitAngle);
        }
        OAT_LOG(_deps.logger,
                DEBUG_MOUNT,
                "[MOUNT]: setDecLimitPosition(Lower) to %f: limit DEC: %ld -> %ld",
                limitAngle,
                _decLowerLimit,
                _decUpperLimit);
    }
}

/////////////////////////////////
//
// clearDecLimitPosition
//
/////////////////////////////////
void Mount::clearDecLimitPosition(bool upper)
{
    if (upper)
    {
        _decUpperLimit = DEC_LIMIT_UP * _stepsPerDECDegree;
        _deps.settings->storeDECUpperLimit(DEC_LIMIT_UP);
        OAT_LOG(_deps.logger, DEBUG_MOUNT, "[MOUNT]: clearDecLimitPosition(Upper): limit DEC: %ld -> %ld", _decLowerLimit, _decUpperLimit);
    }
    else
    {
        _decLowerLimit = -(DEC_LIMIT_DOWN * _stepsPerDECDegree);
        _deps.settings->storeDECLowerLimit(DEC_LIMIT_DOWN);
        OAT_LOG(_deps.logger, DEBUG_MOUNT, "[MOUNT]: clearDecLimitPosition(Lower): limit DEC: %ld -> %ld", _decLowerLimit, _decUpperLimit);
    }
}

/////////////////////////////////
//
// getDecLimitPositions
//
/////////////////////////////////
void Mount::getDecLimitPositions(float &lowerLimit, float &upperLimit)
{
    lowerLimit = -1.0f * _decLowerLimit / _stepsPerDECDegree;
    upperLimit = 1.0f * _decUpperLimit / _stepsPerDECDegree;
    OAT_LOG(_deps.logger,
            DEBUG_MOUNT,
            "[MOUNT]: getDecLimitPositions: limit DEC: %ld -> %ld (%f -> %f)",
            _decLowerLimit,
            _decUpperLimit,
            lowerLimit,
            upperLimit);
}

/////////////////////////////////
//
// setHome
//
/////////////////////////////////
void Mount::setHome(bool clearZeroPos)
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
    //LOG(DEBUG_MOUNT_VERBOSE, "[MOUNT]: setHomePre: zeroPos is %s", _zeroPosRA.ToString());
    _zeroPosRA = clearZeroPos ? DayTime(POLARIS_RA_HOUR, POLARIS_RA_MINUTE, POLARIS_RA_SECOND) : calculateLst();
#if defined(OAM) || defined(OAE)
    _zeroPosRA.addHours(6);  // shift allcoordinates by 90° for EQ mount movement
#endif
    _zeroPosDEC = 0.0f;

    _stepperRA->setCurrentPosition(0);
    _stepperDEC->setCurrentPosition(0);
    _stepperTRK->setCurrentPosition(0);
    _stepperGUIDE->setCurrentPosition(0);

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
    //LOG(DEBUG_MOUNT_VERBOSE, "[MOUNT]: setHomePost: zeroPos is %s", _zeroPosRA.ToString());
    //LOG(DEBUG_MOUNT_VERBOSE, "[MOUNT]: setHomePost: targetRA is %s", targetRA().ToString());
}

/////////////////////////////////
//
// getSpeed
//
// Get the current speed of the stepper. NORTH, WEST, TRACKING
/////////////////////////////////
float Mount::getSpeed(int direction)
{
    if (direction & TRACKING)
    {
        return _coordinator.tracking().speed();
    }
    if (direction & (NORTH | SOUTH))
    {
        return _stepperDEC->speed();
    }
    if (direction & (EAST | WEST))
    {
        return _stepperRA->speed();
    }
    return 0;
}

/////////////////////////////////
//
// calculateStepperPositions
//
// This code calculates the stepper locations to move to, given the right ascension and declination
/////////////////////////////////
void Mount::calculateStepperPositions(float raCoord, float decCoord, long &raPos, long &decPos)
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
void Mount::calculateRAandDECSteppers(long &targetRASteps, long &targetDECSteps, long pSolutions[6]) const
{
    OAT_LOG(_deps.logger,
            DEBUG_COORD_CALC,
            "[MOUNT]: CalcSteppersPre: Current : RA: %s, DEC: %s",
            DayTime(currentRA()).ToString(),
            Declination(currentDEC()).ToString());
    OAT_LOG(
        _deps.logger, DEBUG_COORD_CALC, "[MOUNT]: CalcSteppersPre: Target  : RA: %s, DEC: %s", _targetRA.ToString(), _targetDEC.ToString());
    OAT_LOG(_deps.logger, DEBUG_COORD_CALC, "[MOUNT]: CalcSteppersPre: ZeroRA  : %s", _zeroPosRA.ToString());
    OAT_LOG(_deps.logger, DEBUG_COORD_CALC, "[MOUNT]: CalcSteppersPre: ZeroDEC : %f", _zeroPosDEC);
    OAT_LOG(_deps.logger,
            DEBUG_COORD_CALC,
            "[MOUNT]: CalcSteppersPre: Stepper: RA: %ld, DEC: %ld, TRK: %ld",
            _stepperRA->currentPosition(),
            _stepperDEC->currentPosition(),
            _stepperTRK->currentPosition());
    DayTime raTarget      = _targetRA;
    Declination decTarget = _targetDEC;

    // Calculate how far from the home position this new target is.
    raTarget.subtractTime(_zeroPosRA);
    OAT_LOG(_deps.logger,
            DEBUG_COORD_CALC,
            "[MOUNT]: CalcSteppersIn: Adjust RA by ZeroPosRA. New Target RA: %s, DEC: %s",
            raTarget.ToString(),
            _targetDEC.ToString());

    // Where do we want to move RA to?
    float moveRA = raTarget.getTotalHours();
    OAT_LOG(_deps.logger, DEBUG_COORD_CALC, "[MOUNT]: CalcSteppersIn: moveRA (target) is %f", moveRA);

    // Total hours of tracking-to-date
    float trackedHours = getTrackedHours();
    OAT_LOG(_deps.logger,
            DEBUG_COORD_CALC,
            "[MOUNT]: CalcSteppersIn: Tracked time is %ld steps (%f h).",
            _stepperTRK->currentPosition(),
            trackedHours);

    // The current RA of the home position, taking tracking-to-date into account
    float homeRA = _zeroPosRA.getTotalHours() + trackedHours;
    OAT_LOG(
        _deps.logger, DEBUG_COORD_CALC, "[MOUNT]: CalcSteppersIn: homeRA adjusted by elasped tracking actually represents %f h", homeRA);

    // Delta between target RA and home position with a normalized range of -12 hr to 12 hr
    float homeTargetDeltaRA = _targetRA.getTotalHours() - homeRA;
    OAT_LOG(_deps.logger,
            DEBUG_COORD_CALC,
            "[MOUNT]: CalcSteppersIn: Delta of home to targetRA (%f) is %f (will use to check limits) ",
            _targetRA.getTotalHours(),
            homeTargetDeltaRA);
    while (homeTargetDeltaRA > 12)
    {
        homeTargetDeltaRA = homeTargetDeltaRA - 24;
    }
    while (homeTargetDeltaRA < -12)
    {
        homeTargetDeltaRA = homeTargetDeltaRA + 24;
    }

    // Map [0 to 24] range to [-12 to +12] range
    while (moveRA > 12)
    {
        moveRA = moveRA - 24;
        OAT_LOG(_deps.logger,
                DEBUG_COORD_CALC,
                "[MOUNT]: CalcSteppersIn: moveRA>12 so -24. New Target RA: %s, DEC: %s",
                DayTime(moveRA).ToString(),
                _targetDEC.ToString());
    }

    // How many u-steps moves the RA ring one sidereal hour along when slewing. One sidereal hour moves just shy of 15 degrees
    float stepsPerSiderealHour = _stepsPerRADegree * siderealDegreesInHour;  // u-steps/deg * deg/hr = u-steps/hr

    // Where do we want to move DEC to?
    float moveDEC = decTarget.getTotalDegrees();
    if (!inNorthernHemisphere)
    {
        OAT_LOG(_deps.logger, DEBUG_COORD_CALC, "[MOUNT]: CalcSteppersIn: moveDEC inverted for southern Hemisphere => %f", moveDEC);
        moveDEC = -moveDEC;
    }

    OAT_LOG(_deps.logger,
            DEBUG_COORD_CALC,
            "[MOUNT]: CalcSteppersIn: Target hrs pos RA: Delta:%f (moveRA: %f), DEC: %s (moveDEC: %f)",
            homeTargetDeltaRA,
            moveRA,
            decTarget.ToString(),
            moveDEC);

    /*
  * Current RA wheel has a rotation limit of around 7 hours in each direction from home position.
  * Since tracking does not trigger the meridian flip, we try to extend the possible tracking time 
  * without reaching the RA ring end by executing the meridian flip before slewing to the target.
  * For this flip the RA and DEC rings have to be flipped by 180° (which is 12 RA hours). Based
  * on the physical RA ring limits, this means that the flip can only be executed during +/-[5h to 7h]
  * sections around the home position of RA. The tracking time will still be limited to around 2h in
  * worst case if the target is located right before the 5h mark during slewing. 
  */
    float const RALimitL = inNorthernHemisphere ? -RA_LIMIT_LEFT : -RA_LIMIT_RIGHT;
    float const RALimitR = inNorthernHemisphere ? RA_LIMIT_RIGHT : RA_LIMIT_LEFT;
    OAT_LOG(_deps.logger, DEBUG_COORD_CALC, "[MOUNT]: CalcSteppersIn: Limits are : %f to %f", RALimitL, RALimitR);

    core::SlewGeometry geometry;
    geometry.moveRA               = moveRA;
    geometry.moveDEC              = moveDEC;
    geometry.homeTargetDeltaRA    = homeTargetDeltaRA;
    geometry.stepsPerSiderealHour = stepsPerSiderealHour;
    geometry.stepsPerDECDegree    = _stepsPerDECDegree;
    geometry.zeroPosDEC           = _zeroPosDEC;
    geometry.raLimitL             = RALimitL;
    geometry.raLimitR             = RALimitR;

    const core::SlewResult result = core::calculateStepperPositions(geometry);

    if (pSolutions != nullptr)
    {
        for (int i = 0; i < 6; i++)
        {
            pSolutions[i] = result.solutions[i];
        }
    }

    OAT_LOG(_deps.logger, DEBUG_COORD_CALC, "[MOUNT]: CalcSteppersIn: Solution 1: %f, %f", -moveRA, moveDEC);
    OAT_LOG(_deps.logger, DEBUG_COORD_CALC, "[MOUNT]: CalcSteppersIn: Solution 2: %f, %f", -(moveRA - 12.0f), -moveDEC);
    OAT_LOG(_deps.logger, DEBUG_COORD_CALC, "[MOUNT]: CalcSteppersIn: Solution 3: %f, %f", -(moveRA + 12.0f), -moveDEC);

    switch (result.selected)
    {
        case core::SlewSolution::FlipUpper:
            OAT_LOG(
                _deps.logger,
                DEBUG_COORD_CALC,
                "[MOUNT]: CalcSteppersIn: Using Solution 2, since hometargetDeltaRA %f (RA:%f) is past max limit %f, inverting both axes",
                homeTargetDeltaRA,
                moveRA,
                RALimitR);
            break;
        case core::SlewSolution::FlipLower:
            OAT_LOG(
                _deps.logger,
                DEBUG_COORD_CALC,
                "[MOUNT]: CalcSteppersIn: Using solution 3 since homeTargetDeltaRA %f (RA:%f) is past min limit: %f, inverting both axes",
                homeTargetDeltaRA,
                moveRA,
                RALimitL);
            break;
        case core::SlewSolution::Direct:
            OAT_LOG(_deps.logger,
                    DEBUG_COORD_CALC,
                    "[MOUNT]: CalcSteppersIn: Using solution 1 since targetRA %f is in range. RA: %f, DEC: %f",
                    homeTargetDeltaRA,
                    moveRA,
                    moveDEC);
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
void Mount::moveSteppersTo(float targetRASteps, float targetDECSteps, StepperAxis direction)
{  // Units are u-steps (in slew mode)
    // Show time: tell the steppers where to go!
    _coordinator.slew().clearBacklashCorrection();
    OAT_LOG(
        _deps.logger, DEBUG_STEPPERS, "[STEPPERS]: MoveSteppersTo: RA  From: %ld  To: %f", _stepperRA->currentPosition(), targetRASteps);
    OAT_LOG(
        _deps.logger, DEBUG_STEPPERS, "[STEPPERS]: MoveSteppersTo: DEC From: %ld  To: %f", _stepperDEC->currentPosition(), targetDECSteps);

    if ((direction == RA_AND_DEC_STEPS) || (direction == RA_STEPS))
    {
        if ((_backlashCorrectionSteps != 0) && ((_stepperRA->currentPosition() - targetRASteps) > 0))
        {
            OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: MoveSteppersTo: Needs backlash correction of %d!", _backlashCorrectionSteps);
            targetRASteps -= _backlashCorrectionSteps;
            _coordinator.slew().requestBacklashCorrection();
        }

        _stepperRA->moveTo(targetRASteps);
    }

    if ((direction == RA_AND_DEC_STEPS) || (direction == DEC_STEPS))
    {
        if (_decUpperLimit != 0)
        {
#if DEBUG_LEVEL != 0
            if (targetDECSteps > (float) _decUpperLimit)
            {
                OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: MoveSteppersTo: DEC Upper Limit enforced. To: %ld", _decUpperLimit);
            }
#endif
            targetDECSteps = min(targetDECSteps, (float) _decUpperLimit);
        }

        if (_decLowerLimit != 0)
        {
#if DEBUG_LEVEL != 0
            if (targetDECSteps < (float) _decLowerLimit)
            {
                OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: MoveSteppersTo: DEC Lower Limit enforced. To: %ld", _decLowerLimit);
            }
#endif
            targetDECSteps = max(targetDECSteps, (float) _decLowerLimit);
        }

        _stepperDEC->moveTo(targetDECSteps);
    }
}

/////////////////////////////////
//
// moveStepperBy
//
/////////////////////////////////
void Mount::moveStepperBy(StepperAxis direction, long steps)
{
    OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: moveStepperBy: %ld", steps);

    switch (direction)
    {
        case RA_AND_DEC_STEPS:
            OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: moveStepperBy: MoveStepperBy does not support multiple axes.");
            break;

        case RA_STEPS:
            if (steps != 0)
            {
                // Only stop tracking if we're actually tracking and going to slew somewhere else, otherwise the
                // mount::loop() code won't detect the end of the slewing operation...
                if (isSlewingTRK())
                {
                    OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: moveStepperBy: Stop tracking (NEMA steppers)");
                    stopSlewing(TRACKING);
                    _coordinator.tracking().noteStopped(_deps.clock->now());
                    OAT_LOG(_deps.logger,
                            DEBUG_STEPPERS,
                            "[STEPPERS]: moveStepperBy: TRK stopped at %ldms",
                            _coordinator.tracking().stoppedAt());
                }
            }

            // Slew microsteps once the TRK stepper has stopped.
            setRaMicrosteps(_config.raSlewMicrosteps);
            setStatusFlag(STATUS_SLEWING | STATUS_SLEWING_TO_TARGET);
            publishEvent(core::MountEventType::SlewStarted);
            _coordinator.slew().noteRunning(true);
            moveSteppersTo(_stepperRA->currentPosition() + steps, 0, direction);
            _coordinator.slew().setTotalMove(1.0f * _stepperRA->distanceToGo(), _coordinator.slew().totalDecSteps());
            break;

        case DEC_STEPS:
            {
                setStatusFlag(STATUS_SLEWING | STATUS_SLEWING_TO_TARGET);
                publishEvent(core::MountEventType::SlewStarted);
                // Normal state for DEC is guide microstepping, so switch to slew microstepping here.
                setDecMicrosteps(_config.decSlewMicrosteps);
                _coordinator.slew().noteRunning(true);
                moveSteppersTo(0, _stepperDEC->currentPosition() + steps, direction);
                _coordinator.slew().setTotalMove(_coordinator.slew().totalRaSteps(), 1.0f * _stepperDEC->distanceToGo());
            }
            break;

        case FOCUS_STEPS:
            {
                if (kHasFocusStepper)
                {
                    focusMoveBy(steps);
                }
            }
            break;

        case AZIMUTH_STEPS:
            {
                if (kHasAzStepper)
                {
                    enableAzMotor();
                    OAT_LOG(_deps.logger,
                            DEBUG_STEPPERS,
                            "[STEPPERS]: moveStepperBy: AZ from %ld to %ld",
                            _stepperAZ->currentPosition(),
                            _stepperAZ->currentPosition() + steps);
                    _stepperAZ->moveTo(_stepperAZ->currentPosition() + steps);
                }
            }
            break;

        case ALTITUDE_STEPS:
            {
                if (kHasAltStepper)
                {
                    enableAltMotor();
                    _stepperALT->moveTo(_stepperALT->currentPosition() + steps);
                }
            }
            break;
    }
}

/////////////////////////////////
//
// moveStepperTo
//
/////////////////////////////////
void Mount::moveStepperTo(StepperAxis axis, long position)
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
core::DayTime Mount::hourAngle()
{
    return HA();
}

const char *Mount::raText(uint8_t type, uint8_t active)
{
    RAString(type, active);
    return scratchBuffer;
}

const char *Mount::decText(uint8_t type, uint8_t active)
{
    DECString(type, active);
    return scratchBuffer;
}

/////////////////////////////////
//
// IInfoDisplaySource
//
/////////////////////////////////
ports::InfoDisplayActivity Mount::activity()
{
    return ports::InfoDisplayActivity{isAxisRunning(RA_STEPS), isAxisRunning(DEC_STEPS)};
}

void Mount::fillInfoDisplayState(ports::InfoDisplayState &state)
{
    state.raSteps            = getCurrentStepperPosition(StepperAxis::RA_STEPS);
    state.decSteps           = getCurrentStepperPosition(StepperAxis::DEC_STEPS);
    state.trkSteps           = getCurrentStepperPosition(TRACKING);
    state.raStepsPerDegree   = getStepsPerDegree(StepperAxis::RA_STEPS);
    state.decStepsPerDegree  = getStepsPerDegree(StepperAxis::DEC_STEPS);

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

    state.latitudeHours  = latitude().getTotalHours();
    state.longitudeHours = longitude().getTotalHours();
    state.hoursToLimit   = checkRALimit();
    state.lst            = calculateLst();

    // The panel's coordinate text is the LCD shape the renderer drew before, produced here so the
    // renderer needs no formatter and no mount.
    const String raText  = RAString(LCD_STRING | CURRENT_STRING);
    const String decText = DECString(LCD_STRING | CURRENT_STRING);
    snprintf(state.raText, sizeof(state.raText), "%s", raText.c_str());
    snprintf(state.decText, sizeof(state.decText), "%s", decText.c_str());

    // The status line outlives this call because it is copied into a member-backed scratch.
    _infoStatusScratch = getStatusStateString();
    state.statusText   = _infoStatusScratch.c_str();
}

/////////////////////////////////
//
// getStepperProgress
//
/////////////////////////////////
bool Mount::getStepperProgress(int &raPercentage, int &decPercentage)
{
    // The rules live in core::SlewProgressReport; this gathers the four numbers it needs.
    const core::SlewProgress progress = core::SlewProgressReport::evaluate(
        _coordinator.slew().totalRaSteps(), _coordinator.slew().totalDecSteps(), _stepperRA->distanceToGo(), _stepperDEC->distanceToGo());
    raPercentage  = progress.raPercent;
    decPercentage = progress.decPercent;
    return progress.inProgress;
}
float Mount::plannedRaSteps() const
{
    return _coordinator.slew().totalRaSteps();
}

float Mount::plannedDecSteps() const
{
    return _coordinator.slew().totalDecSteps();
}

String Mount::DECString(byte type, byte active)
{
    Declination dec;
    if ((type & TARGET_STRING) == TARGET_STRING)
    {
        dec = _targetDEC;
    }
    else
    {
        dec = Declination(currentDEC());
    }

    core::formatDEC(scratchBuffer, dec, formatStringsDEC[type & FORMAT_STRING_MASK]);

    if ((type & FORMAT_STRING_MASK) == LCDMENU_STRING)
    {
        scratchBuffer[active * 4 + (active > 0 ? 1 : 0)] = '>';
    }

    return String(scratchBuffer);
}

/////////////////////////////////
//
// RAString
//
/////////////////////////////////
// Return a string of RA in the given format. For LCDSTRING, active determines where the cursor is
String Mount::RAString(byte type, byte active)
{
    DayTime ra;
    if ((type & TARGET_STRING) == TARGET_STRING)
    {
        ra = _targetRA;
    }
    else
    {
        ra = currentRA();
    }

    core::formatRA(scratchBuffer, sizeof(scratchBuffer), ra, formatStringsRA[type & FORMAT_STRING_MASK]);
    if ((type & FORMAT_STRING_MASK) == LCDMENU_STRING)
    {
        scratchBuffer[active * 4] = '>';
    }
    return String(scratchBuffer);
}

/////////////////////////////////
//
//
//
/////////////////////////////////
core::DayTime Mount::getUtcTime()
{
    DayTime timeUTC = getLocalTime();
    timeUTC.addHours(-_localUtcOffset);
    return timeUTC;
}

/////////////////////////////////
//
// localTime
//
/////////////////////////////////
core::DayTime Mount::getLocalTime()
{
    DayTime timeLocal = _localStartTime;
    timeLocal.addSeconds((_deps.clock->now() - _localStartTimeSetMillis) / 1000);
    return timeLocal;
}

/////////////////////////////////
//
// localDate
//
/////////////////////////////////
LocalDate Mount::getLocalDate()
{
    LocalDate localDate        = _localStartDate;
    long secondsSinceSetDayEnd = ((_deps.clock->now() - _localStartTimeSetMillis) / 1000) - (86400 - _localStartTime.getTotalSeconds());

    // Number of day ends passed since the date was set; a partial day still counts as one.
    const long daysElapsed = secondsSinceSetDayEnd >= 0 ? (secondsSinceSetDayEnd / 86400) + 1 : 0;

    const core::CalendarDate rolled = core::addDays({localDate.year, localDate.month, localDate.day}, daysElapsed);
    localDate.year                  = rolled.year;
    localDate.month                 = rolled.month;
    localDate.day                   = rolled.day;

    return localDate;
}

/////////////////////////////////
//
// localUtcOffset
//
/////////////////////////////////
int Mount::getLocalUtcOffset() const
{
    return _localUtcOffset;
}

/////////////////////////////////
//
// setLocalStartDate
//
/////////////////////////////////
void Mount::setLocalStartDate(int year, int month, int day)
{
    _localStartDate.year  = year;
    _localStartDate.month = month;
    _localStartDate.day   = day;

    autoCalcHa();
}

/////////////////////////////////
//
// setLocalStartTime
//
/////////////////////////////////
void Mount::setLocalStartTime(const core::DayTime &localTime)
{
    _localStartTime          = DayTime(localTime);
    _localStartTimeSetMillis = _deps.clock->now();

    autoCalcHa();
}

/////////////////////////////////
//
// setLocalUtcOffset
//
/////////////////////////////////
void Mount::setLocalUtcOffset(int offset)
{
    _localUtcOffset = offset;
    _deps.settings->storeUtcOffset(_localUtcOffset);

    autoCalcHa();
}

/////////////////////////////////
//
// autoCalcHa
//
/////////////////////////////////
void Mount::autoCalcHa()
{
    setHA(calculateHa());
}

/////////////////////////////////
//
// calculateLst
//
/////////////////////////////////
core::DayTime Mount::calculateLst()
{
    DayTime timeUTC     = getUtcTime();
    LocalDate localDate = getLocalDate();
    // The sidereal formula needs the UTC date, which differs from the local date for |UTC offset| hours each day.
    core::CalendarDate utcDate
        = core::localToUtcDate({localDate.year, localDate.month, localDate.day}, getLocalTime().getTotalSeconds(), _localUtcOffset);
    DayTime lst = Sidereal::calculateByDateAndTime(longitude().getTotalHours(), utcDate.year, utcDate.month, utcDate.day, &timeUTC);
    // LOG(DEBUG_INFO,
    //     "[MOUNT]: Calculating LST. UTC time: %s. UTC date: %d-%d-%d. Longitude: %s",
    //     timeUTC.ToString(),
    //     utcDate.year,
    //     utcDate.month,
    //     utcDate.day,
    //     longitude().ToString());
    // LOG(DEBUG_INFO, "[MOUNT]: LST is: %s", lst.ToString());
    return lst;
}

/////////////////////////////////
//
// calculateHa
//
/////////////////////////////////
core::DayTime Mount::calculateHa()
{
    DayTime lst = calculateLst();
    OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: Calculating HA from LST: %s", lst.ToString());
    DayTime ha = Sidereal::calculateHa(lst.getTotalHours());
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
void Mount::testRA_UART_TX()
{
    //microsteps per driver clock tick (From TMC2209 datasheet: v[Hz] (microstep/s) = VACTUAL[2209] * 0.715Hz)
    const int speed = (RA_STEPPER_SPEED / 2) / 0.715255737f;
    //Duration in ms to move X degrees at half of the max speed
    const int duration = UART_CONNECTION_TEST_TX_DEG * (_stepsPerRADegree / (RA_STEPPER_SPEED / 2)) * 1000;
    OAT_LOG(_deps.logger,
            DEBUG_STEPPERS,
            "[STEPPERS]: uartTest: Switching RA driver to microsteps(%d) for UART test",
            _config.raSlewMicrosteps);
    setRaMicrosteps(_config.raSlewMicrosteps);
    testUART_vactual(_driverRA, speed, duration);
    OAT_LOG(_deps.logger,
            DEBUG_STEPPERS,
            "[STEPPERS]: uartTest: Switching RA driver to microsteps(%d) after UART test",
            _config.raTrackingMicrosteps());
    setRaMicrosteps(_config.raTrackingMicrosteps());
}
    #endif

    #if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
void Mount::testDEC_UART_TX()
{
    //microsteps per driver clock tick (From TMC2209 datasheet: v[Hz] (microstep/s) = VACTUAL[2209] * 0.715Hz)
    const int speed = (DEC_STEPPER_SPEED / 2) / 0.715255737f;
    //Duration in ms to move X degrees at half of the max speed
    const int duration = UART_CONNECTION_TEST_TX_DEG * (_stepsPerDECDegree / (DEC_STEPPER_SPEED / 2)) * 1000;
    OAT_LOG(_deps.logger,
            DEBUG_STEPPERS,
            "[STEPPERS]: uartTest: Switching DEC driver to microsteps(%d) for UART test",
            _config.decSlewMicrosteps);
    setDecMicrosteps(_config.decSlewMicrosteps);
    testUART_vactual(_driverDEC, speed, duration);
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
void Mount::testUART_vactual(ports::IMotorDriver *driver, int _speed, int _duration)
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

float Mount::getTrackedHours() const
{
    return _coordinator.tracking().trackedHours();
}

/////////////////////////////////
//
// checkRALimit
//
/////////////////////////////////
float Mount::checkRALimit()
{
    // The arithmetic and the folding live in core::RaTrackingLimit; the side effect — stopping the
    // tracker — stays here, because it owns the axes.
    core::RaLimitFacts facts;
    facts.homeRaHours        = _zeroPosRA.getTotalHours() + getTrackedHours();
    facts.currentRaHours     = currentRA().getTotalHours();
    facts.decAxisDegrees     = (_stepperDEC->currentPosition() / _stepsPerDECDegree) + _zeroPosDEC;
    facts.northernHemisphere = inNorthernHemisphere;
    facts.limitHours         = RA_TRACKING_LIMIT;

    const core::RaLimitResult result = core::RaTrackingLimit::evaluate(facts);

    OAT_LOG(_deps.logger,
            DEBUG_MOUNT_VERBOSE,
            "[MOUNT]: checkRALimit: homeRA: %f (ZeroPos: %f + TrkHrs: %f), remaining: %f",
            facts.homeRaHours,
            _zeroPosRA.getTotalHours(),
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
