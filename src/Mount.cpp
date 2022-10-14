#include "../Configuration.hpp"
#include "Utility.hpp"
#include "EPROMStore.hpp"
#include "LcdMenu.hpp"
#include "Mount.hpp"
#include "Sidereal.hpp"
#include "libs/MappedDict/MappedDict.hpp"

PUSH_NO_WARNINGS
#include <AccelStepper.h>
#if (RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART) || (DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)                                          \
    || (AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART) || (ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)                                       \
    || (FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
    #include <TMCStepper.h>  // If you get an error here, download the TMCstepper library from "Tools > Manage Libraries"
#endif
POP_NO_WARNINGS

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

// slewingStatus()
#define SLEWING_DEC      B00000010
#define SLEWING_RA       B00000001
#define SLEWING_BOTH     B00000011
#define SLEWING_TRACKING B00001000
#define NOT_SLEWING      B00000000

// slewStatus
#define SLEW_MASK_DEC   B0011
#define SLEW_MASK_NORTH B0001
#define SLEW_MASK_SOUTH B0010
#define SLEW_MASK_RA    B1100
#define SLEW_MASK_EAST  B0100
#define SLEW_MASK_WEST  B1000
#define SLEW_MASK_ANY   B1111

#define UART_CONNECTION_TEST_RETRIES 5

// Seconds per astronomical day (23h 56m 4.0905s)
const float secondsPerDay = 86164.0905f;

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
Mount::Mount(LcdMenu *lcdMenu)
#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE) || (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    : _azAltWasRunning(false)
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
    _lcdMenu = lcdMenu;
    initializeVariables();
}

void Mount::initializeVariables()
{
    _stepsPerRADegree  = RA_STEPS_PER_DEGREE;   // u-steps per degree when slewing
    _stepsPerDECDegree = DEC_STEPS_PER_DEGREE;  // u-steps per degree when slewing

    _mountStatus       = 0;
    _lastDisplayUpdate = 0;
    _stepperWasRunning = false;
    _latitude          = Latitude(45.0);
    _longitude         = Longitude(100.0);
    _zeroPosDEC        = 0.0f;

    _compensateForTrackerOff = false;
    _trackerStoppedAt        = 0;

    _totalDECMove  = 0;
    _totalRAMove   = 0;
    _homeOffsetRA  = 0;
    _homeOffsetDEC = 0;
#if USE_HALL_SENSOR_RA_AUTOHOME == 1
    _homing.state = HomingState::HOMING_NOT_ACTIVE;
#endif
    _moveRate                = 4;
    _backlashCorrectionSteps = 0;
    _correctForBacklash      = false;
    _slewingToHome           = false;
    _slewingToPark           = false;
    _raParkingPos            = 0;
    _decParkingPos           = 0;
    _decLowerLimit           = 0;
    _decUpperLimit           = 0;

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
    EEPROMStore::clearConfiguration();
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
    LOG(DEBUG_INFO, "[MOUNT]: Reading configuration data from EEPROM");
    readPersistentData();
    LOG(DEBUG_INFO, "[MOUNT]: Done reading configuration data from EEPROM");
}

/////////////////////////////////
//
// readPersistentData
//
/////////////////////////////////
void Mount::readPersistentData()
{
    // EEPROMStore will always return valid data, even if no data is present in the store

    _stepsPerRADegree = EEPROMStore::getRAStepsPerDegree();
    LOG(DEBUG_INFO, "[MOUNT]: EEPROM: RA steps/deg is %f", _stepsPerRADegree);

    _stepsPerDECDegree = EEPROMStore::getDECStepsPerDegree();
    LOG(DEBUG_INFO, "[MOUNT]: EEPROM: DEC steps/deg is %f", _stepsPerDECDegree);

    float speed = EEPROMStore::getSpeedFactor();
    LOG(DEBUG_INFO, "[MOUNT]: EEPROM: Speed factor is %f", speed);
    setSpeedCalibration(speed, false);

    _backlashCorrectionSteps = EEPROMStore::getBacklashCorrectionSteps();
    LOG(DEBUG_INFO, "[MOUNT]: EEPROM: Backlash correction is %d", _backlashCorrectionSteps);

    _latitude = EEPROMStore::getLatitude();
    LOG(DEBUG_INFO, "[MOUNT]: EEPROM: Latitude is %s", _latitude.ToString());

    _longitude = EEPROMStore::getLongitude();
    LOG(DEBUG_INFO, "[MOUNT]: EEPROM: Longitude is %s", _longitude.ToString());

    _localUtcOffset = EEPROMStore::getUtcOffset();
    LOG(DEBUG_INFO, "[MOUNT]: EEPROM: UTC offset is %d", _localUtcOffset);

#if USE_GYRO_LEVEL == 1
    _pitchCalibrationAngle = EEPROMStore::getPitchCalibrationAngle();
    LOG(DEBUG_INFO, "[MOUNT]: EEPROM: Pitch Offset is %f", _pitchCalibrationAngle);

    _rollCalibrationAngle = EEPROMStore::getRollCalibrationAngle();
    LOG(DEBUG_INFO, "[MOUNT]: EEPROM: Roll Offset is %f", _rollCalibrationAngle);
#endif

    _raParkingPos  = EEPROMStore::getRAParkingPos();
    _decParkingPos = EEPROMStore::getDECParkingPos();
    LOG(DEBUG_INFO, "[MOUNT]: EEPROM: Parking position read as R:%l, D:%l", _raParkingPos, _decParkingPos);

    _decLowerLimit = static_cast<long>(-(EEPROMStore::getDECLowerLimit() * _stepsPerDECDegree));
    if (_decLowerLimit == 0 && DEC_LIMIT_DOWN != 0)
    {
        _decLowerLimit = static_cast<long>(-(DEC_LIMIT_DOWN * _stepsPerDECDegree));
    }
    _decUpperLimit = static_cast<long>((EEPROMStore::getDECUpperLimit() * _stepsPerDECDegree));
    if (_decUpperLimit == 0 && DEC_LIMIT_UP != 0)
    {
        _decUpperLimit = static_cast<long>(DEC_LIMIT_UP * _stepsPerDECDegree);
    }
    LOG(DEBUG_INFO, "[MOUNT]: EEPROM: DEC limits read as %l -> %l", _decLowerLimit, _decUpperLimit);

#if USE_HALL_SENSOR_RA_AUTOHOME == 1
    _homing.offsetRA = EEPROMStore::getRAHomingOffset();
    LOG(DEBUG_INFO, "[MOUNT]: EEPROM: RA Homing offset read as %l", _homing.offsetRA);
#endif
#if DEW_HEATER == 1
    unsigned heaterValues[2];
    EEPROMStore::getHeaterValues(heaterValues);
    LOG(DEBUG_INFO, "[MOUNT]: EEPROM: heater values read as %d %d", heaterValues[0], heaterValues[1]);
    #if defined(DEW_HEATER_1_PIN)
    _heaters[0] = new Heater(0, DEW_HEATER_1_PIN, DEW_HEATER_1_MAX);
    _heaters[0]->setValue(heaterValues[0]);
    #endif
    #if defined(DEW_HEATER_2_PIN)
    _heaters[1] = new Heater(1, DEW_HEATER_2_PIN, DEW_HEATER_2_MAX);
    _heaters[1]->setValue(heaterValues[1]);
    #endif
#endif
}

/////////////////////////////////
//
// configureRAStepper
//
/////////////////////////////////
void Mount::configureRAStepper(byte pin1, byte pin2, int maxSpeed, int maxAcceleration)
{
    _stepperRA = new AccelStepper(AccelStepper::DRIVER, pin1, pin2);
    _stepperRA->setMaxSpeed(maxSpeed);
    _stepperRA->setAcceleration(maxAcceleration);
    _maxRASpeed        = maxSpeed;
    _maxRAAcceleration = maxAcceleration;

    // Use another AccelStepper to run the RA motor as well. This instance tracks earths rotation.
    _stepperTRK = new AccelStepper(AccelStepper::DRIVER, pin1, pin2);

    _stepperTRK->setMaxSpeed(2000);
    _stepperTRK->setAcceleration(15000);

    _stepperRA->setPinsInverted(NORTHERN_HEMISPHERE == RA_INVERT_DIR, false, false);
    _stepperTRK->setPinsInverted(NORTHERN_HEMISPHERE == RA_INVERT_DIR, false, false);
}

/////////////////////////////////
//
// configureDECStepper
//
/////////////////////////////////
void Mount::configureDECStepper(byte pin1, byte pin2, int maxSpeed, int maxAcceleration)
{
    _stepperDEC = new AccelStepper(AccelStepper::DRIVER, pin1, pin2);
    _stepperDEC->setMaxSpeed(maxSpeed);
    _stepperDEC->setAcceleration(maxAcceleration);
    _maxDECSpeed        = maxSpeed;
    _maxDECAcceleration = maxAcceleration;

    // Use another AccelStepper to run the DEC motor as well. This instance is used for guiding.
    _stepperGUIDE = new AccelStepper(AccelStepper::DRIVER, pin1, pin2);

    _stepperGUIDE->setMaxSpeed(2000);
    _stepperGUIDE->setAcceleration(15000);

#if DEC_INVERT_DIR == 1
    _stepperDEC->setPinsInverted(true, false, false);
    _stepperGUIDE->setPinsInverted(true, false, false);
#endif
}

/////////////////////////////////
//
// configureAZStepper / configureALTStepper
//
/////////////////////////////////
#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
void Mount::configureAZStepper(byte pin1, byte pin2, int maxSpeed, int maxAcceleration)
{
    _stepperAZ = new AccelStepper(AccelStepper::DRIVER, pin1, pin2);
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
    _stepperALT = new AccelStepper(AccelStepper::DRIVER, pin1, pin2);
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
    _stepperFocus = new AccelStepper(AccelStepper::DRIVER, pin1, pin2);
    _stepperFocus->setMaxSpeed(maxSpeed);
    _stepperFocus->setAcceleration(maxAcceleration);
    _stepperFocus->setSpeed(0);
    _stepperFocus->setCurrentPosition(50000);
    _maxFocusSpeed        = maxSpeed;
    _maxFocusAcceleration = maxAcceleration;
    _maxFocusRateSpeed    = maxSpeed;
}
#endif

#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART || DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART                                              \
    || AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART || ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART                                           \
    || FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    #if UART_CONNECTION_TEST_TXRX == 1
bool Mount::connectToDriver(TMC2209Stepper *driver, const char *driverKind)
{
    LOG(DEBUG_STEPPERS, "[STEPPERS]: Testing UART Connection to %s driver...", driverKind);
    for (int i = 0; i < UART_CONNECTION_TEST_RETRIES; i++)
    {
        if (driver->test_connection() == 0)
        {
            LOG(DEBUG_STEPPERS, "[STEPPERS]: UART connection to %s driver successful.", driverKind);
            return true;
        }
        else
        {
            delay(500);
        }
    }
    LOG(DEBUG_STEPPERS, "[STEPPERS]: UART connection to %s driver failed.", driverKind);
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
    _driverRA = new TMC2209Stepper(serial, rsense, driveraddress);
    _driverRA->begin();
    bool UART_Rx_connected = false;
        #if UART_CONNECTION_TEST_TXRX == 1
    UART_Rx_connected = connectToDriver(_driverRA, "RA");
    if (!UART_Rx_connected)
    {
        digitalWrite(RA_EN_PIN,
                     HIGH);  //Disable motor for safety reasons if UART connection fails to avoid operating at incorrect rms_current
    }
        #endif
    _driverRA->toff(0);
        #if USE_VREF == 0  //By default, Vref is ignored when using UART to specify rms current.
    _driverRA->I_scale_analog(false);
        #endif
    LOG(DEBUG_STEPPERS, "[MOUNT]: Requested RA motor rms_current: %d mA", rmscurrent);
    _driverRA->rms_current(rmscurrent, 1.0f);  //holdMultiplier = 1 to set ihold = irun
    _driverRA->toff(1);
    _driverRA->en_spreadCycle(RA_UART_STEALTH_MODE == 0);
    _driverRA->blank_time(24);
    _driverRA->microsteps(RA_TRACKING_MICROSTEPPING == 1 ? 0 : RA_TRACKING_MICROSTEPPING);  // System starts in tracking mode
    _driverRA->fclktrim(4);
    _driverRA->TCOOLTHRS(0xFFFFF);  //xFFFFF);
    _driverRA->semin(0);            //disable CoolStep so that current is consistent
    _driverRA->SGTHRS(stallvalue);
    if (UART_Rx_connected)
    {
        LOG(DEBUG_STEPPERS, "[MOUNT]: Actual RA motor rms_current: %d mA", _driverRA->rms_current());
        LOG(DEBUG_STEPPERS, "[MOUNT]: Actual RA CS value: %d", _driverRA->cs_actual());
        LOG(DEBUG_STEPPERS, "[MOUNT]: Actual RA vsense: %d", _driverRA->vsense());
    }
}

    #elif SW_SERIAL_UART == 1

void Mount::configureRAdriver(uint16_t RA_SW_RX, uint16_t RA_SW_TX, float rsense, byte driveraddress, int rmscurrent, int stallvalue)
{
    _driverRA = new TMC2209Stepper(RA_SW_RX, RA_SW_TX, rsense, driveraddress);
    _driverRA->beginSerial(19200);
    _driverRA->mstep_reg_select(true);
    _driverRA->pdn_disable(true);
    bool UART_Rx_connected = false;
        #if UART_CONNECTION_TEST_TXRX == 1
    UART_Rx_connected      = connectToDriver(_driverRA, "RA");
    if (!UART_Rx_connected)
    {
        digitalWrite(RA_EN_PIN,
                     HIGH);  //Disable motor for safety reasons if UART connection fails to avoid operating at incorrect rms_current
    }
        #endif
    _driverRA->toff(0);
        #if USE_VREF == 0  //By default, Vref is ignored when using UART to specify rms current.
    _driverRA->I_scale_analog(false);
        #endif
    LOG(DEBUG_STEPPERS, "[MOUNT]: Requested RA motor rms_current: %d mA", rmscurrent);
    _driverRA->rms_current(rmscurrent, 1.0f);  //holdMultiplier = 1 to set ihold = irun
    _driverRA->toff(1);
    _driverRA->en_spreadCycle(RA_UART_STEALTH_MODE == 0);
    _driverRA->blank_time(24);
    _driverRA->semin(0);                                                                    //disable CoolStep so that current is consistent
    _driverRA->microsteps(RA_TRACKING_MICROSTEPPING == 1 ? 0 : RA_TRACKING_MICROSTEPPING);  // System starts in tracking mode
    _driverRA->fclktrim(4);
    _driverRA->TCOOLTHRS(0xFFFFF);  //xFFFFF);
    _driverRA->SGTHRS(stallvalue);
    if (UART_Rx_connected)
    {
        LOG(DEBUG_STEPPERS, "[MOUNT]: Actual RA motor rms_current: %d mA", _driverRA->rms_current());
        LOG(DEBUG_STEPPERS, "[MOUNT]: Actual RA CS value: %d", _driverRA->cs_actual());
        LOG(DEBUG_STEPPERS, "[MOUNT]: Actual RA vsense: %d", _driverRA->vsense());
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
    _driverDEC = new TMC2209Stepper(serial, rsense, driveraddress);
    _driverDEC->begin();
    bool UART_Rx_connected = false;
        #if UART_CONNECTION_TEST_TXRX == 1
    UART_Rx_connected = connectToDriver(_driverDEC, "DEC");
    if (!UART_Rx_connected)
    {
        digitalWrite(DEC_EN_PIN,
                     HIGH);  //Disable motor for safety reasons if UART connection fails to avoid operating at incorrect rms_current
    }
        #endif
    _driverDEC->toff(0);
        #if USE_VREF == 0  //By default, Vref is ignored when using UART to specify rms current.
    _driverDEC->I_scale_analog(false);
        #endif
    LOG(DEBUG_STEPPERS, "[MOUNT]: Requested DEC motor rms_current: %d mA", rmscurrent);
    _driverDEC->rms_current(rmscurrent, 1.0f);  //holdMultiplier = 1 to set ihold = irun
    _driverDEC->toff(1);
    _driverDEC->en_spreadCycle(DEC_UART_STEALTH_MODE == 0);
    _driverDEC->blank_time(24);
    _driverDEC->microsteps(
        DEC_GUIDE_MICROSTEPPING == 1 ? 0 : DEC_GUIDE_MICROSTEPPING);  // If 1 then disable microstepping. Start with Guide microsteps.
    _driverDEC->TCOOLTHRS(0xFFFFF);
    _driverDEC->semin(0);  //disable CoolStep so that current is consistent
    _driverDEC->SGTHRS(stallvalue);
    if (UART_Rx_connected)
    {
        LOG(DEBUG_STEPPERS, "[MOUNT]: Actual DEC motor rms_current: %d mA", _driverDEC->rms_current());
        LOG(DEBUG_STEPPERS, "[MOUNT]: Actual DEC CS value: %d", _driverDEC->cs_actual());
        LOG(DEBUG_STEPPERS, "[MOUNT]: Actual DEC vsense: %d", _driverDEC->vsense());
    }
}

    #elif SW_SERIAL_UART == 1

void Mount::configureDECdriver(uint16_t DEC_SW_RX, uint16_t DEC_SW_TX, float rsense, byte driveraddress, int rmscurrent, int stallvalue)
{
    _driverDEC = new TMC2209Stepper(DEC_SW_RX, DEC_SW_TX, rsense, driveraddress);
    _driverDEC->beginSerial(19200);
    _driverDEC->mstep_reg_select(true);
    _driverDEC->pdn_disable(true);
    bool UART_Rx_connected = false;
        #if UART_CONNECTION_TEST_TXRX == 1
    UART_Rx_connected      = connectToDriver(_driverDEC, "DEC");
    if (!UART_Rx_connected)
    {
        digitalWrite(DEC_EN_PIN,
                     HIGH);  //Disable motor for safety reasons if UART connection fails to avoid operating at incorrect rms_current
    }
        #endif
    _driverDEC->toff(0);
        #if USE_VREF == 0  //By default, Vref is ignored when using UART to specify rms current.
    _driverDEC->I_scale_analog(false);
        #endif
    LOG(DEBUG_STEPPERS, "[MOUNT]: Requested DEC motor rms_current: %d mA", rmscurrent);
    _driverDEC->rms_current(rmscurrent, 1.0f);  //holdMultiplier = 1 to set ihold = irun
    _driverDEC->toff(1);
    _driverDEC->en_spreadCycle(DEC_UART_STEALTH_MODE == 0);
    _driverDEC->blank_time(24);
    _driverDEC->microsteps(
        DEC_GUIDE_MICROSTEPPING == 1 ? 0 : DEC_GUIDE_MICROSTEPPING);  // If 1 then disable microstepping. Start with Guide microsteps
    _driverDEC->TCOOLTHRS(0xFFFFF);
    _driverDEC->semin(0);  //disable CoolStep so that current is consistent
    _driverDEC->SGTHRS(stallvalue);
    if (UART_Rx_connected)
    {
        LOG(DEBUG_STEPPERS, "[MOUNT]: Actual DEC motor rms_current: %d mA", _driverDEC->rms_current());
        LOG(DEBUG_STEPPERS, "[MOUNT]: Actual DEC CS value: %d", _driverDEC->cs_actual());
        LOG(DEBUG_STEPPERS, "[MOUNT]: Actual DEC vsense: %d", _driverDEC->vsense());
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
    _driverAZ = new TMC2209Stepper(serial, rsense, driveraddress);
    _driverAZ->begin();
    bool UART_Rx_connected = false;
        #if UART_CONNECTION_TEST_TXRX == 1
    UART_Rx_connected = connectToDriver(_driverAZ, "AZ");
    if (!UART_Rx_connected)
    {
        digitalWrite(AZ_EN_PIN,
                     HIGH);  //Disable motor for safety reasons if UART connection fails to avoid operating at incorrect rms_current
    }
        #endif
    _driverAZ->toff(0);
        #if USE_VREF == 0  //By default, Vref is ignored when using UART to specify rms current.
    _driverAZ->I_scale_analog(false);
        #endif
    LOG(DEBUG_STEPPERS, "[MOUNT]: Requested AZ motor rms_current: %d mA", rmscurrent);
    _driverAZ->rms_current(rmscurrent, AZ_MOTOR_HOLD_SETTING / 100.0);  //holdMultiplier = 1 to set ihold = irun
    _driverAZ->toff(1);
    _driverAZ->en_spreadCycle(0);
    _driverAZ->blank_time(24);
    _driverAZ->microsteps(AZ_MICROSTEPPING == 1 ? 0 : AZ_MICROSTEPPING);  // If 1 then disable microstepping
    _driverAZ->TCOOLTHRS(0xFFFFF);                                        //xFFFFF);
    _driverAZ->semin(0);                                                  //disable CoolStep so that current is consistent
    _driverAZ->SGTHRS(stallvalue);
    if (UART_Rx_connected)
    {
        LOG(DEBUG_STEPPERS, "[MOUNT]: Actual AZ motor rms_current: %d mA", _driverAZ->rms_current());
        LOG(DEBUG_STEPPERS, "[MOUNT]: Actual AZ CS value: %d", _driverAZ->cs_actual());
        LOG(DEBUG_STEPPERS, "[MOUNT]: Actual AZ vsense: %d", _driverAZ->vsense());
    }
}

    #elif SW_SERIAL_UART == 1

void Mount::configureAZdriver(uint16_t AZ_SW_RX, uint16_t AZ_SW_TX, float rsense, byte driveraddress, int rmscurrent, int stallvalue)
{
    _driverAZ = new TMC2209Stepper(AZ_SW_RX, AZ_SW_TX, rsense, driveraddress);
    _driverAZ->beginSerial(19200);
    _driverAZ->mstep_reg_select(true);
    _driverAZ->pdn_disable(true);
    bool UART_Rx_connected = false;
        #if UART_CONNECTION_TEST_TXRX == 1
    UART_Rx_connected      = connectToDriver(_driverAZ, "AZ");
    if (!UART_Rx_connected)
    {
        digitalWrite(AZ_EN_PIN,
                     HIGH);  //Disable motor for safety reasons if UART connection fails to avoid operating at incorrect rms_current
    }
        #endif
    _driverAZ->toff(0);
        #if USE_VREF == 0  //By default, Vref is ignored when using UART to specify rms current.
    _driverAZ->I_scale_analog(false);
        #endif
    LOG(DEBUG_STEPPERS, "[MOUNT]: Requested AZ motor rms_current: %d mA", rmscurrent);
    _driverAZ->rms_current(rmscurrent, AZ_MOTOR_HOLD_SETTING / 100.0);  //holdMultiplier = 1 to set ihold = irun
    _driverAZ->toff(1);
    _driverAZ->en_spreadCycle(0);
    _driverAZ->blank_time(24);
    _driverAZ->microsteps(AZ_MICROSTEPPING == 1 ? 0 : AZ_MICROSTEPPING);  // If 1 then disable microstepping
    _driverAZ->TCOOLTHRS(0xFFFFF);                                        //xFFFFF);
    _driverAZ->semin(0);                                                  //disable CoolStep so that current is consistent
    _driverAZ->SGTHRS(stallvalue);
    if (UART_Rx_connected)
    {
        LOG(DEBUG_STEPPERS, "[MOUNT]: Actual AZ motor rms_current: %d mA", _driverAZ->rms_current());
        LOG(DEBUG_STEPPERS, "[MOUNT]: Actual AZ CS value: %d", _driverAZ->cs_actual());
        LOG(DEBUG_STEPPERS, "[MOUNT]: Actual AZ vsense: %d", _driverAZ->vsense());
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
    _driverALT = new TMC2209Stepper(serial, rsense, driveraddress);
    _driverALT->begin();
    bool UART_Rx_connected = false;
        #if UART_CONNECTION_TEST_TXRX == 1
    UART_Rx_connected = connectToDriver(_driverALT, "ALT");
    if (!UART_Rx_connected)
    {
        digitalWrite(ALT_EN_PIN,
                     HIGH);  //Disable motor for safety reasons if UART connection fails to avoid operating at incorrect rms_current
    }
        #endif
    _driverALT->toff(0);
        #if USE_VREF == 0  //By default, Vref is ignored when using UART to specify rms current.
    _driverALT->I_scale_analog(false);
        #endif
    LOG(DEBUG_STEPPERS, "[MOUNT]: Requested ALT motor rms_current: %d mA", rmscurrent);
    _driverALT->rms_current(rmscurrent, ALT_MOTOR_HOLD_SETTING / 100.0);  //holdMultiplier = 1 to set ihold = irun
    _driverALT->toff(1);
    _driverALT->en_spreadCycle(0);
    _driverALT->blank_time(24);
    _driverALT->microsteps(ALT_MICROSTEPPING == 1 ? 0 : ALT_MICROSTEPPING);  // If 1 then disable microstepping
    _driverALT->TCOOLTHRS(0xFFFFF);                                          //xFFFFF);
    _driverALT->semin(0);                                                    //disable CoolStep so that current is consistent
    _driverALT->SGTHRS(stallvalue);
    if (UART_Rx_connected)
    {
        LOG(DEBUG_STEPPERS, "[MOUNT]: Actual ALT motor rms_current: %d mA", _driverALT->rms_current());
        LOG(DEBUG_STEPPERS, "[MOUNT]: Actual ALT CS value: %d", _driverALT->cs_actual());
        LOG(DEBUG_STEPPERS, "[MOUNT]: Actual ALT vsense: %d", _driverALT->vsense());
    }
}

    #elif SW_SERIAL_UART == 1

void Mount::configureALTdriver(uint16_t ALT_SW_RX, uint16_t ALT_SW_TX, float rsense, byte driveraddress, int rmscurrent, int stallvalue)
{
    _driverALT = new TMC2209Stepper(ALT_SW_RX, ALT_SW_TX, rsense, driveraddress);
    _driverALT->beginSerial(19200);
    _driverALT->mstep_reg_select(true);
    _driverALT->pdn_disable(true);
        #if UART_CONNECTION_TEST_TXRX == 1
    bool UART_Rx_connected = false;
    UART_Rx_connected      = connectToDriver(_driverALT, "ALT");
    if (!UART_Rx_connected)
    {
        digitalWrite(ALT_EN_PIN,
                     HIGH);  //Disable motor for safety reasons if UART connection fails to avoid operating at incorrect rms_current
    }
        #endif
    _driverALT->toff(0);
        #if USE_VREF == 0  //By default, Vref is ignored when using UART to specify rms current.
    _driverALT->I_scale_analog(false);
        #endif
    LOG(DEBUG_STEPPERS, "[MOUNT]: Requested ALT motor rms_current: %d mA", rmscurrent);
    _driverALT->rms_current(rmscurrent, ALT_MOTOR_HOLD_SETTING / 100.0);  //holdMultiplier = 1 to set ihold = irun
    _driverALT->toff(1);
    _driverALT->en_spreadCycle(0);
    _driverALT->blank_time(24);
    _driverALT->microsteps(ALT_MICROSTEPPING == 1 ? 0 : ALT_MICROSTEPPING);  // If 1 then disable microstepping
    _driverALT->TCOOLTHRS(0xFFFFF);                                          //xFFFFF);
    _driverALT->semin(0);                                                    //disable CoolStep so that current is consistent
    _driverALT->SGTHRS(stallvalue);
        #if UART_CONNECTION_TEST_TXRX == 1
    if (UART_Rx_connected)
    {
        LOG(DEBUG_STEPPERS, "[MOUNT]: Actual ALT motor rms_current: %d mA", _driverALT->rms_current());
        LOG(DEBUG_STEPPERS, "[MOUNT]: Actual ALT CS value: %d", _driverALT->cs_actual());
        LOG(DEBUG_STEPPERS, "[MOUNT]: Actual ALT vsense: %d", _driverALT->vsense());
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
    _driverFocus = new TMC2209Stepper(serial, rsense, driveraddress);
    _driverFocus->begin();
        #if UART_CONNECTION_TEST_TXRX == 1
    bool UART_Rx_connected = false;
    UART_Rx_connected      = connectToDriver(_driverFocus, "Focus");
    if (!UART_Rx_connected)
    {
        digitalWrite(ALT_EN_PIN,
                     HIGH);  //Disable motor for safety reasons if UART connection fails to avoid operating at incorrect rms_current
    }
        #endif
    _driverFocus->toff(0);
        #if USE_VREF == 0  //By default, Vref is ignored when using UART to specify rms current.
    _driverFocus->I_scale_analog(false);
        #endif
    LOG(DEBUG_STEPPERS | DEBUG_FOCUS, "[FOCUS]: Requested Focus motor rms_current: %d mA", rmscurrent);
    _driverFocus->rms_current(rmscurrent, FOCUSER_MOTOR_HOLD_SETTING / 100.f);  //holdMultiplier = 1 to set ihold = irun
    _driverFocus->toff(1);
    _driverFocus->en_spreadCycle(FOCUS_UART_STEALTH_MODE == 0);
    _driverFocus->blank_time(24);
    _driverFocus->microsteps(FOCUS_MICROSTEPPING == 1 ? 0 : FOCUS_MICROSTEPPING);  // If 1 then disable microstepping
    _driverFocus->TCOOLTHRS(0xFFFFF);                                              //xFFFFF);
    _driverFocus->semin(0);                                                        //disable CoolStep so that current is consistent
    _driverFocus->SGTHRS(stallvalue);
        #if UART_CONNECTION_TEST_TXRX == 1
    if (UART_Rx_connected)
    {
        LOG(DEBUG_STEPPERS | DEBUG_FOCUS, "[FOCUS]: Actual Focus motor rms_current: %d mA", _driverFocus->rms_current());
        LOG(DEBUG_STEPPERS | DEBUG_FOCUS, "[FOCUS]: Actual Focus CS value: %d", _driverFocus->cs_actual());
        LOG(DEBUG_STEPPERS | DEBUG_FOCUS, "[FOCUS]: Actual Focus vsense: %d", _driverFocus->vsense());
    }
        #endif

        #if FOCUSER_ALWAYS_ON == 1
    LOG(DEBUG_FOCUS, "[FOCUS]: Always on -> TMC2209U enabling driver pin.");
    digitalWrite(FOCUS_EN_PIN, LOW);  // Logic LOW to enable driver
        #endif
}

    #elif SW_SERIAL_UART == 1

void Mount::configureFocusDriver(
    uint16_t FOCUS_SW_RX, uint16_t FOCUS_SW_TX, float rsense, byte driveraddress, int rmscurrent, int stallvalue)
{
    _driverFocus = new TMC2209Stepper(FOCUS_SW_RX, FOCUS_SW_TX, rsense, driveraddress);
    _driverFocus->beginSerial(19200);
    _driverFocus->mstep_reg_select(true);
    _driverFocus->pdn_disable(true);
        #if UART_CONNECTION_TEST_TXRX == 1
    bool UART_Rx_connected = false;
    UART_Rx_connected      = connectToDriver(_driverFocus, "Focus");
    if (!UART_Rx_connected)
    {
        digitalWrite(FOCUS_EN_PIN,
                     HIGH);  //Disable motor for safety reasons if UART connection fails to avoid operating at incorrect rms_current
    }
        #endif
    _driverFocus->toff(0);
        #if USE_VREF == 0  //By default, Vref is ignored when using UART to specify rms current.
    _driverFocus->I_scale_analog(false);
        #endif
    LOG(DEBUG_STEPPERS | DEBUG_FOCUS, "[FOCUS]: Requested Focus motor rms_current: %d mA", rmscurrent);
    _driverFocus->rms_current(rmscurrent, FOCUSER_MOTOR_HOLD_SETTING / 100.f);  //holdMultiplier = 1 to set ihold = irun
    _driverFocus->toff(1);
    _driverFocus->en_spreadCycle(FOCUS_UART_STEALTH_MODE == 0);
    _driverFocus->blank_time(24);
    _driverFocus->microsteps(FOCUS_MICROSTEPPING == 1 ? 0 : FOCUS_MICROSTEPPING);  // If 1 then disable microstepping
    _driverFocus->TCOOLTHRS(0xFFFFF);                                              //xFFFFF);
    _driverFocus->semin(0);                                                        //disable CoolStep so that current is consistent
    _driverFocus->SGTHRS(stallvalue);
        #if UART_CONNECTION_TEST_TXRX == 1
    if (UART_Rx_connected)
    {
        LOG(DEBUG_STEPPERS | DEBUG_FOCUS, "[FOCUS]: Actual Focus motor rms_current: %d mA", _driverFocus->rms_current());
        LOG(DEBUG_STEPPERS | DEBUG_FOCUS, "[FOCUS]: Actual Focus CS value: %d", _driverFocus->cs_actual());
        LOG(DEBUG_STEPPERS | DEBUG_FOCUS, "[FOCUS]: Actual Focus vsense: %d", _driverFocus->vsense());
    }
        #endif
        #if FOCUSER_ALWAYS_ON == 1
    LOG(DEBUG_FOCUS, "[FOCUS]: Always on -> TMC2209U enabling driver pin.");
    digitalWrite(FOCUS_EN_PIN, LOW);  // Logic LOW to enable driver
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
    return _trackingSpeedCalibration;
}

/////////////////////////////////
//
// setSpeedCalibration
//
/////////////////////////////////
void Mount::setSpeedCalibration(float val, bool saveToStorage)
{
    LOG(DEBUG_MOUNT, "[MOUNT]: Updating speed calibration from %f to %f", _trackingSpeedCalibration, val);
    _trackingSpeedCalibration = val;

    LOG(DEBUG_MOUNT, "[MOUNT]: Current tracking speed is %f steps/sec", _trackingSpeed);

    // Tracking speed has to be exactly the rotation speed of the earth. The earth rotates 360° per astronomical day.
    // This is 23h 56m 4.0905s, therefore the dimensionless _trackingSpeedCalibration = (23h 56m 4.0905s / 24 h) * mechanical calibration factor
    // Also compensate for higher precision microstepping in tracking mode
    _trackingSpeed = _trackingSpeedCalibration * _stepsPerRADegree * (RA_TRACKING_MICROSTEPPING / RA_SLEW_MICROSTEPPING) * 360.0f
                     / secondsPerDay;  // (fraction of day) * u-steps/deg * (u-steps/u-steps) * deg / (sec/day) = u-steps / sec
    LOG(DEBUG_MOUNT, "[MOUNT]: RA steps per degree is %f steps/deg", _stepsPerRADegree);
    LOG(DEBUG_MOUNT, "[MOUNT]: New tracking speed is %f steps/sec", _trackingSpeed);

    LOG(DEBUG_MOUNT, "[MOUNT]: FactorToSpeed : %s, %s", String(val, 6).c_str(), String(_trackingSpeed, 6).c_str());

    if (saveToStorage)
        EEPROMStore::storeSpeedFactor(_trackingSpeedCalibration);

    // If we are currently tracking, update the speed. No need to update microstepping mode
    if (isSlewingTRK())
    {
        LOG(DEBUG_STEPPERS, "[MOUNT]: SpeedCalibration TRK.setSpeed(%f)", _trackingSpeed);
        _stepperTRK->setSpeed(_trackingSpeed);
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
    EEPROMStore::storePitchCalibrationAngle(_pitchCalibrationAngle);
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
    EEPROMStore::storeRollCalibrationAngle(_rollCalibrationAngle);
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
        EEPROMStore::storeDECStepsPerDegree(_stepsPerDECDegree);
    }
    else if (which == RA_STEPS)
    {
        _stepsPerRADegree = steps;
        EEPROMStore::storeRAStepsPerDegree(_stepsPerRADegree);
        setSpeedCalibration(_trackingSpeedCalibration, false);
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

#if DEW_HEATER == 1
    ret += F("HEATER,");
#else
    ret += F("NO_HEATER,");
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
    EEPROMStore::storeBacklashCorrectionSteps(_backlashCorrectionSteps);
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
    LOG(DEBUG_MOUNT, "[MOUNT]: setSlewRate, rate is %d -> %f", _moveRate, speedFactor[_moveRate]);
    _stepperDEC->setMaxSpeed(speedFactor[_moveRate] * _maxDECSpeed);
    _stepperRA->setMaxSpeed(speedFactor[_moveRate] * _maxRASpeed);
    LOG(DEBUG_MOUNT, "[MOUNT]: setSlewRate, new speeds are RA: %f  DEC: %f", _stepperRA->maxSpeed(), _stepperDEC->maxSpeed());
}

/////////////////////////////////
//
// setHA
//
/////////////////////////////////
void Mount::setHA(const DayTime &haTime)
{
    LOG(DEBUG_MOUNT, "[MOUNT]: setHA:  HA is %s", haTime.ToString());
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
#ifdef OAM
    _zeroPosRA.addHours(6);  // shift allcoordinates by 90° for EQ mount movement
#endif
    LOG(DEBUG_MOUNT, "[MOUNT]: Set LST and ZeroPosRA to: %s", _LST.ToString());
}

/////////////////////////////////
//
// setLatitude
//
/////////////////////////////////
void Mount::setLatitude(Latitude latitude)
{
    _latitude = latitude;
    EEPROMStore::storeLatitude(_latitude);
}

/////////////////////////////////
//
// setLongitude
//
/////////////////////////////////
void Mount::setLongitude(Longitude longitude)
{
    _longitude = longitude;
    EEPROMStore::storeLongitude(_longitude);

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
    if (NORTHERN_HEMISPHERE ? degreePos < 0 : degreePos > 0)
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
    LOG(DEBUG_COORD_CALC, "[MOUNT]: syncPosition( RA: %s and DEC: %s )", _targetRA.ToString(), _targetDEC.ToString());

    // Adjust the home RA position by the delta sync position.
    float raAdjust = ra.getTotalHours() - currentRA().getTotalHours();
    LOG(DEBUG_COORD_CALC, "[MOUNT]: syncPosition: AdjustRA is %f  (%f - %f)", raAdjust, ra.getTotalHours(), currentRA().getTotalHours());
    while (raAdjust > 12)
    {
        raAdjust -= 24;
    }
    while (raAdjust < -12)
    {
        raAdjust += 24;
    }
    LOG(DEBUG_COORD_CALC, "[MOUNT]: syncPosition: AdjustRA is %f", raAdjust);
    _zeroPosRA.addHours(raAdjust);
    LOG(DEBUG_COORD_CALC, "[MOUNT]: syncPosition: ZeroPosRA is now %f", _zeroPosRA.getTotalHours());

    // Adjust the home DEC position by the delta between the sync'd target and current position.
    const float degreePos = (_stepperDEC->currentPosition() / _stepsPerDECDegree) + _zeroPosDEC;  // u-steps / u-steps/deg = deg
    float decAdjust       = dec.getTotalDegrees() - fabsf(currentDEC().getTotalDegrees());
    if (degreePos < 0)
    {
        decAdjust = -decAdjust;
    }
    _zeroPosDEC += decAdjust;
    LOG(DEBUG_COORD_CALC, "[MOUNT]: syncPosition: _zerPosDEC adjusted by: %f", decAdjust);
    LOG(DEBUG_COORD_CALC, "[MOUNT]: syncPosition: _zerPosDEC: %f", _zeroPosDEC);

    long targetRAPosition, targetDECPosition;
    calculateRAandDECSteppers(targetRAPosition, targetDECPosition, solutions);

    LOG(DEBUG_COORD_CALC, "[MOUNT]: syncPosition: Solution 1: RA %l and DEC %l", solutions[0], solutions[1]);
    LOG(DEBUG_COORD_CALC, "[MOUNT]: syncPosition: Solution 2: RA %l and DEC %l", solutions[2], solutions[3]);
    LOG(DEBUG_COORD_CALC, "[MOUNT]: syncPosition: Solution 3: RA %l and DEC %l", solutions[4], solutions[5]);
    LOG(DEBUG_COORD_CALC, "[MOUNT]: syncPosition: Chosen    : RA %l and DEC %l", targetRAPosition, targetDECPosition);
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
    stopGuiding();

    // Make sure we're slewing at full speed on a GoTo
    LOG(DEBUG_STEPPERS, "[STEPPERS]: startSlewingToTarget: Set DEC to MaxSpeed(%d)", _maxDECSpeed);
    _stepperDEC->setMaxSpeed(_maxDECSpeed);
    LOG(DEBUG_STEPPERS, "[STEPPERS]: startSlewingToTarget: Set RA  to MaxSpeed(%d)", _maxRASpeed);
    _stepperRA->setMaxSpeed(_maxRASpeed);

    // Calculate new RA stepper target (and DEC). We are never in guiding mode here.
    _currentRAStepperPosition = _stepperRA->currentPosition();
    long targetRAPosition, targetDECPosition;
    calculateRAandDECSteppers(targetRAPosition, targetDECPosition);

    if (_slewingToHome)
    {
        targetRAPosition -= _homeOffsetRA;
        targetDECPosition -= _homeOffsetDEC;
    }

    moveSteppersTo(targetRAPosition, targetDECPosition);  // u-steps (in slew mode)

    _mountStatus |= STATUS_SLEWING | STATUS_SLEWING_TO_TARGET;
    _totalDECMove = 1.0f * _stepperDEC->distanceToGo();
    _totalRAMove  = 1.0f * _stepperRA->distanceToGo();
    LOG(DEBUG_MOUNT, "[MOUNT]: RA Dist: %l,   DEC Dist: %l", _stepperRA->distanceToGo(), _stepperDEC->distanceToGo());
    if ((_stepperRA->distanceToGo() != 0) || (_stepperDEC->distanceToGo() != 0))
    {
        // Only stop tracking if we're actually going to slew somewhere else, otherwise the
        // mount::loop() code won't detect the end of the slewing operation...
        LOG(DEBUG_STEPPERS, "[MOUNT]: Stop tracking (NEMA steppers)");
        stopSlewing(TRACKING);
        _trackerStoppedAt        = millis();
        _compensateForTrackerOff = true;

// set Slew microsteps for TMC2209 UART once the TRK stepper has stopped
#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
        LOG(DEBUG_STEPPERS, "[STEPPERS]: startSlewingToTarget: Switching RA driver to microsteps(%d)", RA_SLEW_MICROSTEPPING);
        _driverRA->microsteps(RA_SLEW_MICROSTEPPING == 1 ? 0 : RA_SLEW_MICROSTEPPING);
#endif

        LOG(DEBUG_STEPPERS, "[STEPPERS]: startSlewingToTarget: TRK stopped at %lms", _trackerStoppedAt);
    }

#if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    // Since normal state for DEC is guide microstepping, switch to slew microstepping here.
    LOG(DEBUG_STEPPERS, "[STEPPERS]: startSlewingToTarget: Switching DEC driver to microsteps(%d)", DEC_SLEW_MICROSTEPPING);
    _driverDEC->microsteps(DEC_SLEW_MICROSTEPPING == 1 ? 0 : DEC_SLEW_MICROSTEPPING);
#endif
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
    LOG(DEBUG_STEPPERS, "[STEPPERS]: startSlewingToHome: Set DEC to MaxSpeed(%d)", _maxDECSpeed);
    _stepperDEC->setMaxSpeed(_maxDECSpeed);
    LOG(DEBUG_STEPPERS, "[STEPPERS]: startSlewingToHome: Set RA  to MaxSpeed(%d)", _maxRASpeed);
    _stepperRA->setMaxSpeed(_maxRASpeed);

    _currentRAStepperPosition = _stepperRA->currentPosition();

    // Take any syncs that have happened into account
    long targetRAPosition        = -_homeOffsetRA;
    const long targetDECPosition = -_homeOffsetDEC;
    LOG(DEBUG_STEPPERS, "[STEPPERS]: startSlewingToHome: Sync op offsets: RA: %l, DEC: %l", targetRAPosition, targetDECPosition);

    _slewingToHome = true;
    // Take tracking into account
    const long trackingOffset = _stepperTRK->currentPosition() * RA_SLEW_MICROSTEPPING / RA_TRACKING_MICROSTEPPING;
    targetRAPosition -= trackingOffset;
    LOG(DEBUG_STEPPERS,
        "[STEPPERS]: startSlewingToHome: Adjusted with tracking distance: %l (adjusted for MS: %l), result: %l",
        _stepperTRK->currentPosition(),
        trackingOffset,
        targetRAPosition);

    moveSteppersTo(targetRAPosition, targetDECPosition);  // u-steps (in slew mode)

    _mountStatus |= STATUS_SLEWING | STATUS_SLEWING_TO_TARGET;
    _totalDECMove = static_cast<float>(_stepperDEC->distanceToGo());
    _totalRAMove  = static_cast<float>(_stepperRA->distanceToGo());
    LOG(DEBUG_MOUNT, "[MOUNT]: RA Dist: %l,   DEC Dist: %l", _stepperRA->distanceToGo(), _stepperDEC->distanceToGo());
    if ((_stepperRA->distanceToGo() != 0) || (_stepperDEC->distanceToGo() != 0))
    {
        // Only stop tracking if we're actually going to slew somewhere else, otherwise the
        // mount::loop() code won't detect the end of the slewing operation...
        LOG(DEBUG_STEPPERS, "[MOUNT]: Stop tracking (NEMA steppers)");
        stopSlewing(TRACKING);
        _trackerStoppedAt        = millis();
        _compensateForTrackerOff = false;

// set Slew microsteps for TMC2209 UART once the TRK stepper has stopped
#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
        LOG(DEBUG_STEPPERS, "[STEPPERS]: startSlewingToHome: Switching RA driver to microsteps(%d)", RA_SLEW_MICROSTEPPING);
        _driverRA->microsteps(RA_SLEW_MICROSTEPPING == 1 ? 0 : RA_SLEW_MICROSTEPPING);
#endif

        LOG(DEBUG_STEPPERS, "[STEPPERS]: startSlewingToHome: TRK stopped at %lms", _trackerStoppedAt);
    }

#if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    // Since normal state for DEC is guide microstepping, switch to slew microstepping here.
    LOG(DEBUG_STEPPERS, "[STEPPERS]: startSlewingToHome: Switching DEC driver to microsteps(%d)", DEC_SLEW_MICROSTEPPING);
    _driverDEC->microsteps(DEC_SLEW_MICROSTEPPING == 1 ? 0 : DEC_SLEW_MICROSTEPPING);
#endif
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
    if (ra && (_mountStatus & STATUS_GUIDE_PULSE_RA))
    {
        LOG(DEBUG_STEPPERS, "[STEPPERS]: stopGuiding(RA): TRK.setSpeed(%f)", _trackingSpeed);
        _stepperTRK->setSpeed(_trackingSpeed);
        _mountStatus &= ~STATUS_GUIDE_PULSE_RA;
    }

    if (dec && (_mountStatus & STATUS_GUIDE_PULSE_DEC))
    {
        LOG(DEBUG_STEPPERS, "[STEPPERS]: stopGuiding(DEC): Stop motor");

        // Stop DEC guiding and wait for it to stop.
        _stepperGUIDE->stop();

        while (_stepperGUIDE->isRunning())
        {
            _stepperGUIDE->run();
            _stepperTRK->runSpeed();
        }

        LOG(DEBUG_STEPPERS, "[STEPPERS]: stopGuiding(DEC): GuideStepper stopped at %l", _stepperGUIDE->currentPosition());

        _mountStatus &= ~STATUS_GUIDE_PULSE_DEC;
    }

    //disable pulse state if no direction is active
    if ((_mountStatus & STATUS_GUIDE_PULSE_DIR) == 0)
    {
        _mountStatus &= ~STATUS_GUIDE_PULSE_MASK;
    }
}

/////////////////////////////////
//
// guidePulse
//
/////////////////////////////////
void Mount::guidePulse(byte direction, int duration)
{
    LOG(DEBUG_STEPPERS, "[STEPPERS]: guidePulse: > Guide Pulse %d for %dms", direction, duration);

    // DEC stepper moves at sidereal rate in both directions
    // RA stepper moves at either 2.5x sidereal rate or 0.5x sidereal rate.
    // Also compensate for microstepping mode change between slew & guiding/tracking
    float decGuidingSpeed = _stepsPerDECDegree * (DEC_GUIDE_MICROSTEPPING / DEC_SLEW_MICROSTEPPING) * siderealDegreesInHour
                            / 3600.0f;  // u-steps/deg * deg/hr / sec/hr = u-steps/sec
    float raGuidingSpeed = _stepsPerRADegree * (RA_TRACKING_MICROSTEPPING / RA_SLEW_MICROSTEPPING) * siderealDegreesInHour
                           / 3600.0f;  // u-steps/deg * deg/hr / sec/hr = u-steps/sec
    raGuidingSpeed *= _trackingSpeedCalibration;

    // TODO: Do we need to track how many steps the steppers took and add them to the GoHome calculation?
    // If so, we need to remember where we were when we started the guide pulse. Then at the end,
    // we can calculate the difference. Ignore DEC Guide for now.
    // TODO: Take guide pulses on DEC into account

    switch (direction)
    {
        case NORTH:
            LOG(DEBUG_STEPPERS, "[STEPPERS]: guidePulse:  DEC.setSpeed(%f)", DEC_PULSE_MULTIPLIER * decGuidingSpeed);
            _stepperGUIDE->setSpeed(DEC_PULSE_MULTIPLIER * decGuidingSpeed);
            _mountStatus |= STATUS_GUIDE_PULSE | STATUS_GUIDE_PULSE_DEC;
            _guideDecEndTime = millis() + duration;
            break;

        case SOUTH:
            LOG(DEBUG_STEPPERS, "[STEPPERS]: guidePulse:  DEC.setSpeed(%f)", -DEC_PULSE_MULTIPLIER * decGuidingSpeed);
            _stepperGUIDE->setSpeed(-DEC_PULSE_MULTIPLIER * decGuidingSpeed);
            _mountStatus |= STATUS_GUIDE_PULSE | STATUS_GUIDE_PULSE_DEC;
            _guideDecEndTime = millis() + duration;
            break;

        case WEST:
            // We were in tracking mode before guiding, so no need to update microstepping mode on RA driver
            LOG(DEBUG_STEPPERS, "[STEPPERS]: guidePulse:  TRK.setSpeed(%f)", (RA_PULSE_MULTIPLIER * raGuidingSpeed));
            _stepperTRK->setSpeed(RA_PULSE_MULTIPLIER * raGuidingSpeed);  // Faster than siderael
            _mountStatus |= STATUS_GUIDE_PULSE | STATUS_GUIDE_PULSE_RA;
            _guideRaEndTime = millis() + duration;
            break;

        case EAST:
            // We were in tracking mode before guiding, so no need to update microstepping mode on RA driver
            LOG(DEBUG_STEPPERS, "[STEPPERS]: guidePulse:  TRK.setSpeed(%f)", (raGuidingSpeed * (2.0f - RA_PULSE_MULTIPLIER)));
            _stepperTRK->setSpeed(raGuidingSpeed * (2.0f - RA_PULSE_MULTIPLIER));  // Slower than siderael
            _mountStatus |= STATUS_GUIDE_PULSE | STATUS_GUIDE_PULSE_RA;
            _guideRaEndTime = millis() + duration;
            break;
    }

    LOG(DEBUG_STEPPERS, "[STEPPERS]: guidePulse: < Guide Pulse");
}

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
            while (_stepperRA->run())
            {
                yield();
            }

            // Overcome the gearing gap
            _stepperRA->setMaxSpeed(300);
            _stepperRA->move(-20);
            while (_stepperRA->run())
            {
                yield();
            }
            break;

        case WEST:
            // Move steps west at the calculated speed, synchronously
            _stepperRA->setMaxSpeed(speed);
            _stepperRA->move(-numSteps);
            while (_stepperRA->run())
            {
                yield();
            }
            break;

        case 0:
            // Fix the gearing to go back the other way
            _stepperRA->setMaxSpeed(300);
            _stepperRA->move(20);
            while (_stepperRA->run())
            {
                yield();
            }

            // Re-configure the stepper to the correct parameters.
            _stepperRA->setMaxSpeed(_maxRASpeed);
            break;
    }
}

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
        _mountStatus |= STATUS_SLEWING | STATUS_SLEWING_MANUAL;
#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
        LOG(DEBUG_STEPPERS, "[STEPPERS]: setManualSlewMode: Switching RA driver to microsteps(%d)", RA_SLEW_MICROSTEPPING);
        _driverRA->microsteps(RA_SLEW_MICROSTEPPING == 1 ? 0 : RA_SLEW_MICROSTEPPING);
#endif
    }
    else
    {
        _mountStatus &= ~STATUS_SLEWING_MANUAL;
        stopSlewing(ALL_DIRECTIONS);
        waitUntilStopped(ALL_DIRECTIONS);
        LOG(DEBUG_STEPPERS, "[STEPPERS]: setManualSlewMode: Set RA  speed/accel:  %f  / %f", _maxRASpeed, _maxRAAcceleration);
        LOG(DEBUG_STEPPERS, "[STEPPERS]: setManualSlewMode: Set DEC speed/accel:  %f  / %f", _maxDECSpeed, _maxDECAcceleration);
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
        LOG(DEBUG_STEPPERS, "[STEPPERS]: setSpeed: Set RA speed %f degs/s, which is %f steps/s", speedDegsPerSec, stepsPerSec);
        // TODO: Are we already in slew mode?
        _stepperRA->setSpeed(stepsPerSec);
    }
    else if (which == DEC_STEPS)
    {
        float stepsPerSec = speedDegsPerSec * _stepsPerDECDegree;  // deg/sec * u-steps/deg = u-steps/sec
        LOG(DEBUG_STEPPERS, "[STEPPERS]: setSpeed: Set DEC speed %f degs/s, which is %f steps/s", speedDegsPerSec, stepsPerSec);
        // TODO: Are we already in slew mode?
        _stepperDEC->setSpeed(stepsPerSec);
    }
#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    else if (which == AZIMUTH_STEPS)
    {
        float stepsPerSec = speedDegsPerSec * _stepsPerAZDegree;  // deg/sec * u-steps/deg = u-steps/sec
        LOG(DEBUG_STEPPERS, "[STEPPERS]: setSpeed: Set AZ speed %f degs/s, which is %f steps/s", speedDegsPerSec, stepsPerSec);
        _stepperAZ->setSpeed(stepsPerSec);
    }
#endif
#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    else if (which == ALTITUDE_STEPS)
    {
        float stepsPerSec = speedDegsPerSec * _stepsPerALTDegree;  // deg/sec * u-steps/deg = u-steps/sec
        LOG(DEBUG_STEPPERS, "[STEPPERS]: setSpeed: Set ALT speed %f degs/s, which is %f steps/s", speedDegsPerSec, stepsPerSec);
        _stepperALT->setSpeed(stepsPerSec);
    }
#endif

#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    else if (which == FOCUS_STEPS)
    {
        LOG(DEBUG_MOUNT | DEBUG_FOCUS, "[FOCUS]: setSpeed() Focuser setSpeed %f", speedDegsPerSec);
        if (speedDegsPerSec != 0)
        {
            LOG(DEBUG_STEPPERS | DEBUG_FOCUS,
                "[FOCUS]: Mount:setSpeed(): Enabling motor, setting speed to %f. Continuous",
                speedDegsPerSec);
            enableFocusMotor();
            _stepperFocus->setMaxSpeed(speedDegsPerSec);
            _stepperFocus->moveTo(sign(speedDegsPerSec) * 300000);
            _focuserMode = FOCUS_TO_TARGET;
        }
        else
        {
            LOG(DEBUG_STEPPERS | DEBUG_FOCUS, "[FOCUS]: setSpeed(): Stopping motor.");
            _stepperFocus->stop();
        }
    }
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
    _mountStatus |= STATUS_PARKING;
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
        enableAzAltMotors();
        int stepsToMove = arcMinutes * AZIMUTH_STEPS_PER_ARC_MINUTE;
        _stepperAZ->move(stepsToMove);
    }
    #endif
    #if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    if (direction == ALTITUDE_STEPS)
    {
        enableAzAltMotors();
        int stepsToMove = arcMinutes * ALTITUDE_STEPS_PER_ARC_MINUTE;
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
    digitalWrite(AZ_EN_PIN, HIGH);  // Logic HIGH to disable driver
        #endif
    #endif

    #if ALT_ALWAYS_ON == 0
        #if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    digitalWrite(ALT_EN_PIN, HIGH);  // Logic HIGH to disable driver
        #endif
    #endif
}

/////////////////////////////////
//
// enableAzAltMotors
//
/////////////////////////////////
void Mount::enableAzAltMotors()
{
    #if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    digitalWrite(AZ_EN_PIN, LOW);  // Logic LOW to enable driver
    #endif

    #if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    digitalWrite(ALT_EN_PIN, LOW);  // Logic LOW to enable driver
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
    return _stepperFocus->isRunning();
}

/////////////////////////////////
//
// getFocusSpeed
//
/////////////////////////////////
float Mount::getFocusSpeed() const
{
    return _stepperFocus->speed();
}

/////////////////////////////////
//
// focusSetSpeedByRate
//
/////////////////////////////////
void Mount::focusSetSpeedByRate(int rate)
{
    _focusRate          = clamp(rate, 1, 4);
    float speedFactor[] = {0, 0.05, 0.2, 0.5, 1.0};
    _maxFocusRateSpeed  = speedFactor[_focusRate] * _maxFocusSpeed;
    LOG(DEBUG_FOCUS,
        "[FOCUS]: focusSetSpeedByRate: rate is %d, factor %f, maxspeed %f -> %f",
        _focusRate,
        speedFactor[_focusRate],
        _maxFocusSpeed,
        _maxFocusRateSpeed);
    _stepperFocus->setMaxSpeed(_maxFocusRateSpeed);

    if (_stepperFocus->isRunning())
    {
        LOG(DEBUG_FOCUS, "[FOCUS]: focusSetSpeedByRate: stepper is already running so should adjust speed?");
        //_stepperFocus->setSpeed(speedFactor[_focusRate ] * _maxFocusSpeed);
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
    LOG(DEBUG_FOCUS, "[FOCUS]: focusContinuousMove: direction is %d, maxspeed %f", direction, _maxFocusRateSpeed);
    setSpeed(FOCUS_STEPS, static_cast<int>(direction) * _maxFocusRateSpeed);
}

/////////////////////////////////
//
// focusMoveBy
//
/////////////////////////////////
void Mount::focusMoveBy(long steps)
{
    long targetPosition = _stepperFocus->currentPosition() + steps;
    LOG(DEBUG_FOCUS, "[FOCUS]: focusMoveBy: move by %l steps to %l. Target Mode.", steps, targetPosition);
    enableFocusMotor();
    _stepperFocus->moveTo(targetPosition);
    _focuserMode = FOCUS_TO_TARGET;
}

/////////////////////////////////
//
// focusGetPosition
//
/////////////////////////////////
long Mount::focusGetStepperPosition()
{
    return _stepperFocus->currentPosition();
}

/////////////////////////////////
//
// focusSetPosition
//
/////////////////////////////////
void Mount::focusSetStepperPosition(long steps)
{
    _stepperFocus->setCurrentPosition(steps);
}

/////////////////////////////////
//
// disableFocusMotor
//
/////////////////////////////////
void Mount::disableFocusMotor()
{
    LOG(DEBUG_FOCUS, "[FOCUS]: disableFocusMotor: stopping motor and waiting.");
    _stepperFocus->stop();
    waitUntilStopped(FOCUSING);

    #if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
        #if FOCUSER_ALWAYS_ON == 0
            #if (FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)

    LOG(DEBUG_FOCUS, "[FOCUS]: disableFocusMotor: TMC2209U disabling enable pin");
    digitalWrite(FOCUS_EN_PIN, HIGH);  // Logic HIGH to disable driver
            #else
    LOG(DEBUG_FOCUS, "[FOCUS]: disableFocusMotor: non-TMC2209U disabling enable pin");
    digitalWrite(FOCUS_EN_PIN, HIGH);  // Logic HIGH to disable driver
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
    LOG(DEBUG_FOCUS, "[FOCUS]: enableFocusMotor: enabling driver pin.");
    digitalWrite(FOCUS_EN_PIN, LOW);  // Logic LOW to enable driver
}

/////////////////////////////////
//
// focusStop
//
/////////////////////////////////
void Mount::focusStop()
{
    LOG(DEBUG_FOCUS, "[FOCUS]: focusStop: stopping motor.");
    _stepperFocus->stop();
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

/////////////////////////////////
//
// getStatusString
//
/////////////////////////////////
String Mount::getStatusString()
{
    String status;
    if (_mountStatus == STATUS_PARKED)
    {
        status = F("Parked,");
    }
    else if ((_mountStatus & STATUS_PARKING) || (_mountStatus & STATUS_PARKING_POS))
    {
        status = F("Parking,");
    }
    else if (isFindingHome())
    {
        status = F("Homing,");
    }
    else if (isGuiding())
    {
        status = F("Guiding,");
    }
    else if (slewStatus() & SLEW_MASK_ANY)
    {
        if (_mountStatus & STATUS_SLEWING_TO_TARGET)
        {
            status = F("SlewToTarget,");
        }
        else if (_mountStatus & STATUS_SLEWING_FREE)
        {
            status = F("FreeSlew,");
        }
        else if (_mountStatus & STATUS_SLEWING_MANUAL)
        {
            status = F("ManualSlew,");
        }
        else if (slewStatus() & SLEWING_TRACKING)
        {
            status = F("Tracking,");
        }
    }
    else
    {
        status = "Idle,";
    }

    String disp = "------,";
    if (_mountStatus & STATUS_SLEWING)
    {
        byte slew = slewStatus();
        if (slew & SLEWING_RA)
            disp[0] = _stepperRA->speed() < 0 ? 'R' : 'r';
        if (slew & SLEWING_DEC)
            disp[1] = _stepperDEC->speed() < 0 ? 'D' : 'd';
        if (slew & SLEWING_TRACKING)
            disp[2] = 'T';
    }
    else if (isSlewingTRK())
    {
        disp[2] = 'T';
    }
#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    if (_stepperAZ->isRunning())
        disp[3] = _stepperAZ->speed() < 0 ? 'Z' : 'z';
#endif
#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    if (_stepperALT->isRunning())
        disp[4] = _stepperALT->speed() < 0 ? 'A' : 'a';
#endif

#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    if (_stepperFocus->isRunning())
        disp[5] = _stepperFocus->speed() < 0 ? 'F' : 'f';
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
// slewingStatus
//
// Returns the current state of the motors and is a bitfield with these flags:
// NOT_SLEWING is all zero. SLEWING_DEC, SLEWING_RA, SLEWING_BOTH, SLEWING_TRACKING are bits.
/////////////////////////////////
byte Mount::slewStatus() const
{
    if (_mountStatus == STATUS_PARKED)
    {
        return NOT_SLEWING;
    }
    if (isGuiding())
    {
        return NOT_SLEWING;
    }
    byte slewState = _stepperRA->isRunning() ? SLEWING_RA : NOT_SLEWING;
    slewState |= _stepperDEC->isRunning() ? SLEWING_DEC : NOT_SLEWING;

    slewState |= (_mountStatus & STATUS_TRACKING) ? SLEWING_TRACKING : NOT_SLEWING;
    return slewState;
}

/////////////////////////////////
//
// isGuiding
//
/////////////////////////////////
bool Mount::isGuiding() const
{
    return (_mountStatus & STATUS_GUIDE_PULSE);
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
    return _mountStatus & (STATUS_PARKING | STATUS_PARKING_POS);
}

/////////////////////////////////
//
// isFindingHome
//
/////////////////////////////////
bool Mount::isFindingHome() const
{
    return _mountStatus & STATUS_FINDING_HOME;
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
    if (!isParking())
    {
        stopGuiding();

        if (direction & TRACKING)
        {
// Start tracking
#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
            LOG(DEBUG_STEPPERS, "[STEPPERS]: startSlewing: Tracking: Switching RA driver to microsteps(%d)", RA_TRACKING_MICROSTEPPING);
            _driverRA->microsteps(RA_TRACKING_MICROSTEPPING == 1 ? 0 : RA_TRACKING_MICROSTEPPING);
#endif
            _stepperTRK->setSpeed(_trackingSpeed);

            // Turn on tracking
            _mountStatus |= STATUS_TRACKING;
        }
        else
        {
            // Start slewing
            int sign = NORTHERN_HEMISPHERE ? 1 : -1;

            // Set move rate to last commanded slew rate
            setSlewRate(_moveRate);
            if (isSlewingTRK())
            {
                stopSlewing(TRACKING);
                _trackerStoppedAt        = millis();
                _compensateForTrackerOff = true;
                LOG(DEBUG_STEPPERS, "[STEPPERS]: startSlewing: stopped TRK at %l", _trackerStoppedAt);
            }

// Change microstep mode for slewing
#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
            LOG(DEBUG_STEPPERS, "[STEPPERS]: startSlewing: Slewing: Switching RA driver to microsteps(%d)", RA_SLEW_MICROSTEPPING);
            _driverRA->microsteps(RA_SLEW_MICROSTEPPING == 1 ? 0 : RA_SLEW_MICROSTEPPING);
#endif
#if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
            // Since normal state for DEC is guide microstepping, switch to slew microstepping here.
            LOG(DEBUG_STEPPERS, "[STEPPERS]: startSlewing: Slewing: Switching DEC driver to microsteps(%d)", DEC_SLEW_MICROSTEPPING);
            _driverDEC->microsteps(DEC_SLEW_MICROSTEPPING == 1 ? 0 : DEC_SLEW_MICROSTEPPING);
#endif

            if (direction & NORTH)
            {
                long targetLocation = 300000;
                if (_decUpperLimit != 0)
                {
                    targetLocation = _decUpperLimit;
                    LOG(DEBUG_STEPPERS,
                        "[STEPPERS]: startSlewing(N): DEC has upper limit of %l. targetMoveTo is now %l",
                        _decUpperLimit,
                        targetLocation);
                }
                else
                {
                    LOG(DEBUG_STEPPERS, "[STEPPERS]: startSlewing(N): initial targetMoveTo is %l", targetLocation);
                }

                _stepperDEC->moveTo(targetLocation);
                _mountStatus |= STATUS_SLEWING;
            }

            if (direction & SOUTH)
            {
                long targetLocation = -300000;
                if (_decLowerLimit != 0)
                {
                    targetLocation = _decLowerLimit;
                    LOG(DEBUG_STEPPERS,
                        "[STEPPERS]: startSlewing(S): DEC has lower limit of %l. targetMoveTo is now %l",
                        _decLowerLimit,
                        targetLocation);
                }
                else
                {
                    LOG(DEBUG_STEPPERS, "[STEPPERS]: startSlewing(S): initial targetMoveTo is %l", targetLocation);
                }

                _stepperDEC->moveTo(targetLocation);
                _mountStatus |= STATUS_SLEWING;
            }

            if (direction & EAST)
            {
                LOG(DEBUG_STEPPERS, "[STEPPERS]: startSlewing(E): initial targetMoveTo is %l", -sign * 300000);
                _stepperRA->moveTo(-sign * 300000);
                _mountStatus |= STATUS_SLEWING;
            }
            if (direction & WEST)
            {
                LOG(DEBUG_STEPPERS, "[STEPPERS]: startSlewing(W): initial targetMoveTo is %l", sign * 300000);
                _stepperRA->moveTo(sign * 300000);
                _mountStatus |= STATUS_SLEWING;
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
        _mountStatus &= ~STATUS_TRACKING;

        LOG(DEBUG_STEPPERS, "[STEPPERS]: stopSlewing: TRK stepper stop()");
        _stepperTRK->stop();
    }

    if ((direction & (NORTH | SOUTH)) != 0)
    {
        LOG(DEBUG_STEPPERS, "[STEPPERS]: stopSlewing: DEC stepper stop()");
        _stepperDEC->stop();
    }

    if ((direction & (WEST | EAST)) != 0)
    {
        LOG(DEBUG_STEPPERS, "[STEPPERS]: stopSlewing: RA stepper stop()");
        _stepperRA->stop();
        if (isFindingHome())
        {
            _mountStatus &= ~STATUS_FINDING_HOME;
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
           || ((direction & TRACKING) && (((_mountStatus & STATUS_TRACKING) == 0) && _stepperTRK->isRunning()))
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

/////////////////////////////////
//
// setHomingOffset
//
/////////////////////////////////
void Mount::setHomingOffset(StepperAxis axis, long offset)
{
    if (axis == StepperAxis::RA_STEPS)
    {
#if USE_HALL_SENSOR_RA_AUTOHOME == 1
        _homing.offsetRA = offset;
#endif
        EEPROMStore::storeRAHomingOffset(offset);
        LOG(DEBUG_MOUNT, "[MOUNT]: setHomingOffset(RA): Offset: %l", offset);
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
#if USE_HALL_SENSOR_RA_AUTOHOME == 1
        return _homing.offsetRA;
#endif
    }
    return 0;
}

#if USE_HALL_SENSOR_RA_AUTOHOME == 1

String Mount::getHomingState(HomingState state) const
{
    MappedDict<HomingState, String>::DictEntry_t lookupTable[] = {
        {HOMING_MOVE_OFF, F("MOVE_OFF")},
        {HOMING_MOVING_OFF, F("MOVING_OFF")},
        {HOMING_STOP_AT_TIME, F("STOP_AT_TIME")},
        {HOMING_WAIT_FOR_STOP, F("WAIT_FOR_STOP")},
        {HOMING_START_FIND_START, F("START_FIND_START")},
        {HOMING_FINDING_START, F("FINDING_START")},
        {HOMING_FINDING_START_REVERSE, F("FINDING_START_REVERSE")},
        {HOMING_FINDING_END, F("FINDING_END")},
        {HOMING_RANGE_FOUND, F("RANGE_FOUND")},
        {HOMING_FAILED, F("FAILED")},
        {HOMING_SUCCESSFUL, F("SUCCESSFUL")},
        {HOMING_NOT_ACTIVE, F("NOT_ACTIVE")},
    };

    auto strLookup = MappedDict<HomingState, String>(lookupTable, ARRAY_SIZE(lookupTable));
    String rtnStr;
    if (strLookup.tryGet(state, &rtnStr))
    {
        return rtnStr;
    }
    return F("WTF_STATE");
}

/////////////////////////////////
//
// processRAHomingProgress
//
/////////////////////////////////
void Mount::processRAHomingProgress()
{
    switch (_homing.state)
    {
        case HomingState::HOMING_NOT_ACTIVE:
            break;

        case HomingState::HOMING_STOP_AT_TIME:
            {
                if (millis() > _homing.stopAt)
                {
                    LOG(DEBUG_STEPPERS,
                        "[HOMING]: Initiating stop at requested time. Advance to state %s",
                        getHomingState(HomingState::HOMING_WAIT_FOR_STOP).c_str());
                    _stepperRA->stop();
                    _homing.state = HomingState::HOMING_WAIT_FOR_STOP;
                }
            }
            break;

        case HomingState::HOMING_WAIT_FOR_STOP:
            {
                if (!_stepperRA->isRunning())
                {
                    LOG(DEBUG_STEPPERS,
                        "[HOMING]: Stepper has stopped as expected, advancing to next state %s",
                        getHomingState(_homing.nextState).c_str());
                    _homing.state     = _homing.nextState;
                    _homing.nextState = HomingState::HOMING_NOT_ACTIVE;
                }
            }
            break;

        case HomingState::HOMING_MOVE_OFF:
            {
                LOG(DEBUG_STEPPERS,
                    "[HOMING]: Currently over Sensor, so moving off of it by reverse 1h. (%l steps). Advance to %s",
                    (long) (-_homing.initialDir * _stepsPerRADegree * siderealDegreesInHour),
                    getHomingState(HomingState::HOMING_MOVING_OFF).c_str());
                moveStepperBy(StepperAxis::RA_STEPS, -_homing.initialDir * _stepsPerRADegree * siderealDegreesInHour);
                _homing.state = HomingState::HOMING_MOVING_OFF;
            }
            break;

        case HomingState::HOMING_MOVING_OFF:
            {
                if (_stepperRA->isRunning())
                {
                    int homingPinState = digitalRead(RA_HOMING_SENSOR_PIN);
                    if (homingPinState == HIGH)
                    {
                        LOG(DEBUG_STEPPERS,
                            "[HOMING]: Stepper has moved off sensor... stopping in 2s. Advance to %s",
                            getHomingState(HomingState::HOMING_STOP_AT_TIME).c_str());
                        _homing.stopAt    = millis() + 2000;
                        _homing.state     = HomingState::HOMING_STOP_AT_TIME;
                        _homing.nextState = HomingState::HOMING_START_FIND_START;
                    }
                }
                else
                {
                    LOG(DEBUG_STEPPERS,
                        "[HOMING]: Stepper was unable to move off sensor... homing failed! Advance to %s",
                        getHomingState(HomingState::HOMING_FAILED).c_str());
                    _homing.state = HomingState::HOMING_FAILED;
                }
            }
            break;

        case HomingState::HOMING_START_FIND_START:
            {
                long distance = (long) (_homing.initialDir * _stepsPerRADegree * siderealDegreesInHour * _homing.searchDistance);
                LOG(DEBUG_STEPPERS,
                    "[HOMING]: Finding start on forward pass by moving RA by %dh (%l steps). Advance to %s",
                    _homing.searchDistance,
                    distance,
                    getHomingState(HomingState::HOMING_FINDING_START).c_str());
                _homing.pinState = _homing.lastPinState     = digitalRead(RA_HOMING_SENSOR_PIN);
                _homing.position[HOMING_START_PIN_POSITION] = 0;
                _homing.position[HOMING_END_PIN_POSITION]   = 0;

                // Move in initial direction
                moveStepperBy(StepperAxis::RA_STEPS, distance);

                _homing.state = HomingState::HOMING_FINDING_START;
            }
            break;

        case HomingState::HOMING_FINDING_START:
            {
                if (_stepperRA->isRunning())
                {
                    int homingPinState = digitalRead(RA_HOMING_SENSOR_PIN);
                    if (_homing.lastPinState != homingPinState)
                    {
                        LOG(DEBUG_STEPPERS,
                            "[HOMING]: Found start of sensor, continuing until end is found. Advance to %s",
                            getHomingState(HomingState::HOMING_FINDING_END).c_str());
                        // Found the start of the sensor, keep going until we find the end
                        _homing.position[HOMING_START_PIN_POSITION] = _stepperRA->currentPosition();
                        _homing.lastPinState                        = homingPinState;
                        _homing.state                               = HomingState::HOMING_FINDING_END;
                    }
                }
                else
                {
                    // Did not find start. Go reverse direction for twice the distance
                    long distance = (long) (-_homing.initialDir * _stepsPerRADegree * siderealDegreesInHour * _homing.searchDistance * 2);
                    LOG(DEBUG_STEPPERS,
                        "[HOMING]: Hall not found on forward pass. Moving RA reverse by %dh (%l steps). Advance to %s",
                        2 * _homing.searchDistance,
                        distance,
                        getHomingState(HomingState::HOMING_FINDING_START_REVERSE).c_str());
                    moveStepperBy(StepperAxis::RA_STEPS, distance);
                    _homing.state = HomingState::HOMING_FINDING_START_REVERSE;
                }
            }
            break;

        case HomingState::HOMING_FINDING_START_REVERSE:
            {
                if (_stepperRA->isRunning())
                {
                    int homingPinState = digitalRead(RA_HOMING_SENSOR_PIN);
                    if (_homing.lastPinState != homingPinState)
                    {
                        LOG(DEBUG_STEPPERS,
                            "[HOMING]: Found start of sensor reverse, continuing until end is found. Advance to %s",
                            getHomingState(HomingState::HOMING_FINDING_END).c_str());
                        _homing.position[HOMING_START_PIN_POSITION] = _stepperRA->currentPosition();
                        _homing.lastPinState                        = homingPinState;
                        _homing.state                               = HomingState::HOMING_FINDING_END;
                    }
                }
                else
                {
                    // Did not find start in either direction, abort.
                    LOG(DEBUG_STEPPERS,
                        "[HOMING]: Sensor not found on reverse pass either. Homing Failed. Advance to %s",
                        getHomingState(HomingState::HOMING_FAILED).c_str());
                    _homing.state = HomingState::HOMING_FAILED;
                }
            }
            break;

        case HomingState::HOMING_FINDING_END:
            {
                if (_stepperRA->isRunning())
                {
                    int homingPinState = digitalRead(RA_HOMING_SENSOR_PIN);
                    if (_homing.lastPinState != homingPinState)
                    {
                        LOG(DEBUG_STEPPERS,
                            "[HOMING]: Found end of sensor, stopping... Advance to %s",
                            getHomingState(HomingState::HOMING_WAIT_FOR_STOP).c_str());
                        _homing.position[HOMING_END_PIN_POSITION] = _stepperRA->currentPosition();
                        _homing.lastPinState                      = homingPinState;
                        _stepperRA->stop();
                        _homing.state     = HomingState::HOMING_WAIT_FOR_STOP;
                        _homing.nextState = HomingState::HOMING_RANGE_FOUND;
                    }
                }
                else
                {
                    LOG(DEBUG_STEPPERS,
                        "[HOMING]: End of sensor not found! Advance to %s",
                        getHomingState(HomingState::HOMING_FAILED).c_str());
                    _homing.state = HomingState::HOMING_FAILED;
                }
            }
            break;

        case HomingState::HOMING_RANGE_FOUND:
            {
                LOG(DEBUG_STEPPERS,
                    "[HOMING]: Stepper stopped, Hall sensor found! Range: [%l to %l] size: %l",
                    _homing.position[HOMING_START_PIN_POSITION],
                    _homing.position[HOMING_END_PIN_POSITION],
                    _homing.position[HOMING_START_PIN_POSITION] - _homing.position[HOMING_END_PIN_POSITION]);

                long midPos = (_homing.position[HOMING_START_PIN_POSITION] + _homing.position[HOMING_END_PIN_POSITION]) / 2;

                LOG(DEBUG_STEPPERS,
                    "[HOMING]: Moving RA to home by %l - (%l) - (%l) steps. Advance to %s",
                    midPos,
                    _stepperRA->currentPosition(),
                    _homing.offsetRA,
                    getHomingState(HomingState::HOMING_WAIT_FOR_STOP).c_str());

                moveStepperBy(StepperAxis::RA_STEPS, midPos - _stepperRA->currentPosition() - _homing.offsetRA);

                _homing.state     = HomingState::HOMING_WAIT_FOR_STOP;
                _homing.nextState = HomingState::HOMING_SUCCESSFUL;
            }
            break;

        case HomingState::HOMING_SUCCESSFUL:
            {
                LOG(DEBUG_STEPPERS,
                    "[HOMING]: Successfully homed! Setting home and restoring Rate setting. Advance to %s",
                    getHomingState(HomingState::HOMING_NOT_ACTIVE).c_str());
                setHome(false);
                setSlewRate(_homing.savedRate);
                _homing.state = HomingState::HOMING_NOT_ACTIVE;
                _mountStatus &= ~STATUS_FINDING_HOME;
                startSlewing(TRACKING);
            }
            break;

        case HomingState::HOMING_FAILED:
            {
                LOG(DEBUG_STEPPERS,
                    "[HOMING]: Failed to home! Restoring Rate setting and slewing to start position. Advance to %s",
                    getHomingState(HomingState::HOMING_NOT_ACTIVE).c_str());
                setSlewRate(_homing.savedRate);
                _homing.state = HomingState::HOMING_NOT_ACTIVE;
                _mountStatus &= ~STATUS_FINDING_HOME;
                _mountStatus |= STATUS_SLEWING | STATUS_SLEWING_TO_TARGET;
                _stepperRA->moveTo(_homing.startPos);
            }
            break;

        default:
            LOG(DEBUG_STEPPERS, "[HOMING]: Unhandled state (%d)! ", _homing.state);
            break;
    }
}

/////////////////////////////////
//
// findRAHomeByHallSensor
//
/////////////////////////////////
bool Mount::findRAHomeByHallSensor(int initialDirection, int searchDistance)
{
    _homing.startPos       = _stepperRA->currentPosition();
    _homing.savedRate      = _moveRate;
    _homing.initialDir     = initialDirection;
    _homing.searchDistance = searchDistance;

    setSlewRate(4);

    _mountStatus |= STATUS_FINDING_HOME;

    // Check where we are over the sensor already
    if (digitalRead(RA_HOMING_SENSOR_PIN) == LOW)
    {
        _homing.state = HomingState::HOMING_MOVE_OFF;
        LOG(DEBUG_STEPPERS, "[HOMING]: Sensor is signalled, move off sensor started");
    }
    else
    {
        _homing.state = HomingState::HOMING_START_FIND_START;
        LOG(DEBUG_STEPPERS, "[HOMING]: Sensor is not signalled, find start of range");
    }

    return true;
}

#endif

/////////////////////////////////
//
// delay
//
/////////////////////////////////
void Mount::delay(int ms)
{
    unsigned long now = millis();
    while (millis() - now < (unsigned long) ms)
    {
        loop();
        yield();
    }
}

/////////////////////////////////
//
// interruptLoop()
//
// This function is run in an ISR. It needs to be fast and do little work.
/////////////////////////////////
void Mount::interruptLoop()
{
    if (_mountStatus & STATUS_GUIDE_PULSE)
    {
        _stepperTRK->runSpeed();
        if (_mountStatus & STATUS_GUIDE_PULSE_DEC)
        {
            _stepperGUIDE->runSpeed();
        }
        return;
    }

    if (_mountStatus & STATUS_TRACKING)
    {
        _stepperTRK->runSpeed();
    }

    if (_mountStatus & STATUS_SLEWING)
    {
        if (_mountStatus & STATUS_SLEWING_MANUAL)
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

#if USE_HALL_SENSOR_RA_AUTOHOME == 1
    if (_mountStatus & STATUS_FINDING_HOME)
    {
        _stepperRA->run();
        processRAHomingProgress();
    }
#endif

#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    _stepperAZ->run();
#endif
#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    _stepperALT->run();
#endif

#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    if (_focuserMode == FOCUS_TO_TARGET)
    {
        _stepperFocus->run();
    }
    else if (_focuserMode == FOCUS_CONTINUOUS)
    {
        _stepperFocus->runSpeed();
    }
#endif
#if DEW_HEATER == 1
    if(_heaters[0]) 
    {
        _heaters[0]->runLoop();
    }
    if(_heaters[1]) 
    {
        _heaters[1]->runLoop();
    }
#endif
}

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

    unsigned long now = millis();

#if (DEBUG_LEVEL & DEBUG_MOUNT) && (DEBUG_LEVEL & DEBUG_VERBOSE)
    if (now - _lastMountPrint > 2000)
    {
        LOG(DEBUG_MOUNT, "[MOUNT]: Status -> %s", getStatusString().c_str());
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

    if (_stepperFocus->isRunning())
    {
        LOG(DEBUG_FOCUS, "[MOUNT]: Loop: Focuser running at speed %f", _stepperFocus->speed());
        _focuserWasRunning = true;
    }
    else if (_focuserWasRunning)
    {
        LOG(DEBUG_FOCUS, "[MOUNT]: Loop: Focuser is stopped, but was running ");
        // If focuser was running last time through the loop, but not this time, it has
        // either been stopped, or reached the target.
        _focuserMode       = FOCUS_IDLE;
        _focuserWasRunning = false;
        LOG(DEBUG_FOCUS, "[MOUNT]: Loop: Focuser is stopped, but was running, disabling");
        disableFocusMotor();
    }

#endif

    if (isGuiding())
    {
        now                 = millis();
        bool stopRaGuiding  = now > _guideRaEndTime;
        bool stopDecGuiding = now > _guideDecEndTime;
        if (stopRaGuiding || stopDecGuiding)
        {
            stopGuiding(stopRaGuiding, stopDecGuiding);
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
        // Check whether we should stop tracking now
        checkRALimit();

        if (_mountStatus & STATUS_SLEWING_MANUAL)
        {
            if (_stepperWasRunning)
            {
                _mountStatus &= ~(STATUS_SLEWING);
            }
        }
        else
        {
            //
            // Arrived at target after Slew!
            //
            _mountStatus &= ~(STATUS_SLEWING | STATUS_SLEWING_TO_TARGET);

            if (_stepperWasRunning)
            {
                LOG(DEBUG_MOUNT | DEBUG_STEPPERS,
                    "[MOUNT]: Loop: Reached target. RA:%l, DEC:%l",
                    _stepperRA->currentPosition(),
                    _stepperDEC->currentPosition());
                // Mount is at Target!
                // If we we're parking, we just reached home. Clear the flag, reset the motors and stop tracking.
                if (isParking())
                {
                    stopSlewing(TRACKING);
                    // If we're on the second part of the slew to parking, don't set home here
                    if (!_slewingToPark)
                    {
                        LOG(DEBUG_MOUNT | DEBUG_STEPPERS, "[MOUNT]: Loop:   Was Parking, stop tracking and set home.");
                        setHome(false);
                    }
                    else
                    {
                        LOG(DEBUG_MOUNT | DEBUG_STEPPERS, "[MOUNT]: Loop:   Was Parking, stop tracking.");
                    }
                }

                _currentRAStepperPosition = _stepperRA->currentPosition();
#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
                if (!isFindingHome())  // When finding home, we never want to switch back to tracking until homing is finished.
                {
                    LOG(DEBUG_STEPPERS, "[STEPPERS]: Loop: Arrived. RA driver setMicrosteps(%d)", RA_TRACKING_MICROSTEPPING);
                    _driverRA->microsteps(RA_TRACKING_MICROSTEPPING == 1 ? 0 : RA_TRACKING_MICROSTEPPING);
                }
#endif
                if (!isParking())
                {
                    if (_compensateForTrackerOff)
                    {
                        now                             = millis();
                        unsigned long elapsed           = now - _trackerStoppedAt;
                        unsigned long compensationSteps = _trackingSpeed * elapsed / 1000.0f;
                        LOG(DEBUG_STEPPERS,
                            "[STEPPERS]: loop: Arrived at %lms. Tracking was off for %lms (%l steps), compensating.",
                            now,
                            elapsed,
                            compensationSteps);
                        _stepperTRK->runToNewPosition(_stepperTRK->currentPosition() + compensationSteps);
                        _compensateForTrackerOff = false;
                    }

                    if (!isFindingHome())  // If we're homing, RA must stay in Slew configuration
                    {
                        startSlewing(TRACKING);
                    }
                }

// Reset DEC to guide microstepping so that guiding is always ready and no switch is neccessary on guide pulses.
#if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
                LOG(DEBUG_STEPPERS, "[STEPPERS]: Loop: Arrived. DEC driver setMicrosteps(%d)", DEC_GUIDE_MICROSTEPPING);
                _driverDEC->microsteps(DEC_GUIDE_MICROSTEPPING == 1 ? 0 : DEC_GUIDE_MICROSTEPPING);
#endif

                if (_correctForBacklash)
                {
                    LOG(DEBUG_MOUNT | DEBUG_STEPPERS,
                        "[MOUNT]: Loop:   Reached target at %d. Compensating by %d",
                        (int) _currentRAStepperPosition,
                        _backlashCorrectionSteps);
                    _currentRAStepperPosition += _backlashCorrectionSteps;
                    _stepperRA->runToNewPosition(_currentRAStepperPosition);
                    _correctForBacklash = false;
                    LOG(DEBUG_MOUNT | DEBUG_STEPPERS, "[MOUNT]: Loop:   Backlash correction done. Pos: %d", _currentRAStepperPosition);
                }
                else
                {
                    LOG(DEBUG_MOUNT | DEBUG_STEPPERS,
                        "[MOUNT]: Loop:   Reached target at %d, no backlash compensation needed",
                        _currentRAStepperPosition);
                }

                if (_slewingToHome)
                {
                    LOG(DEBUG_MOUNT | DEBUG_STEPPERS, "[MOUNT]: Loop:   Was Slewing home, so setting stepper RA and TRK to zero.");
                    _stepperRA->setCurrentPosition(0);
                    _stepperDEC->setCurrentPosition(0);
                    LOG(DEBUG_STEPPERS, "[STEPPERS]: Loop:  TRK.setCurrentPos(0)");
                    _stepperTRK->setCurrentPosition(0);
                    _stepperGUIDE->setCurrentPosition(0);
                    _homeOffsetRA  = 0;
                    _homeOffsetDEC = 0;

                    _targetRA = currentRA();
                    if (isParking())
                    {
                        LOG(DEBUG_MOUNT | DEBUG_STEPPERS, "[MOUNT]: Loop:   Was parking, so no tracking. Proceeding to park position...");
                        _mountStatus &= ~STATUS_PARKING;
                        _slewingToPark = true;
                        _stepperRA->moveTo(_raParkingPos);
                        _stepperDEC->moveTo(_decParkingPos);
                        _totalDECMove = 1.0f * _stepperDEC->distanceToGo();
                        _totalRAMove  = 1.0f * _stepperRA->distanceToGo();
                        LOG(DEBUG_MOUNT | DEBUG_STEPPERS,
                            "[MOUNT]: Loop:   Park Position is R:%l  D:%l, TotalMove is R:%f, D:%f",
                            _raParkingPos,
                            _decParkingPos,
                            _totalRAMove,
                            _totalDECMove);
                        if ((_stepperDEC->distanceToGo() != 0) || (_stepperRA->distanceToGo() != 0))
                        {
                            _mountStatus |= STATUS_PARKING_POS | STATUS_SLEWING;
                        }
                    }
                    else
                    {
                        LOG(DEBUG_MOUNT | DEBUG_STEPPERS, "[MOUNT]: Loop:   Restart tracking.");
                        startSlewing(TRACKING);
                    }
                    _slewingToHome = false;
                }
                else if (_slewingToPark)
                {
                    LOG(DEBUG_MOUNT | DEBUG_STEPPERS, "[MOUNT]: Loop:   Arrived at park position...");
                    _mountStatus &= ~(STATUS_PARKING_POS | STATUS_SLEWING_TO_TARGET);
                    _slewingToPark = false;
                }
                _totalDECMove = _totalRAMove = 0;

                // Make sure we do one last update when the steppers have stopped.
                displayStepperPosition();
            }
        }
    }

    _stepperWasRunning = raStillRunning || decStillRunning;
}

/////////////////////////////////
//
// bootComplete
//
/////////////////////////////////
void Mount::bootComplete()
{
    _bootComplete = true;
#if USE_HALL_SENSOR_RA_AUTOHOME == 1
    _homing.pinState = _homing.lastPinState = digitalRead(RA_HOMING_SENSOR_PIN);
#endif
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
// setParkingPosition
//
/////////////////////////////////
void Mount::setParkingPosition()
{
    // Calculate how far the tracker has moved in the RA coordinate system.
    long trackedInSlewCoordinates = RA_SLEW_MICROSTEPPING * _stepperTRK->currentPosition() / RA_TRACKING_MICROSTEPPING;

    _raParkingPos = _stepperRA->currentPosition() - trackedInSlewCoordinates;
    // TODO: Take guide pulses on DEC into account
    _decParkingPos = _stepperDEC->currentPosition();

    LOG(DEBUG_MOUNT, "[MOUNT]: setParkingPos: parking RA: %l  DEC:%l", _raParkingPos, _decParkingPos);

    EEPROMStore::storeRAParkingPos(_raParkingPos);
    EEPROMStore::storeDECParkingPos(_decParkingPos);
}

/////////////////////////////////
//
// getDecParkingOffset
//
/////////////////////////////////
long Mount::getDecParkingOffset()
{
    return EEPROMStore::getDECParkingPos();
}

/////////////////////////////////
//
// setDecParkingOffset
//
/////////////////////////////////
void Mount::setDecParkingOffset(long offset)
{
    EEPROMStore::storeDECParkingPos(offset);
}

/////////////////////////////////
//
// setDecLimitPosition
//
/////////////////////////////////
void Mount::setDecLimitPosition(bool upper, float limitAngle)
{
    if (upper)
    {
        if (limitAngle == 0)
        {
            _decUpperLimit = _stepperDEC->currentPosition();
            EEPROMStore::storeDECUpperLimit(fabsf(_decUpperLimit / _stepsPerDECDegree));
        }
        else
        {
            _decUpperLimit = (limitAngle * _stepsPerDECDegree);
            EEPROMStore::storeDECUpperLimit(limitAngle);
        }
        LOG(DEBUG_MOUNT, "[MOUNT]: setDecLimitPosition(Upper): limit DEC: %l -> %l", _decLowerLimit, _decUpperLimit);
    }
    else
    {
        if (limitAngle == 0)
        {
            _decLowerLimit = _stepperDEC->currentPosition();
            EEPROMStore::storeDECLowerLimit(fabsf(_decLowerLimit / _stepsPerDECDegree));
        }
        else
        {
            _decLowerLimit = -(limitAngle * _stepsPerDECDegree);
            EEPROMStore::storeDECLowerLimit(limitAngle);
        }
        LOG(DEBUG_MOUNT, "[MOUNT]: setDecLimitPosition(Lower): limit DEC: %l -> %l", _decLowerLimit, _decUpperLimit);
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
        EEPROMStore::storeDECUpperLimit(DEC_LIMIT_UP);
        LOG(DEBUG_MOUNT, "[MOUNT]: clearDecLimitPosition(Upper): limit DEC: %l -> %l", _decLowerLimit, _decUpperLimit);
    }
    else
    {
        _decLowerLimit = -(DEC_LIMIT_DOWN * _stepsPerDECDegree);
        EEPROMStore::storeDECLowerLimit(DEC_LIMIT_DOWN);
        LOG(DEBUG_MOUNT, "[MOUNT]: clearDecLimitPosition(Lower): limit DEC: %l -> %l", _decLowerLimit, _decUpperLimit);
    }
}

/////////////////////////////////
//
// getDecLimitPositions
//
/////////////////////////////////
void Mount::getDecLimitPositions(long &lowerLimit, long &upperLimit)
{
    lowerLimit = _decLowerLimit;
    upperLimit = _decUpperLimit;
}

/////////////////////////////////
//
// setHome
//
/////////////////////////////////
void Mount::setHome(bool clearZeroPos)
{
    LOG(DEBUG_MOUNT, "[MOUNT]: setHome() called");
    //LOG(DEBUG_MOUNT_VERBOSE, "[MOUNT]: setHomePre: currentRA is %s", currentRA().ToString());
    //LOG(DEBUG_MOUNT_VERBOSE, "[MOUNT]: setHomePre: targetRA is %s", targetRA().ToString());
    //LOG(DEBUG_MOUNT_VERBOSE, "[MOUNT]: setHomePre: zeroPos is %s", _zeroPosRA.ToString());
    _zeroPosRA = clearZeroPos ? DayTime(POLARIS_RA_HOUR, POLARIS_RA_MINUTE, POLARIS_RA_SECOND) : calculateLst();
#ifdef OAM
    _zeroPosRA.addHours(6);  // shift allcoordinates by 90° for EQ mount movement
#endif
    _zeroPosDEC = 0.0f;

    _stepperRA->setCurrentPosition(0);
    _stepperDEC->setCurrentPosition(0);
    _stepperTRK->setCurrentPosition(0);
    // TODO: Set New Guide Stepper to 0

    _targetRA = currentRA();

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
        return _trackingSpeed;
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
    LOG(DEBUG_COORD_CALC, "[MOUNT]: CalcSteppersPre: Current : RA: %s, DEC: %s", currentRA().ToString(), currentDEC().ToString());
    LOG(DEBUG_COORD_CALC, "[MOUNT]: CalcSteppersPre: Target  : RA: %s, DEC: %s", _targetRA.ToString(), _targetDEC.ToString());
    LOG(DEBUG_COORD_CALC, "[MOUNT]: CalcSteppersPre: ZeroRA  : %s", _zeroPosRA.ToString());
    LOG(DEBUG_COORD_CALC, "[MOUNT]: CalcSteppersPre: ZeroDEC : %f", _zeroPosDEC);
    LOG(DEBUG_COORD_CALC,
        "[MOUNT]: CalcSteppersPre: Stepper: RA: %l, DEC: %l, TRK: %l",
        _stepperRA->currentPosition(),
        _stepperDEC->currentPosition(),
        _stepperTRK->currentPosition());
    DayTime raTarget      = _targetRA;
    Declination decTarget = _targetDEC;

    raTarget.subtractTime(_zeroPosRA);
    LOG(DEBUG_COORD_CALC,
        "[MOUNT]: CalcSteppersIn: Adjust RA by ZeroPosRA. New Target RA: %s, DEC: %s",
        raTarget.ToString(),
        _targetDEC.ToString());

    // Where do we want to move RA to?
    float moveRA = raTarget.getTotalHours();
    if (!NORTHERN_HEMISPHERE)
    {
        moveRA += 12;
    }

    // Total hours of tracking-to-date
    float trackedHours = (_stepperTRK->currentPosition() / _trackingSpeed) / 3600.0F;  // steps / steps/s / 3600 = hours
    LOG(DEBUG_COORD_CALC, "[MOUNT]: CalcSteppersIn: Tracked time is %l steps (%f h).", _stepperTRK->currentPosition(), trackedHours);

    // The current RA of the home position, taking tracking-to-date into account
    float homeRA = _zeroPosRA.getTotalHours() + trackedHours;

    // Delta between target RA and home position with a normalized range of -12 hr to 12 hr
    float homeTargetDeltaRA = _targetRA.getTotalHours() - homeRA;
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
        LOG(DEBUG_COORD_CALC,
            "[MOUNT]: CalcSteppersIn: moveRA>12 so -24. New Target RA: %s, DEC: %s",
            DayTime(moveRA).ToString(),
            _targetDEC.ToString());
    }

    // How many u-steps moves the RA ring one sidereal hour along when slewing. One sidereal hour moves just shy of 15 degrees
    float stepsPerSiderealHour = _stepsPerRADegree * siderealDegreesInHour;  // u-steps/deg * deg/hr = u-steps/hr

    // Where do we want to move DEC to?
    float moveDEC = decTarget.getTotalDegrees();

    LOG(DEBUG_COORD_CALC, "[MOUNT]: CalcSteppersIn: Target hrs pos RA: %f (regRA: %f), DEC: %f", homeTargetDeltaRA, moveRA, moveDEC);

    /*
  * Current RA wheel has a rotation limit of around 7 hours in each direction from home position.
  * Since tracking does not trigger the meridian flip, we try to extend the possible tracking time 
  * without reaching the RA ring end by executing the meridian flip before slewing to the target.
  * For this flip the RA and DEC rings have to be flipped by 180° (which is 12 RA hours). Based
  * on the physical RA ring limits, this means that the flip can only be executed during +/-[5h to 7h]
  * sections around the home position of RA. The tracking time will still be limited to around 2h in
  * worst case if the target is located right before the 5h mark during slewing. 
  */
#if NORTHERN_HEMISPHERE == 1
    float const RALimitL = -RA_LIMIT_LEFT;
    float const RALimitR = RA_LIMIT_RIGHT;
#else
    float const RALimitL = -RA_LIMIT_RIGHT;
    float const RALimitR = RA_LIMIT_LEFT;
#endif
    LOG(DEBUG_COORD_CALC, "[MOUNT]: CalcSteppersIn: Limits are : %f to %f", RALimitL, RALimitR);

    if (pSolutions != nullptr)
    {
        pSolutions[0] = long(-moveRA) * stepsPerSiderealHour;
        pSolutions[1] = long(moveDEC) * _stepsPerDECDegree;
        pSolutions[2] = long(-(moveRA - 12.0f)) * stepsPerSiderealHour;
        pSolutions[3] = long(-moveDEC) * _stepsPerDECDegree;
        pSolutions[4] = long(-(moveRA + 12.0f)) * stepsPerSiderealHour;
        pSolutions[5] = long(-moveDEC) * _stepsPerDECDegree;
    }

    LOG(DEBUG_COORD_CALC, "[MOUNT]: CalcSteppersIn: Solution 1: %f, %f", -moveRA, moveDEC);
    LOG(DEBUG_COORD_CALC, "[MOUNT]: CalcSteppersIn: Solution 2: %f, %f", -(moveRA - 12.0f), -moveDEC);
    LOG(DEBUG_COORD_CALC, "[MOUNT]: CalcSteppersIn: Solution 3: %f, %f", -(moveRA + 12.0f), -moveDEC);

    // If we reach the limit in the positive direction ...
    if (homeTargetDeltaRA > RALimitR)
    {
        LOG(DEBUG_COORD_CALC,
            "[MOUNT]: CalcSteppersIn: targetRA %f (RA:%f) is past max limit %f  (solution 2)",
            homeTargetDeltaRA,
            moveRA,
            RALimitR);

        // ... turn both RA and DEC axis around
        moveRA -= 12.0f;
        moveDEC = -moveDEC;
        LOG(DEBUG_COORD_CALC, "[MOUNT]: CalcSteppersIn: Adjusted Target. RA: %f, DEC: %f", moveRA, moveDEC);
    }
    // If we reach the limit in the negative direction...
    else if (homeTargetDeltaRA < RALimitL)
    {
        LOG(DEBUG_COORD_CALC,
            "[MOUNT]: CalcSteppersIn: targetRA %f (RA:%f) is past min limit: %f, (solution 3)",
            homeTargetDeltaRA,
            moveRA,
            RALimitL);
        // ... turn both RA and DEC axis around

        moveRA += 12.0f;
        moveDEC = -moveDEC;
        LOG(DEBUG_COORD_CALC, "[MOUNT]: CalcSteppersIn: Adjusted Target. RA: %f, DEC: %f", moveRA, moveDEC);
    }
    else
    {
        LOG(DEBUG_COORD_CALC,
            "[MOUNT]: CalcSteppersIn: targetRA %f is in range. RA: %f, DEC: %f  (solution 1)",
            homeTargetDeltaRA,
            moveRA,
            moveDEC);
    }

    moveDEC -= _zeroPosDEC;  // deg
    LOG(DEBUG_COORD_CALC, "[MOUNT]: CalcSteppersIn: _zeroPosDEC: %f", _zeroPosDEC);
    LOG(DEBUG_COORD_CALC, "[MOUNT]: CalcSteppersIn: Adjusted moveDEC: %f", moveDEC);

    targetRASteps  = -moveRA * stepsPerSiderealHour;
    targetDECSteps = moveDEC * _stepsPerDECDegree;
    LOG(DEBUG_COORD_CALC, "[MOUNT]: CalcSteppersPost: Target Steps RA: %l, DEC: %l", targetRASteps, targetDECSteps);
}

/////////////////////////////////
//
// moveSteppersTo
//
/////////////////////////////////
void Mount::moveSteppersTo(float targetRASteps, float targetDECSteps)
{  // Units are u-steps (in slew mode)
    // Show time: tell the steppers where to go!
    _correctForBacklash = false;
    LOG(DEBUG_STEPPERS, "[STEPPERS]: MoveSteppersTo: RA  From: %l  To: %f", _stepperRA->currentPosition(), targetRASteps);
    LOG(DEBUG_STEPPERS, "[STEPPERS]: MoveSteppersTo: DEC From: %l  To: %f", _stepperDEC->currentPosition(), targetDECSteps);

    if ((_backlashCorrectionSteps != 0) && ((_stepperRA->currentPosition() - targetRASteps) > 0))
    {
        LOG(DEBUG_STEPPERS, "[STEPPERS]: MoveSteppersTo: Needs backlash correction of %d!", _backlashCorrectionSteps);
        targetRASteps -= _backlashCorrectionSteps;
        _correctForBacklash = true;
    }

    _stepperRA->moveTo(targetRASteps);

    if (_decUpperLimit != 0)
    {
#if DEBUG_LEVEL > 0
        if (targetDECSteps > (float) _decUpperLimit)
        {
            LOG(DEBUG_STEPPERS, "[STEPPERS]: MoveSteppersTo: DEC Upper Limit enforced. To: %l", _decUpperLimit);
        }
#endif
        targetDECSteps = min(targetDECSteps, (float) _decUpperLimit);
    }

    if (_decLowerLimit != 0)
    {
#if DEBUG_LEVEL > 0
        if (targetDECSteps < (float) _decLowerLimit)
        {
            LOG(DEBUG_STEPPERS, "[STEPPERS]: MoveSteppersTo: DEC Lower Limit enforced. To: %l", _decLowerLimit);
        }
#endif
        targetDECSteps = max(targetDECSteps, (float) _decLowerLimit);
    }

    _stepperDEC->moveTo(targetDECSteps);
}

/////////////////////////////////
//
// moveStepperBy
//
/////////////////////////////////
void Mount::moveStepperBy(StepperAxis direction, long steps)
{
    LOG(DEBUG_STEPPERS, "[STEPPERS]: moveStepperBy: %l", steps);
    switch (direction)
    {
        case RA_STEPS:
            moveSteppersTo(_stepperRA->targetPosition() + steps, _stepperDEC->targetPosition());
            _mountStatus |= STATUS_SLEWING | STATUS_SLEWING_TO_TARGET;
            _totalRAMove = 1.0f * _stepperRA->distanceToGo();
            if ((_stepperRA->distanceToGo() != 0) || (_stepperDEC->distanceToGo() != 0))
            {
                // Only stop tracking if we're actually going to slew somewhere else, otherwise the
                // mount::loop() code won't detect the end of the slewing operation...
                LOG(DEBUG_STEPPERS, "[STEPPERS]: moveStepperBy: Stop tracking (NEMA steppers)");
                stopSlewing(TRACKING);
                _trackerStoppedAt        = millis();
                _compensateForTrackerOff = true;

// set Slew microsteps for TMC2209 UART once the TRK stepper has stopped
#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
                LOG(DEBUG_STEPPERS, "[STEPPERS]: moveStepperBy: Switching RA driver to microsteps(%d)", RA_SLEW_MICROSTEPPING);
                _driverRA->microsteps(RA_SLEW_MICROSTEPPING == 1 ? 0 : RA_SLEW_MICROSTEPPING);
#endif

                LOG(DEBUG_STEPPERS, "[STEPPERS]: moveStepperBy: TRK stopped at %lms", _trackerStoppedAt);
            }
            break;
        case DEC_STEPS:
            moveSteppersTo(_stepperRA->targetPosition(), _stepperDEC->targetPosition() + steps);
            _mountStatus |= STATUS_SLEWING | STATUS_SLEWING_TO_TARGET;
            _totalDECMove = 1.0f * _stepperDEC->distanceToGo();

#if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
            // Since normal state for DEC is guide microstepping, switch to slew microstepping here.
            LOG(DEBUG_STEPPERS, "[STEPPERS]: moveStepperBy: Switching DEC driver to microsteps(%d)", DEC_SLEW_MICROSTEPPING);
            _driverDEC->microsteps(DEC_SLEW_MICROSTEPPING == 1 ? 0 : DEC_SLEW_MICROSTEPPING);
#endif

            break;
        case FOCUS_STEPS:
#if FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE
            focusMoveBy(steps);
#endif
            break;
        case AZIMUTH_STEPS:
#if AZ_STEPPER_TYPE != STEPPER_TYPE_NONE
            enableAzAltMotors();
            LOG(DEBUG_STEPPERS,
                "[STEPPERS]: moveStepperBy: AZ from %l to %l",
                _stepperAZ->currentPosition(),
                _stepperAZ->currentPosition() + steps);
            _stepperAZ->moveTo(_stepperAZ->currentPosition() + steps);
#endif
            break;
        case ALTITUDE_STEPS:
#if ALT_STEPPER_TYPE != STEPPER_TYPE_NONE
            enableAzAltMotors();
            _stepperALT->moveTo(_stepperALT->currentPosition() + steps);
#endif
            break;
    }
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

    if ((fabsf(_totalDECMove) > 0.001f) && (fabsf(_totalRAMove) > 0.001f))
    {
        // Both axes moving to target
        float decDist = 100.0f - 100.0f * _stepperDEC->distanceToGo() / _totalDECMove;
        float raDist  = 100.0f - 100.0f * _stepperRA->distanceToGo() / _totalRAMove;

        sprintf(scratchBuffer, "R %s %d%%", RAString(LCD_STRING | CURRENT_STRING).c_str(), (int) raDist);
        _lcdMenu->setCursor(0, 0);
        _lcdMenu->printMenu(String(scratchBuffer));
        sprintf(scratchBuffer, "D%s %d%%", DECString(LCD_STRING | CURRENT_STRING).c_str(), (int) decDist);
        _lcdMenu->setCursor(0, 1);
        _lcdMenu->printMenu(String(scratchBuffer));
        return;
    }

    if (fabsf(_totalDECMove) > 0.001f)
    {
        // Only DEC moving to target
        float decDist = 100.0f - 100.0f * _stepperDEC->distanceToGo() / _totalDECMove;
        sprintf(scratchBuffer, "D%s %d%%", DECString(LCD_STRING | CURRENT_STRING).c_str(), (int) decDist);
        _lcdMenu->setCursor(0, 1);
        _lcdMenu->printMenu(String(scratchBuffer));
    }
    else if (fabsf(_totalRAMove) > 0.001f)
    {
        // Only RAmoving to target
        float raDist = 100.0f - 100.0f * _stepperRA->distanceToGo() / _totalRAMove;
        sprintf(scratchBuffer, "R %s %d%%", RAString(LCD_STRING | CURRENT_STRING).c_str(), (int) raDist);
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
            sprintf(scratchBuffer, " RA: %s", RAString(LCD_STRING | CURRENT_STRING).c_str());
            _lcdMenu->setCursor(0, 0);
            _lcdMenu->printMenu(scratchBuffer);
            sprintf(scratchBuffer, "DEC: %s", DECString(LCD_STRING | CURRENT_STRING).c_str());
            _lcdMenu->setCursor(0, 1);
            _lcdMenu->printMenu(scratchBuffer);
        }
        else
        {
            sprintf(scratchBuffer,
                    "R%s D%s",
                    RAString(COMPACT_STRING | CURRENT_STRING).c_str(),
                    DECString(COMPACT_STRING | CURRENT_STRING).c_str());
            _lcdMenu->setCursor(0, 1);
            _lcdMenu->printMenu(scratchBuffer);
        }
    #else
        sprintf(scratchBuffer,
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
    long elapsed = millis() - _lastDisplayUpdate;
    if (elapsed > DISPLAY_UPDATE_TIME)
    {
        displayStepperPosition();
        _lastDisplayUpdate = millis();
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
        //LOG(DEBUG_MOUNT_VERBOSE, "[MOUNT]: DECString: TARGET!");
        dec = _targetDEC;
    }
    else
    {
        //LOG(DEBUG_MOUNT_VERBOSE, "[MOUNT]: DECString: CURRENT!");
        dec = currentDEC();
    }
    //LOG(DEBUG_INFO, "[MOUNT]: DECString: Precheck  : %s   %s  %dm %ds", dec.ToString(), dec.getDegreesDisplay().c_str(), dec.getMinutes(), dec.getSeconds());
    // dec.checkHours();
    // LOG(DEBUG_MOUNT_VERBOSE, "[MOUNT]: DECString: Postcheck : %s", dec.ToString());

    dec.formatString(scratchBuffer, formatStringsDEC[type & FORMAT_STRING_MASK]);

    // sprintf(scratchBuffer, formatStringsDEC[type & FORMAT_STRING_MASK], dec.getDegreesDisplay().c_str(), dec.getMinutes(), dec.getSeconds());
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

    sprintf(scratchBuffer, formatStringsRA[type & FORMAT_STRING_MASK], ra.getHours(), ra.getMinutes(), ra.getSeconds());
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
    timeLocal.addSeconds((millis() - _localStartTimeSetMillis) / 1000);
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
    long secondsSinceSetDayEnd = ((millis() - _localStartTimeSetMillis) / 1000) - (86400 - _localStartTime.getTotalSeconds());

    while (secondsSinceSetDayEnd >= 0)
    {
        localDate.day++;
        secondsSinceSetDayEnd -= 86400;

        int maxDays = 31;
        switch (localDate.month)
        {
            case 2:
                if (((localDate.year % 4 == 0) && (localDate.year % 100 != 0)) || (localDate.year % 400 == 0))
                {
                    maxDays = 29;
                }
                else
                {
                    maxDays = 28;
                }
                break;

            case 4:
            case 6:
            case 9:
            case 11:
                maxDays = 30;
                break;
        }

        //calculate day overflow
        if (localDate.day > maxDays)
        {
            localDate.day = 1;
            localDate.month++;
        }
        //calculate year overflow
        if (localDate.month > 12)
        {
            localDate.month = 1;
            localDate.year++;
        }
    }

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
    _localStartTimeSetMillis = millis();

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
    EEPROMStore::storeUtcOffset(_localUtcOffset);

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
    DayTime lst = Sidereal::calculateByDateAndTime(longitude().getTotalHours(), localDate.year, localDate.month, localDate.day, &timeUTC);
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
    return Sidereal::calculateHa(lst.getTotalHours());
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
    LOG(DEBUG_STEPPERS, "[STEPPERS]: uartTest: Switching RA driver to microsteps(%d) for UART test", RA_SLEW_MICROSTEPPING);
    _driverRA->microsteps(RA_SLEW_MICROSTEPPING == 1 ? 0 : RA_SLEW_MICROSTEPPING);
    testUART_vactual(_driverRA, speed, duration);
    LOG(DEBUG_STEPPERS, "[STEPPERS]: uartTest: Switching RA driver to microsteps(%d) after UART test", RA_TRACKING_MICROSTEPPING);
    _driverRA->microsteps(RA_TRACKING_MICROSTEPPING == 1 ? 0 : RA_TRACKING_MICROSTEPPING);
}
    #endif

    #if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
void Mount::testDEC_UART_TX()
{
    //microsteps per driver clock tick (From TMC2209 datasheet: v[Hz] (microstep/s) = VACTUAL[2209] * 0.715Hz)
    const int speed = (DEC_STEPPER_SPEED / 2) / 0.715255737f;
    //Duration in ms to move X degrees at half of the max speed
    const int duration = UART_CONNECTION_TEST_TX_DEG * (_stepsPerDECDegree / (DEC_STEPPER_SPEED / 2)) * 1000;
    LOG(DEBUG_STEPPERS, "[STEPPERS]: uartTest: Switching DEC driver to microsteps(%d) for UART test", DEC_SLEW_MICROSTEPPING);
    _driverDEC->microsteps(DEC_SLEW_MICROSTEPPING == 1 ? 0 : DEC_SLEW_MICROSTEPPING);
    testUART_vactual(_driverDEC, speed, duration);
    LOG(DEBUG_STEPPERS, "[STEPPERS]: uartTest: Switching DEC driver to microsteps(%d) after UART test", DEC_GUIDE_MICROSTEPPING);
    _driverDEC->microsteps(DEC_GUIDE_MICROSTEPPING == 1 ? 0 : DEC_GUIDE_MICROSTEPPING);
}
    #endif

    #if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART || DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
/**
 * Runs motor at specified speed for specified duration in each direction, allowing 0.5s to stop motion after each move
 * @param[in,out] driver The stepper driver instance
 * @param[in] speed The speed to run the stepper motor at (in microsteps per driver clock tick)
 * @param[in] duration The amount of time to turn the stepper (in milliseconds)
 */
void Mount::testUART_vactual(TMC2209Stepper *driver, int _speed, int _duration)
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

/////////////////////////////////
//
// checkRALimit
//
/////////////////////////////////
void Mount::checkRALimit()
{
    // Check tracking limits every 5 seconds
    if (millis() - _lastTRKCheck < 5000)
        return;
    const float trackedHours = (_stepperTRK->currentPosition() / _trackingSpeed) / 3600.0F;  // steps / steps/s / 3600 = hours
    const float homeRA       = _zeroPosRA.getTotalHours() + trackedHours;
    const float RALimit      = RA_TRACKING_LIMIT;
    const float degreePos    = (_stepperDEC->currentPosition() / _stepsPerDECDegree) + _zeroPosDEC;
    float hourPos            = currentRA().getTotalHours();
    if (NORTHERN_HEMISPHERE ? degreePos < 0 : degreePos > 0)
    {
        hourPos -= 12;
        if (hourPos < 0)
            hourPos += 24;
    }
    LOG(DEBUG_MOUNT_VERBOSE, "[MOUNT]: checkRALimit: homeRA: %f", homeRA);
    LOG(DEBUG_MOUNT_VERBOSE, "[MOUNT]: checkRALimit: currentRA: %f", currentRA().getTotalHours());
    LOG(DEBUG_MOUNT_VERBOSE, "[MOUNT]: checkRALimit: currentRA (adjusted): %f", hourPos);
    float homeCurrentDeltaRA = homeRA - hourPos;
    while (homeCurrentDeltaRA > 12)
        homeCurrentDeltaRA -= 24;
    while (homeCurrentDeltaRA < -12)
        homeCurrentDeltaRA += 24;
    LOG(DEBUG_MOUNT_VERBOSE, "[MOUNT]: checkRALimit: homeRAdelta: %f", homeCurrentDeltaRA);

    if (homeCurrentDeltaRA > RALimit)
    {
        LOG(DEBUG_MOUNT, "[MOUNT]: checkRALimit: Tracking limit reached");
        stopSlewing(TRACKING);
    }
    _lastTRKCheck = millis();
}

#if DEW_HEATER == 1
/////////////////////////////////
//
// setHeater
//
/////////////////////////////////
void Mount::setHeater(unsigned num, unsigned val)
{
    if(num < 2)
    {
        if(_heaters[num])
        {
            _heaters[num]->setValue(val);
        }
        unsigned heaterValues[2] = {0, 0};
        if(_heaters[0])
        {
            heaterValues[0] = _heaters[0]->getValue();
        }
        if(_heaters[1])
        {
            heaterValues[1] = _heaters[1]->getValue();
        }
        LOG(DEBUG_GENERAL, "[MOUNT]: heater %d was set to %d", num, val);
        EEPROMStore::storeHeaterValues(heaterValues);
    }
    else
    {
        LOG(DEBUG_MOUNT, "[MOUNT]: wrong pin number %d", num);
    }
    return;
}

unsigned Mount::getHeater(unsigned num)
{
    if (num <= 2 && _heaters[num])
    {
        return _heaters[num]->getValue();
    }
    return 0;
}
#endif
