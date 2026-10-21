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
#include "libs/MappedDict/MappedDict.hpp"

#include "adapters/NullMotorDriver.hpp"
#include "adapters/StepperAxis.hpp"
#include "adapters/Tmc2209Driver.hpp"

#ifndef ARDUINO_AVR_ATmega2560
    #include "hal/arduino/AccelStepperMotor.hpp"
#endif

namespace
{
#ifndef ARDUINO_AVR_ATmega2560
/// Wraps a motion engine in its axis adapter. Both objects live for the lifetime of
/// the program, exactly like the concrete steppers they replace.
ports::IStepperAxis *makeAxis(hal::IStepperMotor &motor)
{
    return new adapters::StepperAxis(motor);
}
#endif
}  // namespace

PUSH_NO_WARNINGS
#ifdef NEW_STEPPER_LIB
    #ifdef __AVR_ATmega2560__
        #include "InterruptAccelStepper.h"
        #include "StepperConfiguration.hpp"
    #endif
#endif

#if (INFO_DISPLAY_TYPE == INFO_DISPLAY_TYPE_I2C_SSD1306_128x64)
    #include "SSD1306_128x64_Display.hpp"
#endif
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
Mount::Mount(LcdMenu *lcdMenu, const MountDeps &deps)
    : _deps(deps),
      // Tracking runs at the finer microstep rate while the steps-per-degree value is
      // the slew-mode one, hence the integer ratio (as the inline expression had it).
      _tracking(SIDEREAL_SECONDS_PER_DAY, RA_TRACKING_MICROSTEPPING / RA_SLEW_MICROSTEPPING), _guiding(deps.clock)
#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE) || (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
      ,
      _azAltWasRunning(false)
#endif
#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
      ,
      _stepsPerAZDegree(AZIMUTH_STEPS_PER_REV / 360)
#endif
#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
      ,
      _stepsPerALTDegree(ALTITUDE_STEPS_PER_REV / 360)
#endif

{
    _commandReceived = 0;

#if (INFO_DISPLAY_TYPE != INFO_DISPLAY_TYPE_NONE)
    _lastInfoUpdate = _deps.clock->now();
#endif
    _lcdMenu = lcdMenu;
    initializeVariables();
}

void Mount::initializeVariables()
{
    // We are now defaulting to northern hemisphere at 45deg. Switching is now supported
    // at runtime when the Latitude is received via Meade command.
    inNorthernHemisphere = NORTHERN_HEMISPHERE == 1;

    _stepsPerRADegree  = RA_STEPS_PER_DEGREE;   // u-steps per degree when slewing
    _stepsPerDECDegree = DEC_STEPS_PER_DEGREE;  // u-steps per degree when slewing

    _state.reset(0);
    _lastDisplayUpdate = 0;
    _slew.noteRunning(false);

    // Created by the configure* methods below; null until then.
    _stepperRA    = nullptr;
    _stepperTRK   = nullptr;
    _stepperDEC   = nullptr;
    _stepperGUIDE = nullptr;
    _tracking.detach();
#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    _driverRA = nullptr;
#endif
#if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    _driverDEC = nullptr;
#endif
#if AZ_STEPPER_TYPE != STEPPER_TYPE_NONE
    _stepperAZ = nullptr;
    #if AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    _driverAZ = nullptr;
    #endif
#endif
#if ALT_STEPPER_TYPE != STEPPER_TYPE_NONE
    _stepperALT = nullptr;
    #if ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    _driverALT = nullptr;
    #endif
#endif
#if FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE
    _stepperFocus = nullptr;
    _focus.detach();
    #if FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    _driverFocus = nullptr;
    #endif
#endif

    _latitude   = Latitude(inNorthernHemisphere ? 45.0f : -45.0f);
    _longitude  = Longitude(100.0);
    _zeroPosDEC = 0.0f;

    _slew.clearTotalMove();
    _moveRate                = 4;
    _backlashCorrectionSteps = 0;
    _slew.clearBacklashCorrection();
    _slew.end();
    _slew.setHeadingToParkPosition(false);
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

#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    _stepsPerAZDegree = _deps.settings->getAZStepsPerDegree();
    OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: AZ steps/deg is %f", _stepsPerAZDegree);
#endif

#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    _stepsPerALTDegree = _deps.settings->getALTStepsPerDegree();
    OAT_LOG(_deps.logger, DEBUG_INFO, "[MOUNT]: EEPROM: ALT steps/deg is %f", _stepsPerALTDegree);
#endif
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

#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    long altPos = _deps.settings->getALTPosition();
    _stepperALT->setCurrentPosition(altPos);
#endif

#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    long azPos = _deps.settings->getAZPosition();
    _stepperAZ->setCurrentPosition(azPos);
#endif

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
    _stepperRA = new StepperRaSlew(AccelStepper::DRIVER, pin1, pin2);

    // Use another AccelStepper to run the RA motor as well. This instance tracks earths rotation.
    _stepperTRK = new StepperRaTrk(AccelStepper::DRIVER, pin1, pin2);
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

    _tracking.attach(_stepperTRK);
}

/////////////////////////////////
//
// configureDECStepper
//
/////////////////////////////////
void Mount::configureDECStepper(byte pin1, byte pin2, uint32_t maxSpeed, uint32_t maxAcceleration)
{
#ifdef ARDUINO_AVR_ATmega2560
    _stepperDEC = new StepperDecSlew(AccelStepper::DRIVER, pin1, pin2);

    // Use another AccelStepper to run the DEC motor as well. This instance is used for guiding.
    _stepperGUIDE = new StepperDecTrk(AccelStepper::DRIVER, pin1, pin2);
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
    _stepperAZ = new StepperAzSlew(AccelStepper::DRIVER, pin1, pin2);
    #else
    _stepperAZ = makeAxis(*new hal::AccelStepperMotor(*new AccelStepper(AccelStepper::DRIVER, pin1, pin2)));
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
    _stepperALT = new StepperAltSlew(AccelStepper::DRIVER, pin1, pin2);
    #else
    _stepperALT = makeAxis(*new hal::AccelStepperMotor(*new AccelStepper(AccelStepper::DRIVER, pin1, pin2)));
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
    _stepperFocus = new StepperFocusSlew(AccelStepper::DRIVER, pin1, pin2);
    #else
    _stepperFocus = makeAxis(*new hal::AccelStepperMotor(*new AccelStepper(AccelStepper::DRIVER, pin1, pin2)));
    #endif
    _stepperFocus->setMaxSpeed(maxSpeed);
    _stepperFocus->setAcceleration(maxAcceleration);
    _stepperFocus->setSpeed(0);
    _stepperFocus->setCurrentPosition(50000);
    _maxFocusSpeed        = maxSpeed;
    _maxFocusAcceleration = maxAcceleration;
    _focus.attach(_stepperFocus);
    _focus.initialize(maxSpeed);
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
        #if defined(ARDUINO_ARCH_RP2040)
    _driverRA->toff(4);  // toff>=3 required for StealthChop slow-decay phase
        #else
    _driverRA->toff(1);
        #endif
    _driverRA->en_spreadCycle(RA_UART_STEALTH_MODE == 0);
    _driverRA->blank_time(24);
    _driverRA->microsteps(core::microstepRegisterValue(RA_TRACKING_MICROSTEPPING));  // System starts in tracking mode
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
        #if defined(ARDUINO_ARCH_RP2040)
    _driverRA->toff(4);  // toff>=3 required for StealthChop slow-decay phase
        #else
    _driverRA->toff(1);
        #endif
    _driverRA->en_spreadCycle(RA_UART_STEALTH_MODE == 0);
    _driverRA->blank_time(24);
    _driverRA->semin(0);                                                             //disable CoolStep so that current is consistent
    _driverRA->microsteps(core::microstepRegisterValue(RA_TRACKING_MICROSTEPPING));  // System starts in tracking mode
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
        #if defined(ARDUINO_ARCH_RP2040)
    _driverDEC->toff(4);  // toff>=3 required for StealthChop slow-decay phase
        #else
    _driverDEC->toff(1);
        #endif
    _driverDEC->en_spreadCycle(DEC_UART_STEALTH_MODE == 0);
    _driverDEC->blank_time(24);
    _driverDEC->microsteps(
        core::microstepRegisterValue(DEC_GUIDE_MICROSTEPPING));  // If 1 then disable microstepping. Start with Guide microsteps.
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
        #if defined(ARDUINO_ARCH_RP2040)
    _driverDEC->toff(4);  // toff>=3 required for StealthChop slow-decay phase
        #else
    _driverDEC->toff(1);
        #endif
    _driverDEC->en_spreadCycle(DEC_UART_STEALTH_MODE == 0);
    _driverDEC->blank_time(24);
    _driverDEC->microsteps(
        core::microstepRegisterValue(DEC_GUIDE_MICROSTEPPING));  // If 1 then disable microstepping. Start with Guide microsteps
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
        #if defined(ARDUINO_ARCH_RP2040)
    _driverAZ->toff(4);  // toff>=3 required for StealthChop slow-decay phase
        #else
    _driverAZ->toff(1);
        #endif
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
        #if defined(ARDUINO_ARCH_RP2040)
    _driverAZ->toff(4);  // toff>=3 required for StealthChop slow-decay phase
        #else
    _driverAZ->toff(1);
        #endif
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
        #if defined(ARDUINO_ARCH_RP2040)
    _driverALT->toff(4);  // toff>=3 required for StealthChop slow-decay phase
        #else
    _driverALT->toff(1);
        #endif
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
    UART_Rx_connected      = connectToDriver("ALT");
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
        #if defined(ARDUINO_ARCH_RP2040)
    _driverALT->toff(4);  // toff>=3 required for StealthChop slow-decay phase
        #else
    _driverALT->toff(1);
        #endif
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
        #if defined(ARDUINO_ARCH_RP2040)
    _driverFocus->toff(4);  // toff>=3 required for StealthChop slow-decay phase
        #else
    _driverFocus->toff(1);
        #endif
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
        #if defined(ARDUINO_ARCH_RP2040)
    _driverFocus->toff(4);  // toff>=3 required for StealthChop slow-decay phase
        #else
    _driverFocus->toff(1);
        #endif
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
    return _tracking.calibration();
}

/////////////////////////////////
//
// setSpeedCalibration
//
/////////////////////////////////
void Mount::setSpeedCalibration(float val, bool saveToStorage)
{
    OAT_LOG(_deps.logger, DEBUG_MOUNT, "[MOUNT]: Updating speed calibration from %f to %f", _tracking.calibration(), val);
    _tracking.setCalibration(val);

    OAT_LOG(_deps.logger, DEBUG_MOUNT, "[MOUNT]: Current tracking speed is %f steps/sec", _tracking.speed());

    // The tracking rate is derived from the slewing geometry; see core/TrackingController.hpp.
    _tracking.setStepsPerDegree(_stepsPerRADegree);
    OAT_LOG(_deps.logger, DEBUG_MOUNT, "[MOUNT]: RA steps per degree is %f steps/deg", _stepsPerRADegree);
    OAT_LOG(_deps.logger, DEBUG_MOUNT, "[MOUNT]: New tracking speed is %f steps/sec", _tracking.speed());

    OAT_LOG(_deps.logger, DEBUG_MOUNT, "[MOUNT]: FactorToSpeed : %s, %s", String(val, 6).c_str(), String(_tracking.speed(), 6).c_str());

    if (saveToStorage)
        _deps.settings->storeSpeedFactor(_tracking.calibration());

    // If we are currently tracking, update the speed. No need to update microstepping mode
    if (isSlewingTRK())
    {
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[MOUNT]: SpeedCalibration TRK.setSpeed(%f)", _tracking.speed());
        _tracking.applySpeed();
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
#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
        return _stepsPerAZDegree;  // u-steps/degree
#else
        return 1;
#endif
    }
    if (which == ALTITUDE_STEPS)
    {
#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
        return _stepsPerALTDegree;  // u-steps/degree
#else
        return 1;
#endif
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
        setSpeedCalibration(_tracking.calibration(), false);
    }
#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    else if (which == AZIMUTH_STEPS)
    {
        _stepsPerAZDegree = steps;
        _deps.settings->storeAZStepsPerDegree(_stepsPerAZDegree);
    }
#endif
#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    else if (which == ALTITUDE_STEPS)
    {
        _stepsPerALTDegree = steps;
        _deps.settings->storeALTStepsPerDegree(_stepsPerALTDegree);
    }
#endif
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
String Mount::getStepperInfo()
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
    ret += String(RA_SLEW_MICROSTEPPING);
    ret += ",";
    ret += String(RA_TRACKING_MICROSTEPPING);

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
    ret += String(DEC_SLEW_MICROSTEPPING);
    ret += ",";
    ret += String(DEC_GUIDE_MICROSTEPPING);

    ret += "|";

    return ret;
}

/////////////////////////////////
//
// getMountHardwareInfo
//
/////////////////////////////////
String Mount::getMountHardwareInfo()
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

    return ret;
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
void Mount::setHA(const DayTime &haTime)
{
    OAT_LOG(_deps.logger, DEBUG_MOUNT, "[MOUNT]: setHA:  HA is %s", haTime.ToString());
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
void Mount::setLST(const DayTime &lst)
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
void Mount::setLatitude(Latitude latitude)
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
void Mount::setLongitude(Longitude longitude)
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
const Latitude Mount::latitude() const
{
    return _latitude;
}
/////////////////////////////////
//
// longitude
//
/////////////////////////////////
const Longitude Mount::longitude() const
{
    return _longitude;
}

/////////////////////////////////
//
// targetRA
//
/////////////////////////////////
// Get a reference to the target RA value.
DayTime &Mount::targetRA()
{
    return _targetRA;
}

/////////////////////////////////
//
// targetDEC
//
/////////////////////////////////
// Get a reference to the target DEC value.
Declination &Mount::targetDEC()
{
    return _targetDEC;
}

/////////////////////////////////
//
// currentRA
//
/////////////////////////////////
// Get current RA value.
const DayTime Mount::currentRA() const
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
const Declination Mount::currentDEC() const
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
void Mount::syncPosition(DayTime ra, Declination dec)
{
    long solutions[6];
    _targetDEC = dec;
    _targetRA  = ra;
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
            _tracking.noteStopped(_deps.clock->now());
        }

        // Set RA to slew microsteps for TMC2209 UART once the TRK stepper has stopped
        setRaMicrosteps(RA_SLEW_MICROSTEPPING);

        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: startSlewingToTarget: TRK stopped at %ldms", _tracking.stoppedAt());
    }

    _slew.begin(core::SlewIntent::Target);
    setStatusFlag(STATUS_SLEWING | STATUS_SLEWING_TO_TARGET);
    publishEvent(core::MountEventType::SlewStarted);
    // Normal state for DEC is guide microstepping, so switch to slew microstepping here.
    setDecMicrosteps(DEC_SLEW_MICROSTEPPING);
    _slew.noteRunning(true);
    moveSteppersTo(targetRAPosition, targetDECPosition, RA_AND_DEC_STEPS);  // u-steps (in slew mode)
    _slew.setTotalMove(static_cast<float>(_stepperRA->distanceToGo()), static_cast<float>(_stepperDEC->distanceToGo()));
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

    _slew.begin(core::SlewIntent::Home);
    // Take tracking into account
    const long trackingOffset = _stepperTRK->currentPosition() * RA_SLEW_MICROSTEPPING / RA_TRACKING_MICROSTEPPING;
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
        _tracking.noteStopped(_deps.clock->now());

        // set Slew microsteps for TMC2209 UART once the TRK stepper has stopped
        setRaMicrosteps(RA_SLEW_MICROSTEPPING);

        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: startSlewingToHome: TRK stopped at %ldms", _tracking.stoppedAt());
    }

    setStatusFlag(STATUS_SLEWING | STATUS_SLEWING_TO_TARGET);
    publishEvent(core::MountEventType::SlewStarted);
    // Normal state for DEC is guide microstepping, so switch to slew microstepping here.
    setDecMicrosteps(DEC_SLEW_MICROSTEPPING);
    _slew.noteRunning(true);
    moveSteppersTo(targetRAPosition, targetDECPosition, RA_AND_DEC_STEPS);  // u-steps (in slew mode)
    _slew.setTotalMove(static_cast<float>(_stepperRA->distanceToGo()), static_cast<float>(_stepperDEC->distanceToGo()));
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
    if (ra && _state.test(STATUS_GUIDE_PULSE_RA))
    {
        OAT_LOG(
            _deps.logger, DEBUG_STEPPERS | DEBUG_GUIDE, "[GUIDE]: stopGuide:    TRK stop guide at  : %ld", _stepperTRK->currentPosition());
        _tracking.applySpeed();
        OAT_LOG(_deps.logger, DEBUG_STEPPERS | DEBUG_GUIDE, "[GUIDE]: stopGuide:    TRK speed set to   : %f", _tracking.speed());
        _guiding.clearRa();
        clearStatusFlag(STATUS_GUIDE_PULSE_RA);
    }

    if (dec && _state.test(STATUS_GUIDE_PULSE_DEC))
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

        _guiding.clearDec();
        clearStatusFlag(STATUS_GUIDE_PULSE_DEC);
    }

    //disable pulse state if no direction is active
    if (!_state.test(STATUS_GUIDE_PULSE_DIR))
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
    const bool wasGuiding = _state.test(STATUS_GUIDE_PULSE);

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
                    (1.0 * DEC_GUIDE_MICROSTEPPING / DEC_SLEW_MICROSTEPPING));
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
                    (1.0 * RA_TRACKING_MICROSTEPPING / RA_SLEW_MICROSTEPPING));
        }

        // DEC stepper moves at sidereal rate in both directions
        // RA stepper moves at either 2.5x sidereal rate or 0.5x sidereal rate.
        // Also compensate for microstepping mode change between slew & guiding/tracking
        const core::GuideAxisGeometry raGeometry
            = {_stepsPerRADegree, RA_TRACKING_MICROSTEPPING, RA_SLEW_MICROSTEPPING, RA_PULSE_MULTIPLIER};
        const core::GuideAxisGeometry decGeometry
            = {_stepsPerDECDegree, DEC_GUIDE_MICROSTEPPING, DEC_SLEW_MICROSTEPPING, DEC_PULSE_MULTIPLIER};

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
                        _guiding.startDec(duration);
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
                        _guiding.startDec(duration);
                    }
                }
                break;

            case WEST:
                {
                    // We were in tracking mode before guiding, so no need to update microstepping mode on RA driver
                    const core::RaPulse pulse
                        = core::GuidingController::raPulse(true, raGeometry, siderealDegreesInHour, _tracking.calibration());
                    OAT_LOG(_deps.logger, DEBUG_STEPPERS | DEBUG_GUIDE, "[GUIDE]: guidePulse:   RA  base speed      : %f", pulse.baseSpeed);
                    OAT_LOG(_deps.logger,
                            DEBUG_STEPPERS | DEBUG_GUIDE,
                            "[GUIDE]: guidePulse:   RA  speed factor    : %f",
                            _tracking.calibration());
                    OAT_LOG(
                        _deps.logger, DEBUG_STEPPERS | DEBUG_GUIDE, "[GUIDE]: guidePulse:   RA  adjusted speed  : %f", pulse.adjustedSpeed);
                    OAT_LOG(_deps.logger,
                            DEBUG_STEPPERS | DEBUG_GUIDE,
                            "[GUIDE]: guidePulse:   RA  guide speed     : %f (%f x adjusted speed)",
                            pulse.targetSpeed,
                            RA_PULSE_MULTIPLIER);
                    _stepperTRK->setSpeed(pulse.targetSpeed);  // Faster than siderael
                    setStatusFlag(STATUS_GUIDE_PULSE | STATUS_GUIDE_PULSE_RA);
                    _guiding.startRa(duration);
                }
                break;

            case EAST:
                {
                    // We were in tracking mode before guiding, so no need to update microstepping mode on RA driver
                    const core::RaPulse pulse
                        = core::GuidingController::raPulse(false, raGeometry, siderealDegreesInHour, _tracking.calibration());
                    OAT_LOG(_deps.logger, DEBUG_STEPPERS | DEBUG_GUIDE, "[GUIDE]: guidePulse:   RA  base speed      : %f", pulse.baseSpeed);
                    OAT_LOG(_deps.logger,
                            DEBUG_STEPPERS | DEBUG_GUIDE,
                            "[GUIDE]: guidePulse:   RA  speed factor    : %f",
                            _tracking.calibration());
                    OAT_LOG(
                        _deps.logger, DEBUG_STEPPERS | DEBUG_GUIDE, "[GUIDE]: guidePulse:   RA  adjusted speed  : %f", pulse.adjustedSpeed);
                    OAT_LOG(_deps.logger,
                            DEBUG_STEPPERS | DEBUG_GUIDE,
                            "[GUIDE]: guidePulse:   RA  guide speed     : %f (%f x adjusted speed)",
                            (2.0 - RA_PULSE_MULTIPLIER * pulse.adjustedSpeed),
                            (2.0 - RA_PULSE_MULTIPLIER));
                    _stepperTRK->setSpeed(pulse.targetSpeed);  // Slower than siderael
                    setStatusFlag(STATUS_GUIDE_PULSE | STATUS_GUIDE_PULSE_RA);
                    _guiding.startRa(duration);
                }
                break;
        }
    }
    if (!wasGuiding && _state.test(STATUS_GUIDE_PULSE))
    {
        publishEvent(core::MountEventType::GuidingStarted);
    }

    // Since we will not be updating the display during a guide pulse, update the display here.
    updateInfoDisplay();

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
// runDriftAlignmentPhase
//
// Runs one of the phases of the Drift alignment
// This runs the RA motor 400 steps (about 5.3 arcminutes) in the given duration
// This function should be callsed 3 times:
// The first time with EAST, second with WEST and then with 0.
/////////////////////////////////
void Mount::runDriftAlignmentPhase(int direction, int durationSecs)
{
    float const numArcMinutes(5.3);
    long numSteps = floorf((_stepsPerRADegree * (numArcMinutes / 60.0f)));  // u-steps/deg * minutes / (minutes/deg) = u-steps

    // Calculate the speed at which it takes the given duration to cover the steps.
    float speed = numSteps / durationSecs;
    switch (direction)
    {
        case EAST:
            // Move steps east at the calculated speed, synchronously
            _stepperRA->setMaxSpeed(speed);
            _stepperRA->move(numSteps);
            _stepperRA->runToPosition();

            // Overcome the gearing gap
            _stepperRA->setMaxSpeed(300);
            _stepperRA->move(-20);
            _stepperRA->runToPosition();
            break;

        case WEST:
            // Move steps west at the calculated speed, synchronously
            _stepperRA->setMaxSpeed(speed);
            _stepperRA->move(-numSteps);
            _stepperRA->runToPosition();
            break;

        case 0:
            // Fix the gearing to go back the other way
            _stepperRA->setMaxSpeed(300);
            _stepperRA->move(20);
            _stepperRA->runToPosition();

            // Re-configure the stepper to the correct parameters.
            _stepperRA->setMaxSpeed(_maxRASpeed);
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
        setRaMicrosteps(RA_SLEW_MICROSTEPPING);
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
#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    else if (which == AZIMUTH_STEPS)
    {
        float stepsPerSec = speedDegsPerSec * _stepsPerAZDegree;  // deg/sec * u-steps/deg = u-steps/sec
        OAT_LOG(_deps.logger,
                DEBUG_STEPPERS,
                "[STEPPERS]: setSpeed: Set AZ speed %f degs/s, which is %f steps/s",
                speedDegsPerSec,
                stepsPerSec);
        _stepperAZ->setSpeed(stepsPerSec);
    }
#endif
#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    else if (which == ALTITUDE_STEPS)
    {
        float stepsPerSec = speedDegsPerSec * _stepsPerALTDegree;  // deg/sec * u-steps/deg = u-steps/sec
        OAT_LOG(_deps.logger,
                DEBUG_STEPPERS,
                "[STEPPERS]: setSpeed: Set ALT speed %f degs/s, which is %f steps/s",
                speedDegsPerSec,
                stepsPerSec);
        _stepperALT->setSpeed(stepsPerSec);
    }
#endif

#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    else if (which == FOCUS_STEPS)
    {
        OAT_LOG(_deps.logger, DEBUG_MOUNT | DEBUG_FOCUS, "[FOCUS]: setSpeed() Focuser setSpeed %f", speedDegsPerSec);
        if (speedDegsPerSec != 0)
        {
            OAT_LOG(_deps.logger,
                    DEBUG_STEPPERS | DEBUG_FOCUS,
                    "[FOCUS]: Mount:setSpeed(): Enabling motor, setting speed to %f. Continuous",
                    speedDegsPerSec);
            enableFocusMotor();
            _focus.runAtSpeed(speedDegsPerSec);
            publishEvent(core::MountEventType::FocusMoving);
        }
        else
        {
            OAT_LOG(_deps.logger, DEBUG_STEPPERS | DEBUG_FOCUS, "[FOCUS]: setSpeed(): Stopping motor.");
            _stepperFocus->stop();
        }
    }
#endif
}

void Mount::getAZALTPositions(long &azPos, long &altPos)
{
#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    azPos = _stepperAZ->currentPosition();
#else
    azPos = 0;
#endif
#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    altPos = _stepperALT->currentPosition();
#else
    altPos = 0;
#endif
}

void Mount::moveAZALTToHome()
{
#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    enableAzMotor();
    _stepperAZ->moveTo(0);
#endif
#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    enableAltMotor();
    _stepperALT->moveTo(0);
#endif
}

void Mount::setAZALTHome()
{
#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    _stepperAZ->setCurrentPosition(0);
    _deps.settings->storeAZPosition(0);
#endif
#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    _stepperALT->setCurrentPosition(0);
    _deps.settings->storeALTPosition(0);
#endif
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
    startSlewingToHome();
    setStatusFlag(STATUS_PARKING);
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
#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
        case ALTITUDE_STEPS:
            return _stepperALT->isRunning();
#endif
#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
        case AZIMUTH_STEPS:
            return _stepperAZ->isRunning();
#endif
#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
        case FOCUS_STEPS:
            return _stepperFocus->isRunning();
#endif
        default:
            break;
    }
    return false;
}

#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
/////////////////////////////////
//
// isRunningAZ
//
/////////////////////////////////
bool Mount::isRunningAZ() const
{
    return _stepperAZ->isRunning();
}

#endif

#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
/////////////////////////////////
//
// isRunningALT
//
/////////////////////////////////
bool Mount::isRunningALT() const
{
    return _stepperALT->isRunning();
}
#endif

#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE) || (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
/////////////////////////////////
//
// moveBy
//
/////////////////////////////////
void Mount::moveBy(int direction, float arcMinutes)
{
    #if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    if (direction == AZIMUTH_STEPS)
    {
        enableAzMotor();
        long stepsToMove = arcMinutes * AZIMUTH_STEPS_PER_ARC_MINUTE;
        _stepperAZ->move(stepsToMove);
    }
    #endif
    #if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    if (direction == ALTITUDE_STEPS)
    {
        enableAltMotor();
        long stepsToMove = arcMinutes * ALTITUDE_STEPS_PER_ARC_MINUTE;
        _stepperALT->move(stepsToMove);
    }
    #endif
}

/////////////////////////////////
//
// disableAzAltMotors
//
/////////////////////////////////
void Mount::disableAzAltMotors()
{
    #if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    _stepperAZ->stop();
    #endif
    #if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    _stepperALT->stop();
    #endif

    #if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    while (_stepperAZ->isRunning())
    {
        loop();
    }
    #endif

    #if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    while (_stepperALT->isRunning())
    {
        loop();
    }
    #endif

    #if AZ_ALWAYS_ON == 0
        #if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    _deps.azEnablePin->write(hal::PinLevel::High);  // Logic HIGH to disable driver
        #endif
    #endif

    #if ALT_ALWAYS_ON == 0
        #if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    _deps.altEnablePin->write(hal::PinLevel::High);  // Logic HIGH to disable driver
        #endif
    #endif
}

/////////////////////////////////
//
// enableAzAltMotors
//
/////////////////////////////////
void Mount::enableAzMotor()
{
    #if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    _deps.azEnablePin->write(hal::PinLevel::Low);  // Logic LOW to enable driver
    // Let loop() disable the driver again, even if the move turns out to be zero steps
    // (e.g. move to home while already at home), so it never stays powered.
    _azAltWasRunning = true;
    #endif
}

void Mount::enableAltMotor()
{
    #if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    _deps.altEnablePin->write(hal::PinLevel::Low);  // Logic LOW to enable driver
    // See enableAzMotor()
    _azAltWasRunning = true;
    #endif
}

#endif

#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
/////////////////////////////////
//
// isRunningFocus
//
/////////////////////////////////
bool Mount::isRunningFocus() const
{
    return _focus.isRunning();
}

/////////////////////////////////
//
// getFocusSpeed
//
/////////////////////////////////
float Mount::getFocusSpeed() const
{
    return _focus.speed();
}

/////////////////////////////////
//
// focusSetSpeedByRate
//
/////////////////////////////////
void Mount::focusSetSpeedByRate(int rate)
{
    _focus.setRate(rate, _maxFocusSpeed);
    OAT_LOG(_deps.logger,
            DEBUG_FOCUS,
            "[FOCUS]: focusSetSpeedByRate: rate is %d, factor %f, maxspeed %f -> %f",
            _focus.rate(),
            _focus.rateFactor(),
            _maxFocusSpeed,
            _focus.maxRateSpeed());

    if (_focus.isRunning())
    {
        OAT_LOG(_deps.logger, DEBUG_FOCUS, "[FOCUS]: focusSetSpeedByRate: stepper is already running so should adjust speed?");
        //_stepperFocus->setSpeed(_focus.rateFactor() * _maxFocusSpeed);
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
    OAT_LOG(_deps.logger, DEBUG_FOCUS, "[FOCUS]: focusContinuousMove: direction is %d, maxspeed %f", direction, _focus.maxRateSpeed());
    setSpeed(FOCUS_STEPS, static_cast<int>(direction) * _focus.maxRateSpeed());
}

/////////////////////////////////
//
// focusMoveBy
//
/////////////////////////////////
void Mount::focusMoveBy(long steps)
{
    OAT_LOG(_deps.logger, DEBUG_FOCUS, "[FOCUS]: focusMoveBy: move by %ld steps to %ld. Target Mode.", steps, _focus.position() + steps);
    enableFocusMotor();
    _focus.moveBy(steps);
    publishEvent(core::MountEventType::FocusMoving);
}

/////////////////////////////////
//
// focusGetPosition
//
/////////////////////////////////
long Mount::focusGetStepperPosition()
{
    return _focus.position();
}

/////////////////////////////////
//
// focusSetPosition
//
/////////////////////////////////
void Mount::focusSetStepperPosition(long steps)
{
    _focus.setPosition(steps);
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

    #if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
        #if FOCUSER_ALWAYS_ON == 0
            #if (FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)

    OAT_LOG(_deps.logger, DEBUG_FOCUS, "[FOCUS]: disableFocusMotor: TMC2209U disabling enable pin");
    _deps.focusEnablePin->write(hal::PinLevel::High);  // Logic HIGH to disable driver
            #else
    OAT_LOG(_deps.logger, DEBUG_FOCUS, "[FOCUS]: disableFocusMotor: non-TMC2209U disabling enable pin");
    _deps.focusEnablePin->write(hal::PinLevel::High);  // Logic HIGH to disable driver
            #endif
        #endif
    #endif
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
    _focus.stop();
}

#endif

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
    _events.subscribe(sink);
}

void Mount::clearEventSink()
{
    _events.unsubscribe();
}

/// Records a fact for the tick boundary. The next `loop()` delivers it, so a subscriber
/// is never called from inside the code that just changed the mount's state.
void Mount::publishEvent(core::MountEventType type, bool succeeded)
{
    core::MountEvent event;
    event.type      = type;
    event.succeeded = succeeded;
    _events.publish(event);
}

void Mount::setStatus(int state)
{
    noInterrupts();
    _state.reset((uint16_t) state);
    interrupts();
}

void Mount::setStatusFlag(int flag)
{
    noInterrupts();
    _state.set((uint16_t) flag);
    interrupts();
}

void Mount::clearStatusFlag(int flag)
{
    noInterrupts();
    _state.clear((uint16_t) flag);
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
    if (_state.isParked())
    {
        status = F("Parked");
    }
    else if (_state.test(STATUS_PARKING) || _state.test(STATUS_PARKING_POS))
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
        if (_state.test(STATUS_SLEWING_TO_TARGET))
        {
            status = F("SlewToTarget");
        }
        else if (_state.test(STATUS_SLEWING_FREE))
        {
            status = F("FreeSlew");
        }
        else if (_state.test(STATUS_SLEWING_MANUAL))
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
String Mount::getStatusString()
{
    String status = getStatusStateString() + ",";

    String disp = F("------,");
    if (_state.test(STATUS_SLEWING))
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
#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    if (_stepperAZ->isRunning())
    {
        disp[3] = _stepperAZ->targetPosition() < _stepperAZ->currentPosition() ? 'Z' : 'z';
    }
#endif
#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    if (_stepperALT->isRunning())
    {
        disp[4] = _stepperALT->targetPosition() < _stepperALT->currentPosition() ? 'A' : 'a';
    }
#endif

#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    if (_stepperFocus->isRunning())
        disp[5] = _stepperFocus->targetPosition() < _stepperFocus->currentPosition() ? 'F' : 'f';
#endif

    status += disp;
    status += String(_stepperRA->currentPosition()) + ",";
    status += String(_stepperDEC->currentPosition()) + ",";
    status += String(_stepperTRK->currentPosition()) + ",";

    status += RAString(COMPACT_STRING | CURRENT_STRING) + ",";
    status += DECString(COMPACT_STRING | CURRENT_STRING) + ",";
#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    status += String(_stepperFocus->currentPosition()) + ",";
#else
    status += ",";
#endif

    return status;
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
    if (_state.isParked())
    {
        return NOT_SLEWING;
    }
    if (isGuiding())
    {
        return NOT_SLEWING;
    }
    byte slewState = _stepperRA->isRunning() ? SLEWING_RA : NOT_SLEWING;
    slewState |= _stepperDEC->isRunning() ? SLEWING_DEC : NOT_SLEWING;

    slewState |= _state.test(STATUS_TRACKING) ? SLEWING_TRACKING : NOT_SLEWING;
    return slewState;
}

byte Mount::mountStatus() const
{
    return (byte) _state.value();
}

/////////////////////////////////
//
// isGuiding
//
/////////////////////////////////
bool Mount::isGuiding() const
{
    return _state.test(STATUS_GUIDE_PULSE);
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
    return _state.test(STATUS_PARKING | STATUS_PARKING_POS);
}

/////////////////////////////////
//
// isFindingHome
//
/////////////////////////////////
bool Mount::isFindingHome() const
{
    return _state.test(STATUS_FINDING_HOME);
}

core::MountActivityFacts Mount::activityFacts() const
{
    core::MountActivityFacts facts;
    facts.parking     = isParking();
    facts.tracking    = isSlewingTRK();
    facts.guiding     = isGuiding();
    facts.slewing     = _state.test(STATUS_SLEWING);
    facts.findingHome = isFindingHome();
    return facts;
}

bool Mount::admit(core::MountRequest request) const
{
    return core::MountRequests::admit(request, activityFacts()) == core::Admission::Allowed;
}

#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
void Mount::setRaMicrosteps(int microsteps)
{
    OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: RA driver setMicrosteps(%d)", microsteps);
    _driverRA->microsteps(core::microstepRegisterValue(microsteps));
}
#else
void Mount::setRaMicrosteps(int microsteps)
{
    // This board's driver reads its microsteps from pin strapping.
    (void) microsteps;
}
#endif

#if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
void Mount::setDecMicrosteps(int microsteps)
{
    OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: DEC driver setMicrosteps(%d)", microsteps);
    _driverDEC->microsteps(core::microstepRegisterValue(microsteps));
}
#else
void Mount::setDecMicrosteps(int microsteps)
{
    (void) microsteps;
}
#endif

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
            setRaMicrosteps(RA_TRACKING_MICROSTEPPING);
            _tracking.applySpeed();

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
                _tracking.noteStopped(_deps.clock->now());
                OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: startSlewing: stopped TRK at %ld", _tracking.stoppedAt());
            }

            // Change microstep mode for slewing
            setRaMicrosteps(RA_SLEW_MICROSTEPPING);
            // Normal state for DEC is guide microstepping, so switch to slew microstepping here.
            setDecMicrosteps(DEC_SLEW_MICROSTEPPING);

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
        _tracking.stop();
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
#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
        _stepperAZ->stop();
#endif
    }
    else if (axis == ALTITUDE_STEPS)
    {
#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
        _stepperALT->stop();
#endif
    }
    else if (axis == FOCUS_STEPS)
    {
#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
        _stepperFocus->stop();
#endif
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
           || ((direction & TRACKING) && ((!_state.test(STATUS_TRACKING)) && _stepperTRK->isRunning()))
#if FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE
           || ((direction & FOCUSING) && _stepperFocus->isRunning())
#endif
    )
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
    while (_stepperRA->isRunning() || _stepperDEC->isRunning() || ((!_state.test(STATUS_TRACKING)) && _stepperTRK->isRunning())
#if FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE
           || _stepperFocus->isRunning()
#endif
#if AZ_STEPPER_TYPE != STEPPER_TYPE_NONE
           || _stepperAZ->isRunning()
#endif
#if ALT_STEPPER_TYPE != STEPPER_TYPE_NONE
           || _stepperALT->isRunning()
#endif
    )
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
#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
        return _stepperFocus->currentPosition();
#endif
    }
    if (axis == StepperAxis::ALTITUDE_STEPS)
    {
#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
        return _stepperALT->currentPosition();
#endif
    }
    if (axis == StepperAxis::AZIMUTH_STEPS)
    {
#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
        return _stepperAZ->currentPosition();
#endif
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
        _raHoming          = new HallSensorHoming(this, axis, RA_HOMING_SENSOR_PIN, RA_HOMING_SENSOR_ACTIVE_STATE);
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
        _decHoming         = new HallSensorHoming(this, axis, DEC_HOMING_SENSOR_PIN, DEC_HOMING_SENSOR_ACTIVE_STATE);
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
#if USE_HALL_SENSOR_RA_AUTOHOME == 1
    if ((_raHoming != nullptr) && (!_raHoming->isIdleOrComplete()))
    {
        _raHoming->processHomingProgress();
        if (_raHoming->isIdleOrComplete())
        {
            publishEvent(core::MountEventType::HomingFinished, _raHoming->homingResult() == core::HomingResult::Succeeded);
        }
    }
#endif

#if USE_HALL_SENSOR_DEC_AUTOHOME == 1
    if ((_decHoming != nullptr) && (!_decHoming->isIdleOrComplete()))
    {
        _decHoming->processHomingProgress();
        if (_decHoming->isIdleOrComplete())
        {
            publishEvent(core::MountEventType::HomingFinished, _decHoming->homingResult() == core::HomingResult::Succeeded);
        }
    }
#endif
}

/// End switches are polled twice: once to debounce the raw pin, once to act on the
/// debounced state. On a board without them both calls compile out.
void Mount::pollEndSwitches()
{
#if (USE_RA_END_SWITCH == 1)
    _raEndSwitch->processEndSwitchState();
    _raEndSwitch->checkSwitchState();
#endif

#if (USE_DEC_END_SWITCH == 1)
    _decEndSwitch->processEndSwitchState();
    _decEndSwitch->checkSwitchState();
#endif
}

String Mount::getAutoHomingStates() const
{
    String state;
#if USE_HALL_SENSOR_RA_AUTOHOME == 1
    if ((_raHoming != nullptr) && (!_raHoming->isIdleOrComplete()))
    {
        state += _raHoming->getHomingState(_raHoming->getHomingState());
    }
    else
    {
        state += _raHoming->getLastResult();
    }
#endif
    state += "|";
#if USE_HALL_SENSOR_DEC_AUTOHOME == 1
    if ((_decHoming != nullptr) && (!_decHoming->isIdleOrComplete()))
    {
        state += _decHoming->getHomingState(_decHoming->getHomingState());
    }
    else
    {
        state += _decHoming->getLastResult();
    }
#endif
    return state;
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
    _raEndSwitch = new EndSwitch(
        this, StepperAxis::RA_STEPS, RA_ENDSWITCH_EAST_SENSOR_PIN, RA_ENDSWITCH_WEST_SENSOR_PIN, RA_END_SWITCH_ACTIVE_STATE);
    #endif

    #if (USE_DEC_END_SWITCH == 1)
    _decEndSwitch = new EndSwitch(
        this, StepperAxis::DEC_STEPS, DEC_ENDSWITCH_DOWN_SENSOR_PIN, DEC_ENDSWITCH_UP_SENSOR_PIN, DEC_END_SWITCH_ACTIVE_STATE);
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
    // Keeps the mount loop running, so tracking does not stop while waiting. The
    // deadline comes from the clock port and the yield from the scheduler port, so
    // this needs no direct millis()/yield().
    const unsigned long now = _deps.clock->now();
    while (_deps.clock->now() - now < (unsigned long) ms)
    {
        loop();
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
    if (_state.test(STATUS_GUIDE_PULSE) && _state.test(STATUS_TRACKING))
    {
        _stepperTRK->runSpeed();
        if (_state.test(STATUS_GUIDE_PULSE_DEC))
        {
            _stepperGUIDE->runSpeed();
        }
        return;
    }

    if (_state.test(STATUS_TRACKING))
    {
        _stepperTRK->runSpeed();
    }

    if (_state.test(STATUS_SLEWING))
    {
        if (_state.test(STATUS_SLEWING_MANUAL))
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
    else if (!_state.test(STATUS_TRACKING) && (_stepperTRK->isRunning()))
    {
        // If we are not tracking, but the tracking stepper is running, we need to let it move.
        // This can happen when we need to compensate for a slew (during which the tracker is
        // stopped). After the slew, the tracker is advanced by the distance it would have
        // travelled during the slew if it had been tracking.
        // That compensation uses goto mode (using runToNewPosition()), so we need to use run(),
        // since runSpeed() only advances the stepper when it is in constant speed mode.
        _stepperTRK->run();
    }

    if (_state.test(STATUS_FINDING_HOME))
    {
    #if USE_HALL_SENSOR_RA_AUTOHOME == 1
        _stepperRA->run();
        if (_raHoming != nullptr)
        {
            _raHoming->processHomingProgress();
        }
    #endif
    #if USE_HALL_SENSOR_DEC_AUTOHOME == 1
        _stepperDEC->run();
        if (_decHoming != nullptr)
        {
            _decHoming->processHomingProgress();
        }
    #endif
    }

    #if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    _stepperAZ->run();
    #endif
    #if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    _stepperALT->run();
    #endif

    #if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    // `FocusMode::Continuous` is kept from the original, which never selected it: a
    // continuous run targets a far position and therefore uses `run()`.
    if (_focus.mode() == core::FocusMode::ToTarget)
    {
        _stepperFocus->run();
    }
    else if (_focus.mode() == core::FocusMode::Continuous)
    {
        _stepperFocus->runSpeed();
    }
    #endif

    #if (USE_RA_END_SWITCH == 1)
    _raEndSwitch->processEndSwitchState();
    #endif

    #if (USE_DEC_END_SWITCH == 1)
    _decEndSwitch->processEndSwitchState();
    #endif
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
    // Facts published during the previous tick reach the display here, never from inside
    // the code that published them.
    _events.dispatch();

    bool raStillRunning  = false;
    bool decStillRunning = false;

    unsigned long now = _deps.clock->now();

#if (DEBUG_LEVEL & DEBUG_MOUNT) && (DEBUG_LEVEL & DEBUG_VERBOSE)
    if (now - _lastMountPrint > 2000)
    {
        OAT_LOG(_deps.logger, DEBUG_MOUNT, "[MOUNT]: Status -> %s", getStatusString().c_str());
        _lastMountPrint = now;
    }
#endif

#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE) || (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    bool oneIsRunning = false;
    #if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    oneIsRunning |= _stepperAZ->isRunning();
    #endif
    #if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    oneIsRunning |= _stepperALT->isRunning();
    #endif

    if (!oneIsRunning && _azAltWasRunning)
    {
        // One of the motors was running last time through the loop, but not anymore, so shutdown the outputs.
        disableAzAltMotors();
        _azAltWasRunning = false;
    #if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
        _deps.settings->storeAZPosition(_stepperAZ->currentPosition());
    #endif
    #if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
        _deps.settings->storeALTPosition(_stepperALT->currentPosition());
    #endif
    }

    oneIsRunning = false;
    #if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    oneIsRunning |= _stepperAZ->isRunning();
    #endif
    #if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    oneIsRunning |= _stepperALT->isRunning();
    #endif

    if (oneIsRunning)
    {
        _azAltWasRunning = true;
    }
#endif

#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    // LOG(DEBUG_MOUNT, "[MOUNT]: Focuser running:  %d", _stepperFocus->isRunning());

    if (_focus.isRunning())
    {
        OAT_LOG(_deps.logger, DEBUG_FOCUS, "[MOUNT]: Loop: Focuser running at speed %f", _focus.speed());
    }

    if (_focus.update() == core::FocusUpdate::Stopped)
    {
        OAT_LOG(_deps.logger, DEBUG_FOCUS, "[MOUNT]: Loop: Focuser is stopped, but was running ");
        OAT_LOG(_deps.logger, DEBUG_FOCUS, "[MOUNT]: Loop: Focuser is stopped, but was running, disabling");
        disableFocusMotor();
        publishEvent(core::MountEventType::FocusStopped);
    }

#endif

    if (isGuiding())
    {
        now                 = _deps.clock->now();
        bool stopRaGuiding  = _guiding.raExpired();
        bool stopDecGuiding = _guiding.decExpired();
        if (stopRaGuiding || stopDecGuiding)
        {
            OAT_LOG(_deps.logger,
                    DEBUG_GUIDE,
                    "[MOUNT]: Loop: StopGuiding. Now: %ld, RA End: %ld, DEC End: %ld",
                    now,
                    _guiding.raDeadline(),
                    _guiding.decDeadline());
            stopGuiding(stopRaGuiding, stopDecGuiding);
        }
        else
        {
            updateInfoDisplay();
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

    if (raStillRunning || decStillRunning)
    {
        displayStepperPositionThrottled();
    }
    else
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

        if (_slew.wasRunning())
        {
            publishEvent(core::MountEventType::SlewArrived);
            OAT_LOG(_deps.logger,
                    DEBUG_MOUNT | DEBUG_STEPPERS,
                    "[MOUNT]: Loop: Reached target. RA:%ld, DEC:%ld",
                    _stepperRA->currentPosition(),
                    _stepperDEC->currentPosition());
            // Mount is at Target!
            // If we we're parking, we just reached home. Clear the flag, reset the motors and stop tracking.
            if (isParking())
            {
                stopSlewing(TRACKING);
                // If we're on the second part of the slew to parking, don't set home here
                if (!_slew.headingToParkPosition())
                {
                    const core::SlewSnapshot savedSlew = _slew.snapshot();
                    OAT_LOG(_deps.logger,
                            DEBUG_MOUNT | DEBUG_STEPPERS,
                            "[MOUNT]: Loop:   Was Parking, not slewing to park so stop tracking and set home. SlewToHome: %d",
                            _slew.intent() == core::SlewIntent::Home);
                    setHome(false);
                    _slew.restore(savedSlew);
                }
                else
                {
                    OAT_LOG(_deps.logger,
                            DEBUG_MOUNT | DEBUG_STEPPERS,
                            "[MOUNT]: Loop:   Was Parking and slewing to Park, stop tracking. SlewToHome: %d",
                            _slew.intent() == core::SlewIntent::Home);
                }
                OAT_LOG(_deps.logger,
                        DEBUG_MOUNT | DEBUG_STEPPERS,
                        "[MOUNT]: Loop:   Slew2Park:%d, Slew2Home:%d",
                        _slew.headingToParkPosition(),
                        _slew.intent() == core::SlewIntent::Home);
            }

            _currentRAStepperPosition = _stepperRA->currentPosition();
            if (!isFindingHome())  // When finding home, we never want to switch back to tracking until homing is finished.
            {
                setRaMicrosteps(RA_TRACKING_MICROSTEPPING);
            }
            if (!isParking())
            {
                OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: Loop: Not parking");
                if (_tracking.compensationPending())
                {
                    now                          = _deps.clock->now();
                    const long compensationSteps = _tracking.compensationSteps(now);
                    OAT_LOG(_deps.logger,
                            DEBUG_STEPPERS,
                            "[STEPPERS]: loop: Arrived at %ldms. Tracking was off for %ldms (%ld steps), compensating.",
                            now,
                            now - _tracking.stoppedAt(),
                            compensationSteps);
                    _tracking.applyCompensation(compensationSteps);

                    OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: loop: compensation complete.");
                }

                if (!isFindingHome())  // If we're homing, RA must stay in Slew configuration
                {
                    OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: Loop: Not finding home, so start tracking");
                    startSlewing(TRACKING);
                }
            }

            // Reset DEC to guide microstepping so that guiding is always ready and no switch is neccessary on guide pulses.
            setDecMicrosteps(DEC_GUIDE_MICROSTEPPING);

            if (_slew.backlashOwed())
            {
                OAT_LOG(_deps.logger,
                        DEBUG_MOUNT | DEBUG_STEPPERS,
                        "[MOUNT]: Loop:   Reached target at %d. Compensating for backlash by %d",
                        (int) _currentRAStepperPosition,
                        _backlashCorrectionSteps);
                _currentRAStepperPosition += _backlashCorrectionSteps;
                _stepperRA->runToNewPosition(_currentRAStepperPosition);
                _slew.clearBacklashCorrection();
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

            OAT_LOG(_deps.logger,
                    DEBUG_MOUNT | DEBUG_STEPPERS,
                    "[MOUNT]: Loop:   Slew2Park:%d, Slew2Home:%d",
                    _slew.headingToParkPosition(),
                    _slew.intent() == core::SlewIntent::Home);
            if (_slew.intent() == core::SlewIntent::Home)
            {
                OAT_LOG(_deps.logger, DEBUG_MOUNT | DEBUG_STEPPERS, "[MOUNT]: Loop:   Was Slewing home...");
                _targetRA = currentRA();
                if (isParking())
                {
                    // Set DEC to slew microstepping, since it is set to guiding.
                    setDecMicrosteps(DEC_SLEW_MICROSTEPPING);
                    OAT_LOG(_deps.logger,
                            DEBUG_MOUNT | DEBUG_STEPPERS,
                            "[MOUNT]: Loop:   Was parking, so no tracking. Proceeding to park position...");
                    clearStatusFlag(STATUS_PARKING);
                    _slew.setHeadingToParkPosition(true);
                    _stepperRA->moveTo(-getHomingOffset(StepperAxis::RA_STEPS));
                    _stepperDEC->moveTo(-getHomingOffset(StepperAxis::DEC_STEPS));
                    _slew.setTotalMove(1.0f * _stepperRA->distanceToGo(), 1.0f * _stepperDEC->distanceToGo());
                    OAT_LOG(_deps.logger,
                            DEBUG_MOUNT | DEBUG_STEPPERS,
                            "[MOUNT]: Loop:   Park Position is R:%ld  D:%ld, TotalMove is R:%f, D:%f",
                            -getHomingOffset(StepperAxis::RA_STEPS),
                            -getHomingOffset(StepperAxis::DEC_STEPS),
                            _slew.totalRaSteps(),
                            _slew.totalDecSteps());
                    if ((_stepperDEC->distanceToGo() != 0) || (_stepperRA->distanceToGo() != 0))
                    {
                        OAT_LOG(_deps.logger,
                                DEBUG_MOUNT | DEBUG_STEPPERS,
                                "[MOUNT]: Loop:   Distance to Parking is non-zero, slewing to park position...");
                        setStatusFlag(STATUS_PARKING_POS | STATUS_SLEWING);
                    }
                    else
                    {
                        OAT_LOG(_deps.logger, DEBUG_MOUNT | DEBUG_STEPPERS, "[MOUNT]: Loop:   Already at Parking pos, so done.");
                        noInterrupts();
                        setStatus(STATUS_PARKED);
                        interrupts();
                        publishEvent(core::MountEventType::Parked);
                    }
                }
                else
                {
                    OAT_LOG(_deps.logger, DEBUG_MOUNT | DEBUG_STEPPERS, "[MOUNT]: Loop:   Slewed home, not parking, so restart tracking.");
                    startSlewing(TRACKING);
                }
                _slew.end();
                // Reset DEC to guide microstepping so that guiding is always ready and no switch is neccessary on guide pulses.
                setDecMicrosteps(DEC_GUIDE_MICROSTEPPING);
            }
            else if (_slew.headingToParkPosition())
            {
                OAT_LOG(_deps.logger, DEBUG_MOUNT | DEBUG_STEPPERS, "[MOUNT]: Loop:   Arrived at park position...");
                noInterrupts();
                setStatus(STATUS_PARKED);
                interrupts();
                publishEvent(core::MountEventType::Parked);
                _slew.setHeadingToParkPosition(false);
            }
            _slew.end();
            _slew.clearTotalMove();

            // Make sure we do one last update when the steppers have stopped.
            displayStepperPosition();
        }
    }

    if (_state.test(STATUS_FINDING_HOME))
    {
        processHomingProgress();
    }

    pollEndSwitches();

    _slew.noteRunning(raStillRunning || decStillRunning);
    updateInfoDisplay();
}

#if (INFO_DISPLAY_TYPE != INFO_DISPLAY_TYPE_NONE)
void Mount::setupInfoDisplay()
{
    #if (INFO_DISPLAY_TYPE == INFO_DISPLAY_TYPE_I2C_SSD1306_128x64)
    OAT_LOG(_deps.logger, DEBUG_DISPLAY, "[DISPLAY]: Create SSD1306 OLED class...");
    infoDisplay = new SDD1306OLED128x64(INFO_DISPLAY_I2C_ADDRESS, INFO_DISPLAY_I2C_SDA_PIN, INFO_DISPLAY_I2C_SCL_PIN);
    OAT_LOG(_deps.logger, DEBUG_ANY, "[SYSTEM]: SSD1306 OLED created... initializing");
    infoDisplay->init();
    infoDisplay->setConsoleMode(true);
    OAT_LOG(_deps.logger, DEBUG_DISPLAY, "[DISPLAY]: Created and initialized SSD1306 OLED class...");
    #endif
}
#endif

void Mount::updateInfoDisplay()
{
#if (INFO_DISPLAY_TYPE == INFO_DISPLAY_TYPE_NONE)
    // No panel on this board: the call sites below stay unconditional so the mount code
    // does not have to know whether anything is listening.
    return;
#else
    // If we update this display too often while slewing, the serial port is unable to process commands fast enough. Which makes the driver
    // timeout, causing ASCOM errors.
    // We will update at 30Hz when idle, 5Hz when slewing one axis and skip updates when slewing both.

    int refreshRateHz = 30;
    #if defined(ARDUINO_ARCH_RP2040)
    const bool isSlewingAny = (slewStatus() & (SLEWING_DEC | SLEWING_RA)) != 0;
    refreshRateHz           = isSlewingAny ? 15 : 30;
    #else
    const bool isSlewingRAandDEC = (slewStatus() & (SLEWING_DEC | SLEWING_RA)) == (SLEWING_DEC | SLEWING_RA);
    if (isSlewingRAandDEC)
    {
        return;  // Do not update the display
    }
    else if (isSlewingRAorDEC())
    {
        refreshRateHz = 5;  // Update the display slower
    }
    #endif
    const long now = _deps.clock->now();
    if (now - _lastInfoUpdate > (1000 / refreshRateHz))
    {
        OAT_LOG(_deps.logger, DEBUG_DISPLAY, "[DISPLAY]: Render state to OLED ...");
        infoDisplay->renderScreen((void *) this);
        OAT_LOG(_deps.logger, DEBUG_DISPLAY, "[DISPLAY]: Rendered state to OLED ...");
        _lastInfoUpdate = now;
    }
#endif
}

#if (INFO_DISPLAY_TYPE != INFO_DISPLAY_TYPE_NONE)
InfoDisplayRender *Mount::getInfoDisplay()
{
    return infoDisplay;
}
#endif

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
    _slew.end();
    _slew.setHeadingToParkPosition(false);

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
        return _tracking.speed();
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
            currentRA().ToString(),
            currentDEC().ToString());
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
    _slew.clearBacklashCorrection();
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
            _slew.requestBacklashCorrection();
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
                    _tracking.noteStopped(_deps.clock->now());
                    OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: moveStepperBy: TRK stopped at %ldms", _tracking.stoppedAt());
                }
            }

            // Slew microsteps once the TRK stepper has stopped.
            setRaMicrosteps(RA_SLEW_MICROSTEPPING);
            setStatusFlag(STATUS_SLEWING | STATUS_SLEWING_TO_TARGET);
            publishEvent(core::MountEventType::SlewStarted);
            _slew.noteRunning(true);
            moveSteppersTo(_stepperRA->currentPosition() + steps, 0, direction);
            _slew.setTotalMove(1.0f * _stepperRA->distanceToGo(), _slew.totalDecSteps());
            break;

        case DEC_STEPS:
            {
                setStatusFlag(STATUS_SLEWING | STATUS_SLEWING_TO_TARGET);
                publishEvent(core::MountEventType::SlewStarted);
                // Normal state for DEC is guide microstepping, so switch to slew microstepping here.
                setDecMicrosteps(DEC_SLEW_MICROSTEPPING);
                _slew.noteRunning(true);
                moveSteppersTo(0, _stepperDEC->currentPosition() + steps, direction);
                _slew.setTotalMove(_slew.totalRaSteps(), 1.0f * _stepperDEC->distanceToGo());
            }
            break;

        case FOCUS_STEPS:
            {
#if FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE
                focusMoveBy(steps);
#endif
            }
            break;

        case AZIMUTH_STEPS:
            {
#if AZ_STEPPER_TYPE != STEPPER_TYPE_NONE
                enableAzMotor();
                OAT_LOG(_deps.logger,
                        DEBUG_STEPPERS,
                        "[STEPPERS]: moveStepperBy: AZ from %ld to %ld",
                        _stepperAZ->currentPosition(),
                        _stepperAZ->currentPosition() + steps);
                _stepperAZ->moveTo(_stepperAZ->currentPosition() + steps);
#endif
            }
            break;

        case ALTITUDE_STEPS:
            {
#if ALT_STEPPER_TYPE != STEPPER_TYPE_NONE
                enableAltMotor();
                _stepperALT->moveTo(_stepperALT->currentPosition() + steps);
#endif
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
// getStepperProgress
//
/////////////////////////////////
bool Mount::getStepperProgress(int &raPercentage, int &decPercentage)
{
    bool slewInProgress = true;
    decPercentage       = 100;
    raPercentage        = 100;
    if ((fabsf(_slew.totalDecSteps()) > 0.001f) && (fabsf(_slew.totalRaSteps()) > 0.001f))
    {
        // Both axes moving to target
        decPercentage = (int) round(100.0f - 100.0f * _stepperDEC->distanceToGo() / _slew.totalDecSteps());
        raPercentage  = (int) round(100.0f - 100.0f * _stepperRA->distanceToGo() / _slew.totalRaSteps());
    }
    else if (fabsf(_slew.totalDecSteps()) > 0.001f)
    {
        // Only DEC moving to target
        decPercentage = (int) round(100.0f - 100.0f * _stepperDEC->distanceToGo() / _slew.totalDecSteps());
    }
    else if (fabsf(_slew.totalRaSteps()) > 0.001f)
    {
        // Only RA is moving to target
        raPercentage = (int) round(100.0f - 100.0f * _stepperRA->distanceToGo() / _slew.totalRaSteps());
    }
    else
    {
        // Nothing is slewing
        slewInProgress = false;
    }
    return slewInProgress;
}
/////////////////////////////////
//
// displayStepperPosition
//
/////////////////////////////////
void Mount::displayStepperPosition()
{
#if DISPLAY_TYPE > 0

    String disp;

    if ((fabsf(_slew.totalDecSteps()) > 0.001f) && (fabsf(_slew.totalRaSteps()) > 0.001f))
    {
        // Both axes moving to target
        float decDist = 100.0f - 100.0f * _stepperDEC->distanceToGo() / _slew.totalDecSteps();
        float raDist  = 100.0f - 100.0f * _stepperRA->distanceToGo() / _slew.totalRaSteps();

        OAT_SNPRINTF(scratchBuffer, sizeof(scratchBuffer), "R %s %d%%", RAString(LCD_STRING | CURRENT_STRING).c_str(), (int) raDist);
        _lcdMenu->setCursor(0, 0);
        _lcdMenu->printMenu(String(scratchBuffer));
        OAT_SNPRINTF(scratchBuffer, sizeof(scratchBuffer), "D%s %d%%", DECString(LCD_STRING | CURRENT_STRING).c_str(), (int) decDist);
        _lcdMenu->setCursor(0, 1);
        _lcdMenu->printMenu(String(scratchBuffer));
        return;
    }

    if (fabsf(_slew.totalDecSteps()) > 0.001f)
    {
        // Only DEC moving to target
        float decDist = 100.0f - 100.0f * _stepperDEC->distanceToGo() / _slew.totalDecSteps();
        OAT_SNPRINTF(scratchBuffer, sizeof(scratchBuffer), "D%s %d%%", DECString(LCD_STRING | CURRENT_STRING).c_str(), (int) decDist);
        _lcdMenu->setCursor(0, 1);
        _lcdMenu->printMenu(String(scratchBuffer));
    }
    else if (fabsf(_slew.totalRaSteps()) > 0.001f)
    {
        // Only RAmoving to target
        float raDist = 100.0f - 100.0f * _stepperRA->distanceToGo() / _slew.totalRaSteps();
        OAT_SNPRINTF(scratchBuffer, sizeof(scratchBuffer), "R %s %d%%", RAString(LCD_STRING | CURRENT_STRING).c_str(), (int) raDist);
        disp = disp + String(scratchBuffer);
        _lcdMenu->setCursor(0, inSerialControl ? 0 : 1);
        _lcdMenu->printMenu(String(scratchBuffer));
    }
    else
    {
        // Nothing moving
    #if SUPPORT_SERIAL_CONTROL == 1
        if (inSerialControl)
        {
            OAT_SNPRINTF(scratchBuffer, sizeof(scratchBuffer), " RA: %s", RAString(LCD_STRING | CURRENT_STRING).c_str());
            _lcdMenu->setCursor(0, 0);
            _lcdMenu->printMenu(scratchBuffer);
            OAT_SNPRINTF(scratchBuffer, sizeof(scratchBuffer), "DEC: %s", DECString(LCD_STRING | CURRENT_STRING).c_str());
            _lcdMenu->setCursor(0, 1);
            _lcdMenu->printMenu(scratchBuffer);
        }
        else
        {
            OAT_SNPRINTF(scratchBuffer,
                         sizeof(scratchBuffer),
                         "R%s D%s",
                         RAString(COMPACT_STRING | CURRENT_STRING).c_str(),
                         DECString(COMPACT_STRING | CURRENT_STRING).c_str());
            _lcdMenu->setCursor(0, 1);
            _lcdMenu->printMenu(scratchBuffer);
        }
    #else
        OAT_SNPRINTF(scratchBuffer,
                     sizeof(scratchBuffer),
                     "R%s D%s",
                     RAString(COMPACT_STRING | CURRENT_STRING).c_str(),
                     DECString(COMPACT_STRING | CURRENT_STRING).c_str());
        _lcdMenu->setCursor(0, 1);
        _lcdMenu->printMenu(scratchBuffer);
    #endif
    }
#endif
}

/////////////////////////////////
//
// displayStepperPositionThrottled
//
/////////////////////////////////
void Mount::displayStepperPositionThrottled()
{
#if DISPLAY_TYPE > 0
    long elapsed = _deps.clock->now() - _lastDisplayUpdate;
    if (elapsed > DISPLAY_UPDATE_TIME)
    {
        displayStepperPosition();
        _lastDisplayUpdate = _deps.clock->now();
    }
#endif
}

/////////////////////////////////
//
// DECString
//
// Return a string of DEC in the given format. For LCDSTRING, active determines where the cursor is
/////////////////////////////////
String Mount::DECString(byte type, byte active)
{
    Declination dec;
    if ((type & TARGET_STRING) == TARGET_STRING)
    {
        dec = _targetDEC;
    }
    else
    {
        dec = currentDEC();
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
DayTime Mount::getUtcTime()
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
DayTime Mount::getLocalTime()
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
void Mount::setLocalStartTime(DayTime localTime)
{
    _localStartTime          = localTime;
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
DayTime Mount::calculateLst()
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
DayTime Mount::calculateHa()
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
    OAT_LOG(
        _deps.logger, DEBUG_STEPPERS, "[STEPPERS]: uartTest: Switching RA driver to microsteps(%d) for UART test", RA_SLEW_MICROSTEPPING);
    setRaMicrosteps(RA_SLEW_MICROSTEPPING);
    testUART_vactual(_driverRA, speed, duration);
    OAT_LOG(_deps.logger,
            DEBUG_STEPPERS,
            "[STEPPERS]: uartTest: Switching RA driver to microsteps(%d) after UART test",
            RA_TRACKING_MICROSTEPPING);
    setRaMicrosteps(RA_TRACKING_MICROSTEPPING);
}
    #endif

    #if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
void Mount::testDEC_UART_TX()
{
    //microsteps per driver clock tick (From TMC2209 datasheet: v[Hz] (microstep/s) = VACTUAL[2209] * 0.715Hz)
    const int speed = (DEC_STEPPER_SPEED / 2) / 0.715255737f;
    //Duration in ms to move X degrees at half of the max speed
    const int duration = UART_CONNECTION_TEST_TX_DEG * (_stepsPerDECDegree / (DEC_STEPPER_SPEED / 2)) * 1000;
    OAT_LOG(
        _deps.logger, DEBUG_STEPPERS, "[STEPPERS]: uartTest: Switching DEC driver to microsteps(%d) for UART test", DEC_SLEW_MICROSTEPPING);
    setDecMicrosteps(DEC_SLEW_MICROSTEPPING);
    testUART_vactual(_driverDEC, speed, duration);
    OAT_LOG(_deps.logger,
            DEBUG_STEPPERS,
            "[STEPPERS]: uartTest: Switching DEC driver to microsteps(%d) after UART test",
            DEC_GUIDE_MICROSTEPPING);
    setDecMicrosteps(DEC_GUIDE_MICROSTEPPING);
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
    return _tracking.trackedHours();
}

/////////////////////////////////
//
// checkRALimit
//
/////////////////////////////////
float Mount::checkRALimit()
{
    const float trackedHours = getTrackedHours();
    const float homeRA       = _zeroPosRA.getTotalHours() + trackedHours;
    const float RALimit      = RA_TRACKING_LIMIT;
    OAT_LOG(_deps.logger,
            DEBUG_MOUNT_VERBOSE,
            "[MOUNT]: checkRALimit: homeRA: %f (ZeroPos: %f + TrkHrs: %f)",
            homeRA,
            _zeroPosRA.getTotalHours(),
            trackedHours);
    const float degreePos = (_stepperDEC->currentPosition() / _stepsPerDECDegree) + _zeroPosDEC;
    float hourPos         = currentRA().getTotalHours();
    OAT_LOG(_deps.logger, DEBUG_MOUNT_VERBOSE, "[MOUNT]: checkRALimit: degreePosDec: %f , RA hourpos : %f)", degreePos, hourPos);
    if (inNorthernHemisphere ? degreePos < 0 : degreePos > 0)
    {
        hourPos -= 12;
        if (hourPos < 0)
            hourPos += 24;
        OAT_LOG(_deps.logger, DEBUG_MOUNT_VERBOSE, "[MOUNT]: checkRALimit: switching RA hourPos to: %f", hourPos);
    }
    OAT_LOG(_deps.logger, DEBUG_MOUNT_VERBOSE, "[MOUNT]: checkRALimit: RA hourPos (adjusted): %f", hourPos);
    float homeCurrentDeltaRA = homeRA - hourPos;
    OAT_LOG(
        _deps.logger, DEBUG_MOUNT_VERBOSE, "[MOUNT]: checkRALimit: DeltaRA: %f (home:%f - hour:%f)", homeCurrentDeltaRA, homeRA, hourPos);
    while (homeCurrentDeltaRA > 12)
        homeCurrentDeltaRA -= 24;
    while (homeCurrentDeltaRA < -12)
        homeCurrentDeltaRA += 24;
    OAT_LOG(_deps.logger, DEBUG_MOUNT_VERBOSE, "[MOUNT]: checkRALimit: deltaRA: %f => Check against %f", homeCurrentDeltaRA, RALimit);

    if (homeCurrentDeltaRA > RALimit)
    {
        OAT_LOG(_deps.logger,
                DEBUG_MOUNT,
                "[MOUNT]: checkRALimit: Tracking limit reached. deltaRA: %f > RALimit:%f.  TrackedHrs:%f, HomeRA:%f, ",
                homeCurrentDeltaRA,
                RALimit,
                trackedHours,
                homeRA);
        stopSlewing(TRACKING);
    }
    _lastTRKCheck = _deps.clock->now();

    return RALimit - homeCurrentDeltaRA;
}
