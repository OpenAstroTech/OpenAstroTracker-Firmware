#include "../Configuration.hpp"
#include "Utility.hpp"
#include "EPROMStore.hpp"
#include "LcdMenu.hpp"
#include "Mount.hpp"
#include "Sidereal.hpp"

PUSH_NO_WARNINGS
#include <AccelStepper.h>
#if (RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART) || (DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)                                          \
    || (AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART) || (ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
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

    _compensateForTrackerOff = false;
    _trackerStoppedAt        = 0;

    _totalDECMove  = 0;
    _totalRAMove   = 0;
    _homeOffsetRA  = 0;
    _homeOffsetDEC = 0;

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
    LOGV1(DEBUG_INFO, F("Mount: Reading configuration data from EEPROM"));
    readPersistentData();
    LOGV1(DEBUG_INFO, F("Mount: Done reading configuration data from EEPROM"));
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
    LOGV2(DEBUG_INFO, F("Mount: EEPROM: RA steps/deg is %f"), _stepsPerRADegree);

    _stepsPerDECDegree = EEPROMStore::getDECStepsPerDegree();
    LOGV2(DEBUG_INFO, F("Mount: EEPROM: DEC steps/deg is %f"), _stepsPerDECDegree);

    float speed = EEPROMStore::getSpeedFactor();
    LOGV2(DEBUG_INFO, F("Mount: EEPROM: Speed factor is %f"), speed);
    setSpeedCalibration(speed, false);

    _backlashCorrectionSteps = EEPROMStore::getBacklashCorrectionSteps();
    LOGV2(DEBUG_INFO, F("Mount: EEPROM: Backlash correction is %d"), _backlashCorrectionSteps);

    _latitude = EEPROMStore::getLatitude();
    LOGV2(DEBUG_INFO, F("Mount: EEPROM: Latitude is %s"), _latitude.ToString());

    _longitude = EEPROMStore::getLongitude();
    LOGV2(DEBUG_INFO, F("Mount: EEPROM: Longitude is %s"), _longitude.ToString());

    _localUtcOffset = EEPROMStore::getUtcOffset();
    LOGV2(DEBUG_INFO, F("Mount: EEPROM: UTC offset is %d"), _localUtcOffset);

#if USE_GYRO_LEVEL == 1
    _pitchCalibrationAngle = EEPROMStore::getPitchCalibrationAngle();
    LOGV2(DEBUG_INFO, F("Mount: EEPROM: Pitch Offset is %f"), _pitchCalibrationAngle);

    _rollCalibrationAngle = EEPROMStore::getRollCalibrationAngle();
    LOGV2(DEBUG_INFO, F("Mount: EEPROM: Roll Offset is %f"), _rollCalibrationAngle);
#endif

    _raParkingPos  = EEPROMStore::getRAParkingPos();
    _decParkingPos = EEPROMStore::getDECParkingPos();
    LOGV3(DEBUG_INFO, F("Mount: EEPROM: Parking position read as R:%l, D:%l"), _raParkingPos, _decParkingPos);

    _decLowerLimit = EEPROMStore::getDECLowerLimit();
    _decUpperLimit = EEPROMStore::getDECUpperLimit();
    LOGV3(DEBUG_INFO, F("Mount: EEPROM: DEC limits read as %l -> %l"), _decLowerLimit, _decUpperLimit);
}

/////////////////////////////////
//
// configureRAStepper
//
/////////////////////////////////
#if RA_STEPPER_TYPE == STEPPER_TYPE_28BYJ48
void Mount::configureRAStepper(byte pin1, byte pin2, byte pin3, byte pin4, int maxSpeed, int maxAcceleration)
{
    #if NORTHERN_HEMISPHERE
    _stepperRA = new AccelStepper((RA_SLEW_MICROSTEPPING == 1) ? AccelStepper::FULL4WIRE : AccelStepper::HALF4WIRE, pin4, pin3, pin2, pin1);
    #else
    _stepperRA = new AccelStepper((RA_SLEW_MICROSTEPPING == 1) ? AccelStepper::FULL4WIRE : AccelStepper::HALF4WIRE, pin1, pin2, pin3, pin4);
    #endif
    _stepperRA->setMaxSpeed(maxSpeed);
    _stepperRA->setAcceleration(maxAcceleration);
    _maxRASpeed        = maxSpeed;
    _maxRAAcceleration = maxAcceleration;

    // Use another AccelStepper to run the RA motor as well. This instance tracks earths rotation.
    #if NORTHERN_HEMISPHERE
    _stepperTRK
        = new AccelStepper((RA_TRACKING_MICROSTEPPING == 1) ? AccelStepper::FULL4WIRE : AccelStepper::HALF4WIRE, pin4, pin3, pin2, pin1);
    #else
    _stepperTRK
        = new AccelStepper((RA_TRACKING_MICROSTEPPING == 1) ? AccelStepper::FULL4WIRE : AccelStepper::HALF4WIRE, pin1, pin2, pin3, pin4);
    #endif
    _stepperTRK->setMaxSpeed(10000);
    _stepperTRK->setAcceleration(2500);
}
#endif

#if RA_STEPPER_TYPE == STEPPER_TYPE_NEMA17
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
#endif

/////////////////////////////////
//
// configureDECStepper
//
/////////////////////////////////
#if DEC_STEPPER_TYPE == STEPPER_TYPE_28BYJ48
void Mount::configureDECStepper(byte pin1, byte pin2, byte pin3, byte pin4, int maxSpeed, int maxAcceleration)
{
    #if NORTHERN_HEMISPHERE
    _stepperDEC
        = new AccelStepper((DEC_SLEW_MICROSTEPPING == 1) ? AccelStepper::FULL4WIRE : AccelStepper::HALF4WIRE, pin1, pin2, pin3, pin4);
    #else
    _stepperDEC
        = new AccelStepper((DEC_SLEW_MICROSTEPPING == 1) ? AccelStepper::FULL4WIRE : AccelStepper::HALF4WIRE, pin4, pin3, pin2, pin1);
    #endif
    _stepperDEC->setMaxSpeed(maxSpeed);
    _stepperDEC->setAcceleration(maxAcceleration);
    _maxDECSpeed        = maxSpeed;
    _maxDECAcceleration = maxAcceleration;

    _stepperGUIDE
        = new AccelStepper((DEC_GUIDE_MICROSTEPPING == 1) ? AccelStepper::FULL4WIRE : AccelStepper::HALF4WIRE, pin4, pin3, pin2, pin1);
    _stepperGUIDE->setMaxSpeed(10000);
    _stepperGUIDE->setAcceleration(6000);
}
#endif

#if DEC_STEPPER_TYPE == STEPPER_TYPE_NEMA17
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
#endif

/////////////////////////////////
//
// configureAZStepper / configureALTStepper
//
/////////////////////////////////
#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    #if AZ_DRIVER_TYPE == DRIVER_TYPE_ULN2003
void Mount::configureAZStepper(byte pin1, byte pin2, byte pin3, byte pin4, int maxSpeed, int maxAcceleration)
{
    _stepperAZ = new AccelStepper((AZ_MICROSTEPPING == 1) ? AccelStepper::FULL4WIRE : AccelStepper::HALF4WIRE, pin1, pin2, pin3, pin4);
    _stepperAZ->setSpeed(0);
    _stepperAZ->setMaxSpeed(maxSpeed);
    _stepperAZ->setAcceleration(maxAcceleration);
}
    #endif
    #if AZ_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC || AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE                                    \
        || AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
void Mount::configureAZStepper(byte pin1, byte pin2, int maxSpeed, int maxAcceleration)
{
    _stepperAZ = new AccelStepper(AccelStepper::DRIVER, pin1, pin2);
    _stepperAZ->setMaxSpeed(maxSpeed);
    _stepperAZ->setAcceleration(maxAcceleration);
    _maxAZSpeed        = maxSpeed;
    _maxAZAcceleration = maxAcceleration;
}
    #endif
#endif
#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    #if ALT_DRIVER_TYPE == DRIVER_TYPE_ULN2003
void Mount::configureALTStepper(byte pin1, byte pin2, byte pin3, byte pin4, int maxSpeed, int maxAcceleration)
{
    _stepperALT = new AccelStepper((ALT_MICROSTEPPING == 1) ? AccelStepper::FULL4WIRE : AccelStepper::HALF4WIRE, pin1, pin2, pin3, pin4);
    _stepperALT->setSpeed(0);
    _stepperALT->setMaxSpeed(maxSpeed);
    _stepperALT->setAcceleration(maxAcceleration);
}
    #endif
    #if ALT_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC || ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE                                  \
        || ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
void Mount::configureALTStepper(byte pin1, byte pin2, int maxSpeed, int maxAcceleration)
{
    _stepperALT = new AccelStepper(AccelStepper::DRIVER, pin1, pin2);
    _stepperALT->setMaxSpeed(maxSpeed);
    _stepperALT->setAcceleration(maxAcceleration);
    _maxALTSpeed        = maxSpeed;
    _maxALTAcceleration = maxAcceleration;
}
    #endif
#endif

/////////////////////////////////
//
// configureFocusStepper
//
/////////////////////////////////
#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    #if FOCUS_DRIVER_TYPE == DRIVER_TYPE_ULN2003
void Mount::configureFocusStepper(byte pin1, byte pin2, byte pin3, byte pin4, int maxSpeed, int maxAcceleration)
{
    _stepperFocus
        = new AccelStepper((FOCUS_MICROSTEPPING == 1) ? AccelStepper::FULL4WIRE : AccelStepper::HALF4WIRE, pin1, pin2, pin3, pin4);
    _stepperFocus->setSpeed(0);
    _stepperFocus->setMaxSpeed(maxSpeed);
    _stepperFocus->setAcceleration(maxAcceleration);
    _stepperFocus->setCurrentPosition(50000);
    _maxFocusRateSpeed = maxSpeed;
}
    #endif
    #if FOCUS_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC || FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE                              \
        || FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
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
#endif

#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART || DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART                                              \
    || AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART || ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    #if UART_CONNECTION_TEST_TXRX == 1
bool Mount::connectToDriver(TMC2209Stepper *driver, const char *driverKind)
{
    LOGV2(DEBUG_STEPPERS, F("Testing UART Connection to %s driver..."), driverKind);
    for (int i = 0; i < UART_CONNECTION_TEST_RETRIES; i++)
    {
        if (driver->test_connection() == 0)
        {
            LOGV2(DEBUG_STEPPERS, F("UART connection to %s driver successful."), driverKind);
            return true;
        }
        else
        {
            delay(500);
        }
    }
    LOGV2(DEBUG_STEPPERS, F("UART connection to %s driver failed."), driverKind);
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
    LOGV2(DEBUG_STEPPERS, F("Mount: Requested RA motor rms_current: %d mA"), rmscurrent);
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
        LOGV2(DEBUG_STEPPERS, F("Mount: Actual RA motor rms_current: %d mA"), _driverRA->rms_current());
        LOGV2(DEBUG_STEPPERS, F("Mount: Actual RA CS value: %d"), _driverRA->cs_actual());
        LOGV2(DEBUG_STEPPERS, F("Mount: Actual RA vsense: %d"), _driverRA->vsense());
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
    LOGV2(DEBUG_STEPPERS, F("Mount: Requested RA motor rms_current: %d mA"), rmscurrent);
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
        LOGV2(DEBUG_STEPPERS, F("Mount: Actual RA motor rms_current: %d mA"), _driverRA->rms_current());
        LOGV2(DEBUG_STEPPERS, F("Mount: Actual RA CS value: %d"), _driverRA->cs_actual());
        LOGV2(DEBUG_STEPPERS, F("Mount: Actual RA vsense: %d"), _driverRA->vsense());
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
    LOGV2(DEBUG_STEPPERS, F("Mount: Requested DEC motor rms_current: %d mA"), rmscurrent);
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
        LOGV2(DEBUG_STEPPERS, F("Mount: Actual DEC motor rms_current: %d mA"), _driverDEC->rms_current());
        LOGV2(DEBUG_STEPPERS, F("Mount: Actual DEC CS value: %d"), _driverDEC->cs_actual());
        LOGV2(DEBUG_STEPPERS, F("Mount: Actual DEC vsense: %d"), _driverDEC->vsense());
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
    LOGV2(DEBUG_STEPPERS, F("Mount: Requested DEC motor rms_current: %d mA"), rmscurrent);
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
        LOGV2(DEBUG_STEPPERS, F("Mount: Actual DEC motor rms_current: %d mA"), _driverDEC->rms_current());
        LOGV2(DEBUG_STEPPERS, F("Mount: Actual DEC CS value: %d"), _driverDEC->cs_actual());
        LOGV2(DEBUG_STEPPERS, F("Mount: Actual DEC vsense: %d"), _driverDEC->vsense());
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
    LOGV2(DEBUG_STEPPERS, F("Mount: Requested AZ motor rms_current: %d mA"), rmscurrent);
    _driverAZ->rms_current(rmscurrent, 1.0f);  //holdMultiplier = 1 to set ihold = irun
    _driverAZ->toff(1);
    _driverAZ->en_spreadCycle(0);
    _driverAZ->blank_time(24);
    _driverAZ->microsteps(AZ_MICROSTEPPING == 1 ? 0 : AZ_MICROSTEPPING);  // If 1 then disable microstepping
    _driverAZ->TCOOLTHRS(0xFFFFF);                                        //xFFFFF);
    _driverAZ->semin(0);                                                  //disable CoolStep so that current is consistent
    _driverAZ->SGTHRS(stallvalue);
    if (UART_Rx_connected)
    {
        LOGV2(DEBUG_STEPPERS, F("Mount: Actual AZ motor rms_current: %d mA"), _driverAZ->rms_current());
        LOGV2(DEBUG_STEPPERS, F("Mount: Actual AZ CS value: %d"), _driverAZ->cs_actual());
        LOGV2(DEBUG_STEPPERS, F("Mount: Actual AZ vsense: %d"), _driverAZ->vsense());
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
    LOGV2(DEBUG_STEPPERS, F("Mount: Requested AZ motor rms_current: %d mA"), rmscurrent);
    _driverAZ->rms_current(rmscurrent, 1.0f);  //holdMultiplier = 1 to set ihold = irun
    _driverAZ->toff(1);
    _driverAZ->en_spreadCycle(0);
    _driverAZ->blank_time(24);
    _driverAZ->microsteps(AZ_MICROSTEPPING == 1 ? 0 : AZ_MICROSTEPPING);  // If 1 then disable microstepping
    _driverAZ->TCOOLTHRS(0xFFFFF);                                        //xFFFFF);
    _driverAZ->semin(0);                                                  //disable CoolStep so that current is consistent
    _driverAZ->SGTHRS(stallvalue);
    if (UART_Rx_connected)
    {
        LOGV2(DEBUG_STEPPERS, F("Mount: Actual AZ motor rms_current: %d mA"), _driverAZ->rms_current());
        LOGV2(DEBUG_STEPPERS, F("Mount: Actual AZ CS value: %d"), _driverAZ->cs_actual());
        LOGV2(DEBUG_STEPPERS, F("Mount: Actual AZ vsense: %d"), _driverAZ->vsense());
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
    LOGV2(DEBUG_STEPPERS, F("Mount: Requested ALT motor rms_current: %d mA"), rmscurrent);
    _driverALT->rms_current(rmscurrent, 1.0f);  //holdMultiplier = 1 to set ihold = irun
    _driverALT->toff(1);
    _driverALT->en_spreadCycle(0);
    _driverALT->blank_time(24);
    _driverALT->microsteps(ALT_MICROSTEPPING == 1 ? 0 : ALT_MICROSTEPPING);  // If 1 then disable microstepping
    _driverALT->TCOOLTHRS(0xFFFFF);                                          //xFFFFF);
    _driverALT->semin(0);                                                    //disable CoolStep so that current is consistent
    _driverALT->SGTHRS(stallvalue);
    if (UART_Rx_connected)
    {
        LOGV2(DEBUG_STEPPERS, F("Mount: Actual ALT motor rms_current: %d mA"), _driverALT->rms_current());
        LOGV2(DEBUG_STEPPERS, F("Mount: Actual ALT CS value: %d"), _driverALT->cs_actual());
        LOGV2(DEBUG_STEPPERS, F("Mount: Actual ALT vsense: %d"), _driverALT->vsense());
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
    LOGV2(DEBUG_STEPPERS, F("Mount: Requested ALT motor rms_current: %d mA"), rmscurrent);
    _driverALT->rms_current(rmscurrent, 1.0f);  //holdMultiplier = 1 to set ihold = irun
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
        LOGV2(DEBUG_STEPPERS, F("Mount: Actual ALT motor rms_current: %d mA"), _driverALT->rms_current());
        LOGV2(DEBUG_STEPPERS, F("Mount: Actual ALT CS value: %d"), _driverALT->cs_actual());
        LOGV2(DEBUG_STEPPERS, F("Mount: Actual ALT vsense: %d"), _driverALT->vsense());
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
    LOGV2(DEBUG_STEPPERS, F("Mount: Requested Focus motor rms_current: %d mA"), rmscurrent);
    _driverFocus->rms_current(rmscurrent, 1.0f);  //holdMultiplier = 1 to set ihold = irun
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
        LOGV2(DEBUG_STEPPERS, F("Mount: Actual Focus motor rms_current: %d mA"), _driverFocus->rms_current());
        LOGV2(DEBUG_STEPPERS, F("Mount: Actual Focus CS value: %d"), _driverFocus->cs_actual());
        LOGV2(DEBUG_STEPPERS, F("Mount: Actual Focus vsense: %d"), _driverFocus->vsense());
    }
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
    UART_Rx_connected = connectToDriver(_driverFocus, "Focus");
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
    LOGV2(DEBUG_STEPPERS, F("Mount: Requested Focus motor rms_current: %d mA"), rmscurrent);
    _driverFocus->rms_current(rmscurrent, 1.0f);  //holdMultiplier = 1 to set ihold = irun
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
        LOGV2(DEBUG_STEPPERS, F("Mount: Actual Focus motor rms_current: %d mA"), _driverFocus->rms_current());
        LOGV2(DEBUG_STEPPERS, F("Mount: Actual Focus CS value: %d"), _driverFocus->cs_actual());
        LOGV2(DEBUG_STEPPERS, F("Mount: Actual Focus vsense: %d"), _driverFocus->vsense());
    }
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
    LOGV3(DEBUG_MOUNT, F("Mount: Updating speed calibration from %f to %f"), _trackingSpeedCalibration, val);
    _trackingSpeedCalibration = val;

    LOGV2(DEBUG_MOUNT, F("Mount: Current tracking speed is %f steps/sec"), _trackingSpeed);

    // Tracking speed has to be exactly the rotation speed of the earth. The earth rotates 360° per astronomical day.
    // This is 23h 56m 4.0905s, therefore the dimensionless _trackingSpeedCalibration = (23h 56m 4.0905s / 24 h) * mechanical calibration factor
    // Also compensate for higher precision microstepping in tracking mode
    _trackingSpeed = _trackingSpeedCalibration * _stepsPerRADegree * (RA_TRACKING_MICROSTEPPING / RA_SLEW_MICROSTEPPING) * 360.0f
                     / secondsPerDay;  // (fraction of day) * u-steps/deg * (u-steps/u-steps) * deg / (sec/day) = u-steps / sec
    LOGV2(DEBUG_MOUNT, F("Mount: RA steps per degree is %f steps/deg"), _stepsPerRADegree);
    LOGV2(DEBUG_MOUNT, F("Mount: New tracking speed is %f steps/sec"), _trackingSpeed);

    LOGV3(DEBUG_MOUNT, F("Mount: FactorToSpeed : %s, %s"), String(val, 6).c_str(), String(_trackingSpeed, 6).c_str());

    if (saveToStorage)
        EEPROMStore::storeSpeedFactor(_trackingSpeedCalibration);

    // If we are currently tracking, update the speed. No need to update microstepping mode
    if (isSlewingTRK())
    {
        LOGV2(DEBUG_STEPPERS, F("SpeedCal: TRK.setSpeed(%f)"), _trackingSpeed);
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

#if RA_DRIVER_TYPE == DRIVER_TYPE_ULN2003
    ret += "U";
#elif RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
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

#if DEC_DRIVER_TYPE == DRIVER_TYPE_ULN2003
    ret += "U";
#elif DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
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

#if RA_STEPPER_TYPE == STEPPER_TYPE_28BYJ48
    ret += F("28BYJ|");
#elif RA_STEPPER_TYPE == STEPPER_TYPE_NEMA17
    ret += F("NEMA|");
#else
    ret += "?|";
#endif
    ret += String(RA_PULLEY_TEETH) + "|";
    ret += String(RA_STEPPER_SPR) + ",";

#if DEC_STEPPER_TYPE == STEPPER_TYPE_28BYJ48
    ret += F("28BYJ|");
#elif DEC_STEPPER_TYPE == STEPPER_TYPE_NEMA17
    ret += F("NEMA|");
#else
    ret += "?|";
#endif

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
    LOGV3(DEBUG_MOUNT, F("Mount::setSlewRate: rate is %d -> %f"), _moveRate, speedFactor[_moveRate]);
    _stepperDEC->setMaxSpeed(speedFactor[_moveRate] * _maxDECSpeed);
    _stepperRA->setMaxSpeed(speedFactor[_moveRate] * _maxRASpeed);
    LOGV3(DEBUG_MOUNT, F("Mount::setSlewRate: new speeds are RA: %f  DEC: %f"), _stepperRA->maxSpeed(), _stepperDEC->maxSpeed());
}

/////////////////////////////////
//
// setHA
//
/////////////////////////////////
void Mount::setHA(const DayTime &haTime)
{
    LOGV2(DEBUG_MOUNT, F("Mount: setHA:  HA is %s"), haTime.ToString());
    DayTime lst = DayTime(POLARIS_RA_HOUR, POLARIS_RA_MINUTE, POLARIS_RA_SECOND);
    lst.addTime(haTime);
    setLST(lst);
    _lastHASet = millis();
}

/////////////////////////////////
//
// HA
//
/////////////////////////////////
const DayTime Mount::HA() const
{
    // LOGV1(DEBUG_MOUNT_VERBOSE, F("Mount: Get HA."));
    // LOGV2(DEBUG_MOUNT_VERBOSE, F("Mount: Polaris adjust: %s"), DayTime(POLARIS_RA_HOUR, POLARIS_RA_MINUTE, POLARIS_RA_SECOND).ToString());
    DayTime ha = _LST;
    // LOGV2(DEBUG_MOUNT_VERBOSE, F("Mount: LST: %s"), _LST.ToString());
    ha.subtractTime(DayTime(POLARIS_RA_HOUR, POLARIS_RA_MINUTE, POLARIS_RA_SECOND));
    // LOGV2(DEBUG_MOUNT, F("Mount: GetHA: LST-Polaris is HA %s"), ha.ToString());
    return ha;
}

/////////////////////////////////
//
// LST
//
/////////////////////////////////
const DayTime &Mount::LST() const
{
    return _LST;
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
    LOGV2(DEBUG_MOUNT, F("Mount: Set LST and ZeroPosRA to: %s"), _LST.ToString());
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

    LOGV4(DEBUG_MOUNT_VERBOSE,
          F("CurrentRA: Steps/h    : %s (%f x %s)"),
          String(stepsPerSiderealHour, 2).c_str(),
          _stepsPerRADegree,
          String(siderealDegreesInHour, 5).c_str());
    LOGV2(DEBUG_MOUNT_VERBOSE, F("CurrentRA: RA Steps   : %d"), _stepperRA->currentPosition());
    LOGV2(DEBUG_MOUNT_VERBOSE, F("CurrentRA: POS        : %s"), String(hourPos).c_str());
    hourPos += _zeroPosRA.getTotalHours();
    // LOGV2(DEBUG_MOUNT_VERBOSE,F("CurrentRA: ZeroPos    : %s"), _zeroPosRA.ToString());
    // LOGV2(DEBUG_MOUNT_VERBOSE,F("CurrentRA: POS (+zp)  : %s"), DayTime(hourPos).ToString());

    bool flipRA = _stepperDEC->currentPosition() < 0;
    if (flipRA)
    {
        hourPos += 12;
        if (hourPos > 24)
            hourPos -= 24;
        // LOGV2(DEBUG_MOUNT_VERBOSE,F("CurrentRA: RA (+12h): %s"), DayTime(hourPos).ToString());
    }

    // Make sure we are normalized
    if (hourPos < 0)
        hourPos += 24;
    if (hourPos > 24)
        hourPos -= 24;

    // LOGV2(DEBUG_MOUNT_VERBOSE,F("CurrentRA: RA Pos  -> : %s"), DayTime(hourPos).ToString());
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
    float degreePos = _stepperDEC->currentPosition() / _stepsPerDECDegree;  // u-steps / u-steps/deg = deg
    //LOGV2(DEBUG_MOUNT_VERBOSE,F("CurrentDEC: Steps/deg  : %f"), _stepsPerDECDegree);
    //LOGV2(DEBUG_MOUNT_VERBOSE,F("CurrentDEC: DEC Steps  : %d"), _stepperDEC->currentPosition());
    //LOGV2(DEBUG_MOUNT_VERBOSE,F("CurrentDEC: POS        : %s"), String(degreePos).c_str());

    if (NORTHERN_HEMISPHERE ? degreePos > 0 : degreePos < 0)
    {
        degreePos = -degreePos;
        //LOGV1(DEBUG_MOUNT_VERBOSE,F("CurrentDEC: Greater Zero, flipping."));
    }

    //LOGV2(DEBUG_MOUNT_VERBOSE,F("CurrentDEC: POS      : %s"), Declination(degreePos).ToString());
    return degreePos;
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
    _targetRA  = ra;
    _targetDEC = dec;

    long targetRAPosition, targetDECPosition;
    LOGV3(DEBUG_MOUNT, "Mount: Sync Position to RA: %s and DEC: %s", _targetRA.ToString(), _targetDEC.ToString());
    calculateRAandDECSteppers(targetRAPosition, targetDECPosition, solutions);

    LOGV3(DEBUG_STEPPERS, F("STEP-syncPosition: Solution 1: RA %l and DEC: %l"), solutions[0], solutions[1]);
    LOGV3(DEBUG_STEPPERS, F("STEP-syncPosition: Solution 2: RA %l and DEC: %l"), solutions[2], solutions[3]);
    LOGV3(DEBUG_STEPPERS, F("STEP-syncPosition: Solution 3: RA %l and DEC: %l"), solutions[4], solutions[5]);
    LOGV3(DEBUG_STEPPERS, F("STEP-syncPosition: Chose solution RA: %l and DEC: %l"), targetRAPosition, targetDECPosition);

    long raMove  = targetRAPosition - _stepperRA->currentPosition();
    long decMove = targetDECPosition - _stepperDEC->currentPosition();
    LOGV3(DEBUG_STEPPERS, F("STEP-syncPosition: Moving steppers by RA: %l and DEC: %l"), raMove, decMove);
    _homeOffsetRA -= raMove;
    _homeOffsetDEC -= decMove;
    _stepperRA->setCurrentPosition(targetRAPosition);    // u-steps (in slew mode)
    _stepperDEC->setCurrentPosition(targetDECPosition);  // u-steps (in slew mode)
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
    if (isGuiding())
    {
        stopGuiding();
    }

    // Make sure we're slewing at full speed on a GoTo
    LOGV2(DEBUG_STEPPERS, F("STEP-startSlewingToTarget: Set DEC to MaxSpeed(%d)"), _maxDECSpeed);
    _stepperDEC->setMaxSpeed(_maxDECSpeed);
    LOGV2(DEBUG_STEPPERS, F("STEP-startSlewingToTarget: Set RA  to MaxSpeed(%d)"), _maxRASpeed);
    _stepperRA->setMaxSpeed(_maxRASpeed);

    // Calculate new RA stepper target (and DEC). We are never in guding mode here.
    _currentDECStepperPosition = _stepperDEC->currentPosition();
    _currentRAStepperPosition  = _stepperRA->currentPosition();
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
    LOGV3(DEBUG_MOUNT, "Mount: RA Dist: %l,   DEC Dist: %l", _stepperRA->distanceToGo(), _stepperDEC->distanceToGo());
#if RA_STEPPER_TYPE == STEPPER_TYPE_NEMA17  // tracking while slewing causes problems (can only run one AccelStepper at a time)
    if ((_stepperRA->distanceToGo() != 0) || (_stepperDEC->distanceToGo() != 0))
    {
        // Only stop tracking if we're actually going to slew somewhere else, otherwise the
        // mount::loop() code won't detect the end of the slewing operation...
        LOGV1(DEBUG_STEPPERS, "Mount: Stop tracking (NEMA steppers)");
        stopSlewing(TRACKING);
        _trackerStoppedAt        = millis();
        _compensateForTrackerOff = true;

    // set Slew microsteps for TMC2209 UART once the TRK stepper has stopped
    #if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
        LOGV2(DEBUG_STEPPERS, F("STEP-startSlewingToTarget: Switching RA driver to microsteps(%d)"), RA_SLEW_MICROSTEPPING);
        _driverRA->microsteps(RA_SLEW_MICROSTEPPING == 1 ? 0 : RA_SLEW_MICROSTEPPING);
    #endif

        LOGV2(DEBUG_STEPPERS, F("STEP-startSlewingToTarget: TRK stopped at %lms"), _trackerStoppedAt);
    }
#endif

#if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    // Since normal state for DEC is guide microstepping, switch to slew microstepping here.
    LOGV2(DEBUG_STEPPERS, F("STEP-startSlewingToTarget: Switching DEC driver to microsteps(%d)"), DEC_SLEW_MICROSTEPPING);
    _driverDEC->microsteps(DEC_SLEW_MICROSTEPPING == 1 ? 0 : DEC_SLEW_MICROSTEPPING);
#endif
}

/////////////////////////////////
//
// stopGuiding
//
/////////////////////////////////
void Mount::stopGuiding()
{
    stopGuiding(true, true);
}

void Mount::stopGuiding(bool ra, bool dec)
{
    // Stop RA guide first, since it's just a speed change back to tracking speed
    if (ra && (_mountStatus & STATUS_GUIDE_PULSE_RA))
    {
        LOGV2(DEBUG_STEPPERS, F("STEP-stopGuiding(RA): TRK.setSpeed(%f)"), _trackingSpeed);
        _stepperTRK->setSpeed(_trackingSpeed);
        _mountStatus &= ~STATUS_GUIDE_PULSE_RA;
    }

    if (dec && (_mountStatus & STATUS_GUIDE_PULSE_DEC))
    {
        LOGV1(DEBUG_STEPPERS, F("STEP-stopGuiding(DEC): Stop motor"));

        // Stop DEC guiding and wait for it to stop.
        _stepperGUIDE->stop();

        while (_stepperGUIDE->isRunning())
        {
            _stepperGUIDE->run();
            _stepperTRK->runSpeed();
        }

        LOGV2(DEBUG_STEPPERS, F("STEP-stopGuiding(DEC): GuideStepper stopped at %l"), _stepperGUIDE->currentPosition());

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
    LOGV3(DEBUG_STEPPERS, F("STEP-guidePulse: > Guide Pulse %d for %dms"), direction, duration);

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
            LOGV2(DEBUG_STEPPERS, F("STEP-guidePulse:  DEC.setSpeed(%f)"), DEC_PULSE_MULTIPLIER * decGuidingSpeed);
            _stepperGUIDE->setSpeed(DEC_PULSE_MULTIPLIER * decGuidingSpeed);
            _mountStatus |= STATUS_GUIDE_PULSE | STATUS_GUIDE_PULSE_DEC;
            _guideDecEndTime = millis() + duration;
            break;

        case SOUTH:
            LOGV2(DEBUG_STEPPERS, F("STEP-guidePulse:  DEC.setSpeed(%f)"), -DEC_PULSE_MULTIPLIER * decGuidingSpeed);
            _stepperGUIDE->setSpeed(-DEC_PULSE_MULTIPLIER * decGuidingSpeed);
            _mountStatus |= STATUS_GUIDE_PULSE | STATUS_GUIDE_PULSE_DEC;
            _guideDecEndTime = millis() + duration;
            break;

        case WEST:
            // We were in tracking mode before guiding, so no need to update microstepping mode on RA driver
            LOGV2(DEBUG_STEPPERS, F("STEP-guidePulse:  TRK.setSpeed(%f)"), (RA_PULSE_MULTIPLIER * raGuidingSpeed));
            _stepperTRK->setSpeed(RA_PULSE_MULTIPLIER * raGuidingSpeed);  // Faster than siderael
            _mountStatus |= STATUS_GUIDE_PULSE | STATUS_GUIDE_PULSE_RA;
            _guideRaEndTime = millis() + duration;
            break;

        case EAST:
            // We were in tracking mode before guiding, so no need to update microstepping mode on RA driver
            LOGV2(DEBUG_STEPPERS, F("STEP-guidePulse:  TRK.setSpeed(%f)"), (raGuidingSpeed * (2.0f - RA_PULSE_MULTIPLIER)));
            _stepperTRK->setSpeed(raGuidingSpeed * (2.0f - RA_PULSE_MULTIPLIER));  // Slower than siderael
            _mountStatus |= STATUS_GUIDE_PULSE | STATUS_GUIDE_PULSE_RA;
            _guideRaEndTime = millis() + duration;
            break;
    }

    LOGV1(DEBUG_STEPPERS, F("STEP-guidePulse: < Guide Pulse"));
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
        LOGV2(DEBUG_STEPPERS, F("STEP-setManualSlewMode: Switching RA driver to microsteps(%d)"), RA_SLEW_MICROSTEPPING);
        _driverRA->microsteps(RA_SLEW_MICROSTEPPING == 1 ? 0 : RA_SLEW_MICROSTEPPING);
#endif
    }
    else
    {
        _mountStatus &= ~STATUS_SLEWING_MANUAL;
        stopSlewing(ALL_DIRECTIONS);
        waitUntilStopped(ALL_DIRECTIONS);
        LOGV3(DEBUG_STEPPERS, F("STEP-setManualSlewMode: Set RA  speed/accel:  %f  / %f"), _maxRASpeed, _maxRAAcceleration);
        LOGV3(DEBUG_STEPPERS, F("STEP-setManualSlewMode: Set DEC speed/accel:  %f  / %f"), _maxDECSpeed, _maxDECAcceleration);
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
        LOGV3(DEBUG_STEPPERS, F("STEP-setSpeed: Set RA speed %f degs/s, which is %f steps/s"), speedDegsPerSec, stepsPerSec);
        // TODO: Are we already in slew mode?
        _stepperRA->setSpeed(stepsPerSec);
    }
    else if (which == DEC_STEPS)
    {
        float stepsPerSec = speedDegsPerSec * _stepsPerDECDegree;  // deg/sec * u-steps/deg = u-steps/sec
        LOGV3(DEBUG_STEPPERS, F("STEP-setSpeed: Set DEC speed %f degs/s, which is %f steps/s"), speedDegsPerSec, stepsPerSec);
        // TODO: Are we already in slew mode?
        _stepperDEC->setSpeed(stepsPerSec);
    }
#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    else if (which == AZIMUTH_STEPS)
    {
    #if AZ_DRIVER_TYPE == DRIVER_TYPE_ULN2003
        float curAzSpeed = _stepperAZ->speed();

        // If we are changing directions or asking for a stop, do a stop
        if ((signbit(speedDegsPerSec) != signbit(curAzSpeed)) || (speedDegsPerSec == 0))
        {
            _stepperAZ->stop();
            while (_stepperAZ->isRunning())
            {
                loop();
            }
        }

        // Are we starting a move or changing speeds?
        if (speedDegsPerSec != 0)
        {
            _stepperAZ->enableOutputs();
            _stepperAZ->setSpeed(speedDegsPerSec);
            _stepperAZ->move(speedDegsPerSec * 100000);
        }  // Are we stopping a move?
        else if (speedDegsPerSec == 0)
        {
            _stepperAZ->disableOutputs();
        }
    #elif AZ_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC || AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE                                  \
        || AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
        float stepsPerSec = speedDegsPerSec * _stepsPerAZDegree;  // deg/sec * u-steps/deg = u-steps/sec
        LOGV3(DEBUG_STEPPERS, F("STEP-setSpeed: Set AZ speed %f degs/s, which is %f steps/s"), speedDegsPerSec, stepsPerSec);
        _stepperAZ->setSpeed(stepsPerSec);
    #endif
    }
#endif
#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    else if (which == ALTITUDE_STEPS)
    {
    #if ALT_DRIVER_TYPE == DRIVER_TYPE_ULN2003
        float curAltSpeed = _stepperALT->speed();

        // If we are changing directions or asking for a stop, do a stop
        if ((signbit(speedDegsPerSec) != signbit(curAltSpeed)) || (speedDegsPerSec == 0))
        {
            _stepperALT->stop();
            while (_stepperALT->isRunning())
            {
                loop();
            }
        }

        // Are we starting a move or changing speeds?
        if (speedDegsPerSec != 0)
        {
            _stepperALT->enableOutputs();
            _stepperALT->setSpeed(speedDegsPerSec);
            _stepperALT->move(speedDegsPerSec * 100000);
        }  // Are we stopping a move?
        else if (speedDegsPerSec == 0)
        {
            _stepperALT->disableOutputs();
        }
    #elif ALT_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC || ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE                                \
        || ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
        float stepsPerSec = speedDegsPerSec * _stepsPerALTDegree;  // deg/sec * u-steps/deg = u-steps/sec
        LOGV3(DEBUG_STEPPERS, F("STEP-setSpeed: Set ALT speed %f degs/s, which is %f steps/s"), speedDegsPerSec, stepsPerSec);
        _stepperALT->setSpeed(stepsPerSec);
    #endif
    }
#endif

#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    else if (which == FOCUS_STEPS)
    {
        LOGV2(DEBUG_MOUNT, F("Mount: Focuser setSpeed %f"), speedDegsPerSec);

    #if FOCUS_DRIVER_TYPE == DRIVER_TYPE_ULN2003
        float curFocusSpeed = _stepperFocus->speed();

        // If we are changing directions or asking for a stop, do a stop
        if ((signbit(speedDegsPerSec) != signbit(curFocusSpeed)) || (speedDegsPerSec == 0))
        {
            _stepperFocus->stop();
            waitUntilStopped(FOCUSING);
        }

        // Are we starting a move or changing speeds?
        if (speedDegsPerSec != 0)
        {
            enableFocusMotor();
            _stepperFocus->setMaxSpeed(speedDegsPerSec);
            _stepperFocus->moveTo(sign(speedDegsPerSec) * 300000);
            _focuserMode = FOCUS_TO_TARGET;
        }  // Are we stopping a move?
        else if (speedDegsPerSec == 0)
        {
            _stepperFocus->stop();
        }
    #elif FOCUS_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC || FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE                            \
        || FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
        if (speedDegsPerSec != 0)
        {
            LOGV2(DEBUG_STEPPERS, F("STEP-setSpeed: Enabling motor and setting speed. Continuous"), speedDegsPerSec);
            enableFocusMotor();
            _stepperFocus->setMaxSpeed(speedDegsPerSec);
            _stepperFocus->moveTo(sign(speedDegsPerSec) * 300000);
            _focuserMode = FOCUS_TO_TARGET;
        }
        else
        {
            LOGV2(DEBUG_STEPPERS, F("STEP-setSpeed: Stopping motor."), speedDegsPerSec);
            _stepperFocus->stop();
        }

    #endif
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
    setTargetToHome();
    startSlewingToTarget();
    _mountStatus |= STATUS_PARKING;
}

/////////////////////////////////
//
// goHome
//
// Synchronously moves mount to home position
/////////////////////////////////
void Mount::goHome()
{
    stopGuiding();
    setTargetToHome();
    startSlewingToTarget();
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
        #if AZ_DRIVER_TYPE == DRIVER_TYPE_ULN2003
        int stepsToMove = arcMinutes * AZIMUTH_STEPS_PER_ARC_MINUTE * AZ_MICROSTEPPING;
        #else
        int stepsToMove = arcMinutes * AZIMUTH_STEPS_PER_ARC_MINUTE;
        #endif
        _stepperAZ->move(stepsToMove);
    }
    #endif
    #if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    if (direction == ALTITUDE_STEPS)
    {
        enableAzAltMotors();
        #if ALT_DRIVER_TYPE == DRIVER_TYPE_ULN2003
        int stepsToMove = arcMinutes * ALTITUDE_STEPS_PER_ARC_MINUTE * ALT_MICROSTEPPING;
        #else
        int stepsToMove = arcMinutes * ALTITUDE_STEPS_PER_ARC_MINUTE;
        #endif

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

    #if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
        #if AZ_DRIVER_TYPE == DRIVER_TYPE_ULN2003
    _stepperAZ->disableOutputs();
        #else
    digitalWrite(AZ_EN_PIN, HIGH);     // Logic HIGH to disable driver
        #endif
    #endif

    #if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
        #if ALT_DRIVER_TYPE == DRIVER_TYPE_ULN2003
    _stepperALT->disableOutputs();
        #else
    digitalWrite(ALT_EN_PIN, HIGH);    // Logic HIGH to disable driver
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
        #if AZ_DRIVER_TYPE == DRIVER_TYPE_ULN2003
    _stepperAZ->enableOutputs();
        #else
    digitalWrite(AZ_EN_PIN, LOW);      // Logic LOW to enable driver
        #endif
    #endif

    #if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
        #if ALT_DRIVER_TYPE == DRIVER_TYPE_ULN2003
    _stepperALT->enableOutputs();
        #else
    digitalWrite(ALT_EN_PIN, LOW);     // Logic LOW to enable driver
        #endif
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
    LOGV3(DEBUG_MOUNT, F("Mount::focusSetSpeedByRate: rate is %d -> %f"), _focusRate, _maxFocusRateSpeed);
    _stepperFocus->setMaxSpeed(_maxFocusRateSpeed);

    if (_stepperFocus->isRunning())
    {
        LOGV1(DEBUG_MOUNT, F("Mount::focusSetSpeedByRate: stepper is already running so adjust speed"));
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
    LOGV3(DEBUG_MOUNT, F("Mount::focusMoveBy: move by %l steps to %l. Target Mode."), steps, targetPosition);
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
    LOGV1(DEBUG_MOUNT, F("Mount::disableFocusMotor: stopping motor and waiting."));
    _stepperFocus->stop();
    waitUntilStopped(FOCUSING);

    LOGV1(DEBUG_MOUNT, F("Mount::disableFocusMotor: disabling motor"));
    #if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
        #if FOCUS_DRIVER_TYPE == DRIVER_TYPE_ULN2003
    _stepperFocus->disableOutputs();
        #else
    digitalWrite(FOCUS_EN_PIN, HIGH);  // Logic HIGH to disable driver
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
    LOGV1(DEBUG_MOUNT, F("Mount::enableFocusMotor: enabling."));
    #if FOCUS_DRIVER_TYPE == DRIVER_TYPE_ULN2003
    _stepperFocus->enableOutputs();
    #else
    digitalWrite(FOCUS_EN_PIN, LOW);  // Logic LOW to enable driver
    #endif
}

/////////////////////////////////
//
// focusStop
//
/////////////////////////////////
void Mount::focusStop()
{
    LOGV1(DEBUG_MOUNT, F("Mount::focusStop: stopping motor."));
    _stepperFocus->stop();
}

#endif

/////////////////////////////////
//
// mountStatus
//
/////////////////////////////////
byte Mount::mountStatus()
{
    return _mountStatus;
}

#if DEBUG_LEVEL & (DEBUG_MOUNT | DEBUG_MOUNT_VERBOSE)
/////////////////////////////////
//
// mountStatusString
//
/////////////////////////////////
String Mount::mountStatusString()
{
    if (_mountStatus == STATUS_PARKED)
    {
        return "PARKED";
    }
    String disp = "";
    if (_mountStatus & STATUS_PARKING)
    {
        disp = "PARKNG ";
    }
    else if (isGuiding())
    {
        disp = "GUIDING ";
    }
    else
    {
        if (_mountStatus & STATUS_TRACKING)
            disp += "TRK ";
        if (_mountStatus & STATUS_SLEWING)
            disp += "SLW-";
        if (_mountStatus & STATUS_SLEWING_TO_TARGET)
            disp += "2TRG ";
        if (_mountStatus & STATUS_SLEWING_FREE)
            disp += "FR ";
        if (_mountStatus & STATUS_SLEWING_MANUAL)
            disp += "MAN ";

        if (_mountStatus & STATUS_SLEWING)
        {
            byte slew = slewStatus();
            if (slew & SLEWING_RA)
                disp += " SRA ";
            if (slew & SLEWING_DEC)
                disp += " SDEC ";
            if (slew & SLEWING_TRACKING)
                disp += " STRK ";
        }
    }

    disp += " RA:" + String(_stepperRA->currentPosition());
    disp += " DEC:" + String(_stepperDEC->currentPosition());
    disp += " TRK:" + String(_stepperTRK->currentPosition());

    return disp;
}
#endif

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
    else if (isGuiding())
    {
        status = F("Guiding,");
    }
    else if (isFindingHome())
    {
        status = F("Homing,");
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
// isSlewingDEC
//
/////////////////////////////////
bool Mount::isSlewingDEC() const
{
    if (isParking())
        return true;
    return (slewStatus() & SLEWING_DEC) != 0;
}

/////////////////////////////////
//
// isSlewingRA
//
/////////////////////////////////
bool Mount::isSlewingRA() const
{
    if (isParking())
        return true;
    return (slewStatus() & SLEWING_RA) != 0;
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
// isParked
//
/////////////////////////////////
bool Mount::isParked() const
{
    return (slewStatus() == NOT_SLEWING) && (_mountStatus == STATUS_PARKED);
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
        if (isGuiding())
        {
            stopGuiding();
        }

        if (direction & TRACKING)
        {
// Start tracking
#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
            LOGV2(DEBUG_STEPPERS, F("STEP-startSlewing: Tracking: Switching RA driver to microsteps(%d)"), RA_TRACKING_MICROSTEPPING);
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
#if RA_STEPPER_TYPE == STEPPER_TYPE_NEMA17
            if (isSlewingTRK())
            {
                stopSlewing(TRACKING);
                _trackerStoppedAt        = millis();
                _compensateForTrackerOff = true;
                LOGV2(DEBUG_STEPPERS, F("STEP-startSlewing: stopped TRK at %l"), _trackerStoppedAt);
            }
#endif

// Change microstep mode for slewing
#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
            LOGV2(DEBUG_STEPPERS, F("STEP-startSlewing: Slewing: Switching RA driver to microsteps(%d)"), RA_SLEW_MICROSTEPPING);
            _driverRA->microsteps(RA_SLEW_MICROSTEPPING == 1 ? 0 : RA_SLEW_MICROSTEPPING);
#endif
#if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
            // Since normal state for DEC is guide microstepping, switch to slew microstepping here.
            LOGV2(DEBUG_STEPPERS, F("STEP-startSlewing: Slewing: Switching DEC driver to microsteps(%d)"), DEC_SLEW_MICROSTEPPING);
            _driverDEC->microsteps(DEC_SLEW_MICROSTEPPING == 1 ? 0 : DEC_SLEW_MICROSTEPPING);
#endif

            if (direction & NORTH)
            {
                long targetLocation = 300000;
                if (_decUpperLimit != 0)
                {
                    targetLocation = _decUpperLimit;
                    LOGV3(DEBUG_STEPPERS,
                          F("STEP-startSlewing(N): DEC has upper limit of %l. targetMoveTo is now %l"),
                          _decUpperLimit,
                          targetLocation);
                }
                else
                {
                    LOGV2(DEBUG_STEPPERS, F("STEP-startSlewing(N): initial targetMoveTo is %l"), targetLocation);
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
                    LOGV3(DEBUG_STEPPERS,
                          F("STEP-startSlewing(S): DEC has lower limit of %l. targetMoveTo is now %l"),
                          _decLowerLimit,
                          targetLocation);
                }
                else
                {
                    LOGV2(DEBUG_STEPPERS, F("STEP-startSlewing(S): initial targetMoveTo is %l"), targetLocation);
                }

                _stepperDEC->moveTo(targetLocation);
                _mountStatus |= STATUS_SLEWING;
            }

            if (direction & EAST)
            {
                LOGV2(DEBUG_STEPPERS, F("STEP-startSlewing(E): initial targetMoveTo is %l"), -sign * 300000);
                _stepperRA->moveTo(-sign * 300000);
                _mountStatus |= STATUS_SLEWING;
            }
            if (direction & WEST)
            {
                LOGV2(DEBUG_STEPPERS, F("STEP-startSlewing(W): initial targetMoveTo is %l"), sign * 300000);
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

        LOGV1(DEBUG_STEPPERS, F("STEP-stopSlewing: TRK stepper stop()"));
        _stepperTRK->stop();
    }

    if ((direction & (NORTH | SOUTH)) != 0)
    {
        LOGV1(DEBUG_STEPPERS, F("STEP-stopSlewing: DEC stepper stop()"));
        _stepperDEC->stop();
    }
    if ((direction & (WEST | EAST)) != 0)
    {
        LOGV1(DEBUG_STEPPERS, F("STEP-stopSlewing: RA stepper stop()"));
        _stepperRA->stop();
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

// Since some of the boards cannot process timer interrupts at the required
// speed (or at all), we'll just stick to deterministic calls here.
#if RUN_STEPPERS_IN_MAIN_LOOP == 1
    interruptLoop();
#endif

    unsigned long now = millis();

#if (DEBUG_LEVEL & DEBUG_MOUNT) && (DEBUG_LEVEL & DEBUG_VERBOSE)
    if (now - _lastMountPrint > 2000)
    {
        LOGV2(DEBUG_MOUNT, "%s", getStatusString().c_str());
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
    // LOGV2(DEBUG_MOUNT, F("Mount: Focuser running:  %d"), _stepperFocus->isRunning());

    if (_stepperFocus->isRunning())
    {
        LOGV2(DEBUG_MOUNT, F("Mount: Focuser running at speed %f"), _stepperFocus->speed());
        _focuserWasRunning = true;
    }
    else if (_focuserWasRunning)
    {
        LOGV1(DEBUG_MOUNT, F("Mount: Focuser is stopped, but was running "));
        // If focuser was running last time through the loop, but not this time, it has
        // either been stopped, or reached the target.
        _focuserMode       = FOCUS_IDLE;
        _focuserWasRunning = false;
    #if FOCUSER_ALWAYS_ON == 0
        disableFocusMotor();
    #endif
    }

#endif

#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART && DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART && USE_AUTOHOME == 1
    if (isFindingHome())
    {
        if (digitalRead(DEC_DIAG_PIN) == HIGH)
        {
            finishFindingHomeDEC();
            return;
        }
        if (digitalRead(RA_DIAG_PIN) == HIGH)
        {
            finishFindingHomeRA();
            return;
        }
        //return;
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
                LOGV3(DEBUG_MOUNT | DEBUG_STEPPERS,
                      F("Mount::Loop: Reached target. RA:%l, DEC:%l"),
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
                        LOGV1(DEBUG_MOUNT | DEBUG_STEPPERS, F("Mount::Loop:   Was Parking, stop tracking and set home."));
                        setHome(false);
                    }
                    else
                    {
                        LOGV1(DEBUG_MOUNT | DEBUG_STEPPERS, F("Mount::Loop:   Was Parking, stop tracking."));
                    }
                }

                _currentDECStepperPosition = _stepperDEC->currentPosition();
                _currentRAStepperPosition  = _stepperRA->currentPosition();
#if RA_STEPPER_TYPE == STEPPER_TYPE_NEMA17
    #if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
                LOGV2(DEBUG_STEPPERS, F("STEP-loop: Arrived. RA driver setMicrosteps(%d)"), RA_TRACKING_MICROSTEPPING);
                _driverRA->microsteps(RA_TRACKING_MICROSTEPPING == 1 ? 0 : RA_TRACKING_MICROSTEPPING);
    #endif
                if (!isParking())
                {
                    if (_compensateForTrackerOff)
                    {
                        now                             = millis();
                        unsigned long elapsed           = now - _trackerStoppedAt;
                        unsigned long compensationSteps = _trackingSpeed * elapsed / 1000.0f;
                        LOGV4(DEBUG_STEPPERS,
                              F("STEP-loop: Arrived at %lms. Tracking was off for %lms (%l steps), compensating."),
                              now,
                              elapsed,
                              compensationSteps);
                        _stepperTRK->runToNewPosition(_stepperTRK->currentPosition() + compensationSteps);
                        _compensateForTrackerOff = false;
                    }
                    startSlewing(TRACKING);
                }
#endif

// Reset DEC to guide microstepping so that guiding is always ready and no switch is neccessary on guide pulses.
#if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
                LOGV2(DEBUG_STEPPERS, F("STEP-loop: Arrived. DEC driver setMicrosteps(%d)"), DEC_GUIDE_MICROSTEPPING);
                _driverDEC->microsteps(DEC_GUIDE_MICROSTEPPING == 1 ? 0 : DEC_GUIDE_MICROSTEPPING);
#endif

                if (_correctForBacklash)
                {
                    LOGV3(DEBUG_MOUNT | DEBUG_STEPPERS,
                          F("Mount::Loop:   Reached target at %d. Compensating by %d"),
                          (int) _currentRAStepperPosition,
                          _backlashCorrectionSteps);
                    _currentRAStepperPosition += _backlashCorrectionSteps;
                    _stepperRA->runToNewPosition(_currentRAStepperPosition);
                    _correctForBacklash = false;
                    LOGV2(DEBUG_MOUNT | DEBUG_STEPPERS, F("Mount::Loop:   Backlash correction done. Pos: %d"), _currentRAStepperPosition);
                }
                else
                {
                    LOGV2(DEBUG_MOUNT | DEBUG_STEPPERS,
                          F("Mount::Loop:   Reached target at %d, no backlash compensation needed"),
                          _currentRAStepperPosition);
                }

                if (_slewingToHome)
                {
                    LOGV1(DEBUG_MOUNT | DEBUG_STEPPERS, F("Mount::Loop:   Was Slewing home, so setting stepper RA and TRK to zero."));
                    _stepperRA->setCurrentPosition(0);
                    LOGV1(DEBUG_STEPPERS, F("STEP-loop:  TRK.setCurrentPos(0)"));
                    _stepperTRK->setCurrentPosition(0);
                    _stepperGUIDE->setCurrentPosition(0);
                    _homeOffsetRA  = 0;
                    _homeOffsetDEC = 0;

                    _targetRA = currentRA();
                    if (isParking())
                    {
                        LOGV1(DEBUG_MOUNT | DEBUG_STEPPERS,
                              F("Mount::Loop:   Was parking, so no tracking. Proceeding to park position..."));
                        _mountStatus &= ~STATUS_PARKING;
                        _slewingToPark = true;
                        _stepperRA->moveTo(_raParkingPos);
                        _stepperDEC->moveTo(_decParkingPos);
                        _totalDECMove = 1.0f * _stepperDEC->distanceToGo();
                        _totalRAMove  = 1.0f * _stepperRA->distanceToGo();
                        LOGV5(DEBUG_MOUNT | DEBUG_STEPPERS,
                              F("Mount::Loop:   Park Position is R:%l  D:%l, TotalMove is R:%f, D:%f"),
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
                        LOGV1(DEBUG_MOUNT | DEBUG_STEPPERS, F("Mount::Loop:   Restart tracking."));
                        startSlewing(TRACKING);
                    }
                    _slewingToHome = false;
                }
                else if (_slewingToPark)
                {
                    LOGV1(DEBUG_MOUNT | DEBUG_STEPPERS, F("Mount::Loop:   Arrived at park position..."));
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
}

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

    LOGV3(DEBUG_MOUNT, F("Mount::setParkingPos: parking RA: %l  DEC:%l"), _raParkingPos, _decParkingPos);

    EEPROMStore::storeRAParkingPos(_raParkingPos);
    EEPROMStore::storeDECParkingPos(_decParkingPos);
}

/////////////////////////////////
//
// setDecLimitPosition
//
/////////////////////////////////
void Mount::setDecLimitPosition(bool upper)
{
    if (upper)
    {
        _decUpperLimit = _stepperDEC->currentPosition();
        EEPROMStore::storeDECUpperLimit(_decUpperLimit);
        LOGV3(DEBUG_MOUNT, F("Mount::setDecLimitPosition(Upper): limit DEC: %l -> %l"), _decLowerLimit, _decUpperLimit);
    }
    else
    {
        _decLowerLimit = _stepperDEC->currentPosition();
        EEPROMStore::storeDECLowerLimit(_decLowerLimit);
        LOGV3(DEBUG_MOUNT, F("Mount::setDecLimitPosition(Lower): limit DEC: %l -> %l"), _decLowerLimit, _decUpperLimit);
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
        _decUpperLimit = 0;
        EEPROMStore::storeDECUpperLimit(_decUpperLimit);
        LOGV3(DEBUG_MOUNT, F("Mount::clearDecLimitPosition(Upper): limit DEC: %l -> %l"), _decLowerLimit, _decUpperLimit);
    }
    else
    {
        _decLowerLimit = 0;
        EEPROMStore::storeDECLowerLimit(_decLowerLimit);
        LOGV3(DEBUG_MOUNT, F("Mount::clearDecLimitPosition(Lower): limit DEC: %l -> %l"), _decLowerLimit, _decUpperLimit);
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
    LOGV1(DEBUG_MOUNT, F("Mount::setHome() called"));
    //LOGV2(DEBUG_MOUNT_VERBOSE,F("Mount::setHomePre: currentRA is %s"), currentRA().ToString());
    //LOGV2(DEBUG_MOUNT_VERBOSE,F("Mount::setHomePre: targetRA is %s"), targetRA().ToString());
    //LOGV2(DEBUG_MOUNT_VERBOSE,F("Mount::setHomePre: zeroPos is %s"), _zeroPosRA.ToString());
    _zeroPosRA = clearZeroPos ? DayTime(POLARIS_RA_HOUR, POLARIS_RA_MINUTE, POLARIS_RA_SECOND) : currentRA();

    _stepperRA->setCurrentPosition(0);
    _stepperDEC->setCurrentPosition(0);
    _stepperTRK->setCurrentPosition(0);
    // TODO: Set New Guide Stepper to 0

    _targetRA = currentRA();

    //LOGV2(DEBUG_MOUNT_VERBOSE,F("Mount::setHomePost: currentRA is %s"), currentRA().ToString());
    //LOGV2(DEBUG_MOUNT_VERBOSE,F("Mount::setHomePost: zeroPos is %s"), _zeroPosRA.ToString());
    //LOGV2(DEBUG_MOUNT_VERBOSE,F("Mount::setHomePost: targetRA is %s"), targetRA().ToString());
}

/////////////////////////////////
//
// setTargetToHome
//
// Set RA and DEC to the home position
/////////////////////////////////
void Mount::setTargetToHome()
{
    float trackedSeconds = _stepperTRK->currentPosition() / _trackingSpeed;  // steps / steps/s = seconds

    LOGV2(DEBUG_MOUNT, F("Mount::setTargetToHome() called with %fs elapsed tracking"), trackedSeconds);

    // In order for RA coordinates to work correctly, we need to
    // offset HATime by elapsed time since last HA set and also
    // adjust RA by the elapsed time and set it to zero.
    //LOGV2(DEBUG_MOUNT_VERBOSE,F("Mount::setTargetToHomePre:  currentRA is %s"), currentRA().ToString());
    //LOGV2(DEBUG_MOUNT_VERBOSE,F("Mount::setTargetToHomePre:  ZeroPosRA is %s"), _zeroPosRA.ToString());
    //LOGV3(DEBUG_MOUNT_VERBOSE,F("Mount::setTargetToHomePre:  TrackedSeconds is %f, TRK Stepper: %l"), trackedSeconds, _stepperTRK->currentPosition());
    //LOGV2(DEBUG_MOUNT_VERBOSE,F("Mount::setTargetToHomePre:  LST is %s"), _LST.ToString());
    DayTime lst(_LST);
    lst.addSeconds(trackedSeconds);
    setLST(lst);
    _targetRA = _zeroPosRA;
    _targetRA.addSeconds(trackedSeconds);

    //LOGV2(DEBUG_MOUNT_VERBOSE,F("Mount::setTargetToHomePost:  currentRA is %s"), currentRA().ToString());
    //LOGV2(DEBUG_MOUNT_VERBOSE,F("Mount::setTargetToHomePost: ZeroPosRA is %s"), _zeroPosRA.ToString());
    //LOGV2(DEBUG_MOUNT_VERBOSE,F("Mount::setTargetToHomePost: LST is %s"), _LST.ToString());
    //LOGV2(DEBUG_MOUNT_VERBOSE,F("Mount::setTargetToHomePost: TargetRA is %s"), _targetRA.ToString());

    // Set DEC to pole
    _targetDEC.set(0, 0, 0);
    _slewingToHome = true;
    // Stop the tracking stepper
    LOGV1(DEBUG_MOUNT, F("Mount::setTargetToHome() stop tracking"));
    stopSlewing(TRACKING);
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
    //LOGV3(DEBUG_MOUNT_VERBOSE,F("Mount::CalcSteppersPre: Current: RA: %s, DEC: %s"), currentRA().ToString(), currentDEC().ToString());
    //LOGV3(DEBUG_MOUNT_VERBOSE,F("Mount::CalcSteppersPre: Target : RA: %s, DEC: %s"), _targetRA.ToString(), _targetDEC.ToString());
    //LOGV2(DEBUG_MOUNT_VERBOSE,F("Mount::CalcSteppersPre: ZeroRA : %s"), _zeroPosRA.ToString());
    //LOGV4(DEBUG_MOUNT_VERBOSE,F("Mount::CalcSteppersPre: Stepper: RA: %l, DEC: %l, TRK: %l"), _stepperRA->currentPosition(), _stepperDEC->currentPosition(), _stepperTRK->currentPosition());
    DayTime raTarget = _targetRA;

    raTarget.subtractTime(_zeroPosRA);
    //LOGV3(DEBUG_MOUNT_VERBOSE,F("Mount::CalcSteppersIn: Adjust RA by Zeropos. New Target RA: %s, DEC: %s"), raTarget.ToString(), _targetDEC.ToString());

    float hourPos = raTarget.getTotalHours();
    if (!NORTHERN_HEMISPHERE)
    {
        hourPos += 12;
    }
    // Map [0 to 24] range to [-12 to +12] range
    while (hourPos > 12)
    {
        hourPos = hourPos - 24;
        //LOGV3(DEBUG_MOUNT_VERBOSE,F("Mount::CalcSteppersIn: RA>12 so -24. New Target RA: %s, DEC: %s"), DayTime(hourPos).ToString(), _targetDEC.ToString());
    }

    // How many u-steps moves the RA ring one sidereal hour along when slewing. One sidereal hour moves just shy of 15 degrees
    float stepsPerSiderealHour = _stepsPerRADegree * siderealDegreesInHour;  // u-steps/deg * deg/hr = u-steps/hr

    // Where do we want to move RA to?
    float moveRA = hourPos * stepsPerSiderealHour;  // hr * u-steps/hr = u-steps

    // Where do we want to move DEC to?
    // the variable targetDEC 0deg for the celestial pole (90deg), and goes negative only.
    float moveDEC = -_targetDEC.getTotalDegrees() * _stepsPerDECDegree;  // deg * u-steps/deg = u-steps

//LOGV3(DEBUG_MOUNT_VERBOSE,F("Mount::CalcSteppersIn: RA Steps/deg: %d   Steps/srhour: %f"), _stepsPerRADegree, stepsPerSiderealHour);
//LOGV3(DEBUG_MOUNT_VERBOSE,F("Mount::CalcSteppersIn: Target Step pos RA: %f, DEC: %f"), moveRA, moveDEC);

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
    float const RALimitL = (RA_LIMIT_LEFT * stepsPerSiderealHour);
    float const RALimitR = (RA_LIMIT_RIGHT * stepsPerSiderealHour);
#else
    float const RALimitL = (RA_LIMIT_RIGHT * stepsPerSiderealHour);
    float const RALimitR = (RA_LIMIT_LEFT * stepsPerSiderealHour);
#endif

    if (pSolutions != nullptr)
    {
        pSolutions[0] = long(-moveRA);
        pSolutions[1] = long(moveDEC);
        pSolutions[2] = long(-(moveRA - long(12.0f * stepsPerSiderealHour)));
        pSolutions[3] = long(-moveDEC);
        pSolutions[4] = long(-(moveRA + long(12.0f * stepsPerSiderealHour)));
        pSolutions[5] = long(-moveDEC);
    }

    // If we reach the limit in the positive direction ...
    if (moveRA > RALimitR)
    {
        //LOGV2(DEBUG_MOUNT_VERBOSE,F("Mount::CalcSteppersIn: RA is past +limit: %f, DEC: %f"), RALimit);

        // ... turn both RA and DEC axis around

        moveRA -= long(12.0f * stepsPerSiderealHour);
        moveDEC = -moveDEC;
        //LOGV3(DEBUG_MOUNT_VERBOSE,F("Mount::CalcSteppersIn: Adjusted Target Step pos RA: %f, DEC: %f"), moveRA, moveDEC);
    }
    // If we reach the limit in the negative direction...
    else if (moveRA < -RALimitL)
    {
        //LOGV2(DEBUG_MOUNT_VERBOSE,F("Mount::CalcSteppersIn: RA is past -limit: %f, DEC: %f"), -RALimit);
        // ... turn both RA and DEC axis around

        moveRA += long(12.0f * stepsPerSiderealHour);
        moveDEC = -moveDEC;
        //LOGV1(DEBUG_MOUNT_VERBOSE,F("Mount::CalcSteppersPost: Adjusted Target. Moved RA, inverted DEC"));
    }

    LOGV3(DEBUG_MOUNT, F("Mount::CalcSteppersPost: Target Steps RA: %f, DEC: %f"), -moveRA, moveDEC);
    //    float targetRA = clamp(-moveRA, -RAStepperLimit, RAStepperLimit);
    //    float targetDEC = clamp(moveDEC, DECStepperUpLimit, DECStepperDownLimit);
    targetRASteps  = -moveRA;
    targetDECSteps = moveDEC;

    // Can we get there without physical issues? (not doing anything with this yet)
    //  isUnreachable = ((targetRA != -moveRA) || (targetDEC != moveDEC));

    //  if (stepperRA.currentPosition() != int(targetRA)) {
    //    Serial.println("Moving RA from " + String(stepperRA.currentPosition()) + " to " + targetRA);
    //  }
    //  if (stepperDEC.currentPosition() != (targetDEC)) {
    //    Serial.println("Moving DEC from " + String(stepperDEC.currentPosition()) + " to " + targetDEC);
    //  }
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
    LOGV3(DEBUG_MOUNT, F("Mount::MoveSteppersTo: RA  From: %l  To: %f"), _stepperRA->currentPosition(), targetRASteps);
    LOGV3(DEBUG_MOUNT, F("Mount::MoveSteppersTo: DEC From: %l  To: %f"), _stepperDEC->currentPosition(), targetDECSteps);

    if ((_backlashCorrectionSteps != 0) && ((_stepperRA->currentPosition() - targetRASteps) > 0))
    {
        LOGV2(DEBUG_MOUNT, F("Mount::MoveSteppersTo: Needs backlash correction of %d!"), _backlashCorrectionSteps);
        targetRASteps -= _backlashCorrectionSteps;
        _correctForBacklash = true;
    }

    _stepperRA->moveTo(targetRASteps);

    if (_decUpperLimit != 0)
    {
        targetDECSteps = min(targetDECSteps, (float) _decUpperLimit);
        LOGV2(DEBUG_MOUNT, F("Mount::MoveSteppersTo: DEC Upper Limit enforced. To: %f"), targetDECSteps);
    }
    if (_decLowerLimit != 0)
    {
        targetDECSteps = max(targetDECSteps, (float) _decLowerLimit);
        LOGV2(DEBUG_MOUNT, F("Mount::MoveSteppersTo: DEC Lower Limit enforced. To: %f"), targetDECSteps);
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
    switch (direction)
    {
        case RA_STEPS:
            moveSteppersTo(_stepperRA->targetPosition() + steps, _stepperDEC->targetPosition());
            _mountStatus |= STATUS_SLEWING | STATUS_SLEWING_TO_TARGET;
            _totalRAMove = 1.0f * _stepperRA->distanceToGo();
#if RA_STEPPER_TYPE == STEPPER_TYPE_NEMA17  // tracking while slewing causes problems (can only run one AccelStepper at a time)
            if ((_stepperRA->distanceToGo() != 0) || (_stepperDEC->distanceToGo() != 0))
            {
                // Only stop tracking if we're actually going to slew somewhere else, otherwise the
                // mount::loop() code won't detect the end of the slewing operation...
                LOGV1(DEBUG_STEPPERS, "STEP-moveStepperBy: Stop tracking (NEMA steppers)");
                stopSlewing(TRACKING);
                _trackerStoppedAt        = millis();
                _compensateForTrackerOff = true;

    // set Slew microsteps for TMC2209 UART once the TRK stepper has stopped
    #if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
                LOGV2(DEBUG_STEPPERS, F("STEP-moveStepperBy: Switching RA driver to microsteps(%d)"), RA_SLEW_MICROSTEPPING);
                _driverRA->microsteps(RA_SLEW_MICROSTEPPING == 1 ? 0 : RA_SLEW_MICROSTEPPING);
    #endif

                LOGV2(DEBUG_STEPPERS, F("STEP-moveStepperBy: TRK stopped at %lms"), _trackerStoppedAt);
            }
#endif
            break;
        case DEC_STEPS:
            moveSteppersTo(_stepperRA->targetPosition(), _stepperDEC->targetPosition() + steps);
            _mountStatus |= STATUS_SLEWING | STATUS_SLEWING_TO_TARGET;
            _totalDECMove = 1.0f * _stepperDEC->distanceToGo();

#if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
            // Since normal state for DEC is guide microstepping, switch to slew microstepping here.
            LOGV2(DEBUG_STEPPERS, F("STEP-moveStepperBy: Switching DEC driver to microsteps(%d)"), DEC_SLEW_MICROSTEPPING);
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
            _stepperAZ->moveTo(_stepperAZ->currentPosition() + steps);
#endif
            break;
        case ALTITUDE_STEPS:
#if ALT_STEPPER_TYPE != STEPPER_TYPE_NONE
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
        //LOGV1(DEBUG_MOUNT_VERBOSE,F("DECString: TARGET!"));
        dec = _targetDEC;
    }
    else
    {
        //LOGV1(DEBUG_MOUNT_VERBOSE,F("DECString: CURRENT!"));
        dec = currentDEC();
    }
    //LOGV5(DEBUG_INFO,F("DECString: Precheck  : %s   %s  %dm %ds"), dec.ToString(), dec.getDegreesDisplay().c_str(), dec.getMinutes(), dec.getSeconds());
    // dec.checkHours();
    // LOGV2(DEBUG_MOUNT_VERBOSE,F("DECString: Postcheck : %s"), dec.ToString());

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
// StartFindingHome
//
/////////////////////////////////
// Automatically home the mount. Only with TMC2209 in UART mode
#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART && DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART && USE_AUTOHOME == 1

void Mount::startFindingHomeDEC()
{
    _driverDEC->SGTHRS(10);
    _driverDEC->rms_current(700);

    setManualSlewMode(true);
    _mountStatus |= STATUS_FINDING_HOME;
    _stepperDEC->setMaxSpeed(3000);
    _stepperDEC->setSpeed(-3000);
}

void Mount::finishFindingHomeDEC()
{
    _stepperDEC->stop();
    setManualSlewMode(false);
    _stepperDEC->setMaxSpeed(1000);
    _stepperDEC->setSpeed(1000);
    //_stepperDEC->move(2350);
    _stepperDEC->move(100);
    while (_stepperDEC->run())
        ;

    setManualSlewMode(false);

    delay(100);

    startFindingHomeRA();
}

void Mount::startFindingHomeRA()
{
    _driverRA->SGTHRS(50);
    _driverRA->rms_current(1000);
    // TODO: Fix broken microstep management to re-instate fine pointing
    // _driverRA->microsteps(FULLSTEP);
    _driverRA->semin(0);  // turn off coolstep
    _driverRA->semin(5);
    //_driverRA->TCOOLTHRS(0xFF);  // turn autocurrent threshold down to prevent false reading

    setManualSlewMode(true);
    //_mountStatus |= STATUS_FINDING_HOME;

    _stepperRA->setMaxSpeed(500);
    _stepperRA->setAcceleration(500);
    _stepperRA->setSpeed(-500);
}

void Mount::finishFindingHomeRA()
{
    _stepperRA->stop();

    _stepperRA->setSpeed(1000);
    //_stepperRA->move(15850.0);
    setManualSlewMode(false);
    _stepperRA->move(1000);

    while (_stepperRA->run())
        ;

    //setManualSlewMode(false);

    startSlewing(TRACKING);
    setHome(true);
}
#endif

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
    int _speed    = 1000;  //microsteps per driver clock tick
    int _duration = 1000;  //Duration to run in ms
    testUART_vactual(_driverRA, _speed, _duration);
}
    #endif

    #if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
void Mount::testDEC_UART_TX()
{
    int _speed    = 1000;  //microsteps per driver clock tick
    int _duration = 1000;  //Duration to run in ms
    testUART_vactual(_driverDEC, _speed, _duration);
}
    #endif

    #if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART || DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
void Mount::testUART_vactual(TMC2209Stepper *driver, int _speed, int _duration)
{
    driver->VACTUAL(_speed);
    delay(_duration);
    driver->shaft(1);
    delay(_duration);
    driver->shaft(0);
    driver->VACTUAL(0);
    delay(_duration);
}
    #endif
#endif
