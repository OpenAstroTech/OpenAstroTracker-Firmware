#include "../Configuration.hpp"
#include "Utility.hpp"
#include "EPROMStore.hpp"
#include "LcdMenu.hpp"
#include "Mount.hpp"
#include "Sidereal.hpp"
#include "MappedDict.hpp"
#include "AccelStepper.h"

#include "IntervalInterrupt.h"

#if defined(ARDUINO_ARCH_AVR)
ISR(TIMER3_OVF_vect)
{
    IntervalInterrupt_AVR<Timer::TIMER_3>::handle_overflow();
}
ISR(TIMER3_COMPA_vect)
{
    IntervalInterrupt_AVR<Timer::TIMER_3>::handle_compare_match();
}
ISR(TIMER4_OVF_vect)
{
    IntervalInterrupt_AVR<Timer::TIMER_4>::handle_overflow();
}
ISR(TIMER4_COMPA_vect)
{
    IntervalInterrupt_AVR<Timer::TIMER_4>::handle_compare_match();
}
ISR(TIMER5_OVF_vect)
{
    IntervalInterrupt_AVR<Timer::TIMER_5>::handle_overflow();
}
ISR(TIMER5_COMPA_vect)
{
    IntervalInterrupt_AVR<Timer::TIMER_5>::handle_compare_match();
}
#endif

#include "StepperConfiguration.h"

PUSH_NO_WARNINGS
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
// : _currentRAStepperPosition(0.0f), _currentDECStepperPosition(0.0f), _raParkingPos(0.0f),
//   _decParkingPos(0.0f)
#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE) || (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    : _azAltWasRunning(false)
#endif
{
    _lcdMenu = lcdMenu;
    initializeVariables();
}

void Mount::initializeVariables()
{
    _mountStatus       = 0;
    _lastDisplayUpdate = 0;
    _stepperWasRunning = false;
    _latitude          = Latitude(45.0);
    _longitude         = Longitude(100.0);
    _zeroPosDEC        = 0.0f;

#if USE_HALL_SENSOR_RA_AUTOHOME == 1
    _homing.state = HomingState::HOMING_NOT_ACTIVE;
#endif
    _moveRate      = 4;
    _slewingToHome = false;
    _slewingToPark = false;
    _raParkingPos  = Angle::deg(0.0f);
    _decParkingPos = Angle::deg(0.0f);
    _decLowerLimit = Angle::deg(0.0f);
    _decUpperLimit = Angle::deg(0.0f);

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
    LOGV1(DEBUG_INFO, F("[MOUNT]: Reading configuration data from EEPROM"));
    readPersistentData();
    LOGV1(DEBUG_INFO, F("[MOUNT]: Done reading configuration data from EEPROM"));
}

/////////////////////////////////
//
// readPersistentData
//
/////////////////////////////////
void Mount::readPersistentData()
{
    // EEPROMStore will always return valid data, even if no data is present in the store

    // _stepsPerRADegree = EEPROMStore::getRAStepsPerDegree();
    // LOGV2(DEBUG_INFO, F("[MOUNT]: EEPROM: RA steps/deg is %f"), _stepsPerRADegree);

    // _stepsPerDECDegree = EEPROMStore::getDECStepsPerDegree();
    // LOGV2(DEBUG_INFO, F("[MOUNT]: EEPROM: DEC steps/deg is %f"), _stepsPerDECDegree);

    float speed = EEPROMStore::getSpeedFactor();
    LOGV2(DEBUG_INFO, F("[MOUNT]: EEPROM: Speed factor is %f"), speed);
    setSpeedCalibration(speed, false);

    _latitude = EEPROMStore::getLatitude();
    LOGV2(DEBUG_INFO, F("[MOUNT]: EEPROM: Latitude is %s"), _latitude.ToString());

    _longitude = EEPROMStore::getLongitude();
    LOGV2(DEBUG_INFO, F("[MOUNT]: EEPROM: Longitude is %s"), _longitude.ToString());

    _localUtcOffset = EEPROMStore::getUtcOffset();
    LOGV2(DEBUG_INFO, F("[MOUNT]: EEPROM: UTC offset is %d"), _localUtcOffset);

#if USE_GYRO_LEVEL == 1
    _pitchCalibrationAngle = EEPROMStore::getPitchCalibrationAngle();
    LOGV2(DEBUG_INFO, F("[MOUNT]: EEPROM: Pitch Offset is %f"), _pitchCalibrationAngle);

    _rollCalibrationAngle = EEPROMStore::getRollCalibrationAngle();
    LOGV2(DEBUG_INFO, F("[MOUNT]: EEPROM: Roll Offset is %f"), _rollCalibrationAngle);
#endif

    _raParkingPos  = Angle::deg(EEPROMStore::getRAParkingPos());
    _decParkingPos = Angle::deg(EEPROMStore::getDECParkingPos());
    LOGV3(DEBUG_INFO, F("[MOUNT]: EEPROM: Parking position read as R:%f, D:%f"), _raParkingPos.deg(), _decParkingPos.deg());

    _decLowerLimit = Angle::deg(EEPROMStore::getDECLowerLimit());
    if (_decLowerLimit.deg() == 0 && DEC_LIMIT_DOWN != 0)
    {
        _decLowerLimit = Angle::deg(-DEC_LIMIT_DOWN);
    }

    _decUpperLimit = Angle::deg(EEPROMStore::getDECUpperLimit());
    if (_decUpperLimit.deg() == 0 && DEC_LIMIT_UP != 0)
    {
        _decUpperLimit = Angle::deg(DEC_LIMIT_UP);
    }

    LOGV3(DEBUG_INFO, F("[MOUNT]: EEPROM: DEC limits read as %f -> %f"), _decLowerLimit.deg(), _decUpperLimit.deg());

#if USE_HALL_SENSOR_RA_AUTOHOME == 1
    _homing.offsetRA = EEPROMStore::getRAHomingOffset();
    LOGV2(DEBUG_INFO, F("[MOUNT]: EEPROM: RA Homing offset read as %l"), _homing.offsetRA);
#endif
}

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
    LOGV2(DEBUG_STEPPERS, F("[STEPPERS]: Testing UART Connection to %s driver..."), driverKind);
    for (int i = 0; i < UART_CONNECTION_TEST_RETRIES; i++)
    {
        if (driver->test_connection() == 0)
        {
            LOGV2(DEBUG_STEPPERS, F("[STEPPERS]: UART connection to %s driver successful."), driverKind);
            return true;
        }
        else
        {
            delay(500);
        }
    }
    LOGV2(DEBUG_STEPPERS, F("[STEPPERS]: UART connection to %s driver failed."), driverKind);
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
    LOGV2(DEBUG_STEPPERS, F("[MOUNT]: Requested RA motor rms_current: %d mA"), rmscurrent);
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
        LOGV2(DEBUG_STEPPERS, F("[MOUNT]: Actual RA motor rms_current: %d mA"), _driverRA->rms_current());
        LOGV2(DEBUG_STEPPERS, F("[MOUNT]: Actual RA CS value: %d"), _driverRA->cs_actual());
        LOGV2(DEBUG_STEPPERS, F("[MOUNT]: Actual RA vsense: %d"), _driverRA->vsense());
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
    LOGV2(DEBUG_STEPPERS, F("[MOUNT]: Requested RA motor rms_current: %d mA"), rmscurrent);
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
        LOGV2(DEBUG_STEPPERS, F("[MOUNT]: Actual RA motor rms_current: %d mA"), _driverRA->rms_current());
        LOGV2(DEBUG_STEPPERS, F("[MOUNT]: Actual RA CS value: %d"), _driverRA->cs_actual());
        LOGV2(DEBUG_STEPPERS, F("[MOUNT]: Actual RA vsense: %d"), _driverRA->vsense());
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
    LOGV2(DEBUG_STEPPERS, F("[MOUNT]: Requested DEC motor rms_current: %d mA"), rmscurrent);
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
        LOGV2(DEBUG_STEPPERS, F("[MOUNT]: Actual DEC motor rms_current: %d mA"), _driverDEC->rms_current());
        LOGV2(DEBUG_STEPPERS, F("[MOUNT]: Actual DEC CS value: %d"), _driverDEC->cs_actual());
        LOGV2(DEBUG_STEPPERS, F("[MOUNT]: Actual DEC vsense: %d"), _driverDEC->vsense());
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
    LOGV2(DEBUG_STEPPERS, F("[MOUNT]: Requested DEC motor rms_current: %d mA"), rmscurrent);
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
        LOGV2(DEBUG_STEPPERS, F("[MOUNT]: Actual DEC motor rms_current: %d mA"), _driverDEC->rms_current());
        LOGV2(DEBUG_STEPPERS, F("[MOUNT]: Actual DEC CS value: %d"), _driverDEC->cs_actual());
        LOGV2(DEBUG_STEPPERS, F("[MOUNT]: Actual DEC vsense: %d"), _driverDEC->vsense());
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
    LOGV2(DEBUG_STEPPERS, F("[MOUNT]: Requested AZ motor rms_current: %d mA"), rmscurrent);
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
        LOGV2(DEBUG_STEPPERS, F("[MOUNT]: Actual AZ motor rms_current: %d mA"), _driverAZ->rms_current());
        LOGV2(DEBUG_STEPPERS, F("[MOUNT]: Actual AZ CS value: %d"), _driverAZ->cs_actual());
        LOGV2(DEBUG_STEPPERS, F("[MOUNT]: Actual AZ vsense: %d"), _driverAZ->vsense());
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
    LOGV2(DEBUG_STEPPERS, F("[MOUNT]: Requested AZ motor rms_current: %d mA"), rmscurrent);
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
        LOGV2(DEBUG_STEPPERS, F("[MOUNT]: Actual AZ motor rms_current: %d mA"), _driverAZ->rms_current());
        LOGV2(DEBUG_STEPPERS, F("[MOUNT]: Actual AZ CS value: %d"), _driverAZ->cs_actual());
        LOGV2(DEBUG_STEPPERS, F("[MOUNT]: Actual AZ vsense: %d"), _driverAZ->vsense());
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
    LOGV2(DEBUG_STEPPERS, F("[MOUNT]: Requested ALT motor rms_current: %d mA"), rmscurrent);
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
        LOGV2(DEBUG_STEPPERS, F("[MOUNT]: Actual ALT motor rms_current: %d mA"), _driverALT->rms_current());
        LOGV2(DEBUG_STEPPERS, F("[MOUNT]: Actual ALT CS value: %d"), _driverALT->cs_actual());
        LOGV2(DEBUG_STEPPERS, F("[MOUNT]: Actual ALT vsense: %d"), _driverALT->vsense());
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
    LOGV2(DEBUG_STEPPERS, F("[MOUNT]: Requested ALT motor rms_current: %d mA"), rmscurrent);
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
        LOGV2(DEBUG_STEPPERS, F("[MOUNT]: Actual ALT motor rms_current: %d mA"), _driverALT->rms_current());
        LOGV2(DEBUG_STEPPERS, F("[MOUNT]: Actual ALT CS value: %d"), _driverALT->cs_actual());
        LOGV2(DEBUG_STEPPERS, F("[MOUNT]: Actual ALT vsense: %d"), _driverALT->vsense());
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
    LOGV2(DEBUG_STEPPERS | DEBUG_FOCUS, F("[FOCUS]: Requested Focus motor rms_current: %d mA"), rmscurrent);
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
        LOGV2(DEBUG_STEPPERS | DEBUG_FOCUS, F("[FOCUS]: Actual Focus motor rms_current: %d mA"), _driverFocus->rms_current());
        LOGV2(DEBUG_STEPPERS | DEBUG_FOCUS, F("[FOCUS]: Actual Focus CS value: %d"), _driverFocus->cs_actual());
        LOGV2(DEBUG_STEPPERS | DEBUG_FOCUS, F("[FOCUS]: Actual Focus vsense: %d"), _driverFocus->vsense());
    }
        #endif

        #if FOCUSER_ALWAYS_ON == 1
    LOGV1(DEBUG_FOCUS, F("[FOCUS]: Always on -> TMC2209U enabling driver pin."));
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
    LOGV2(DEBUG_STEPPERS | DEBUG_FOCUS, F("[FOCUS]: Requested Focus motor rms_current: %d mA"), rmscurrent);
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
        LOGV2(DEBUG_STEPPERS | DEBUG_FOCUS, F("[FOCUS]: Actual Focus motor rms_current: %d mA"), _driverFocus->rms_current());
        LOGV2(DEBUG_STEPPERS | DEBUG_FOCUS, F("[FOCUS]: Actual Focus CS value: %d"), _driverFocus->cs_actual());
        LOGV2(DEBUG_STEPPERS | DEBUG_FOCUS, F("[FOCUS]: Actual Focus vsense: %d"), _driverFocus->vsense());
    }
        #endif
        #if FOCUSER_ALWAYS_ON == 1
    LOGV1(DEBUG_FOCUS, F("[FOCUS]: Always on -> TMC2209U enabling driver pin."));
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
    LOGV3(DEBUG_MOUNT, F("[MOUNT]: Updating speed calibration from %f to %f"), _trackingSpeedCalibration, val);
    _trackingSpeedCalibration = val;

    if (saveToStorage)
        EEPROMStore::storeSpeedFactor(_trackingSpeedCalibration);

    // If we are currently tracking, update the speed. No need to update microstepping mode
    if (isSlewingTRK())
    {
        // TODO: _stepperTRK->setSpeedFactor(_trackingSpeedCalibration);
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
    // if (which == RA_STEPS)
    // {
    //     return _stepsPerRADegree;  // u-steps/degree
    // }
    // if (which == DEC_STEPS)
    // {
    //     return _stepsPerDECDegree;  // u-steps/degree
    // }

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
        //_stepsPerDECDegree = steps;
        // EEPROMStore::storeDECStepsPerDegree(_stepsPerDECDegree);
    }
    else if (which == RA_STEPS)
    {
        //_stepsPerRADegree = steps;
        // EEPROMStore::storeRAStepsPerDegree(_stepsPerRADegree);
        //setSpeedCalibration(_trackingSpeedCalibration, false);
    }
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

    return ret;
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
    LOGV3(DEBUG_MOUNT, F("[MOUNT]: setSlewRate, rate is %d -> %f"), _moveRate, speedFactor[_moveRate]);
    DEC::setSlewRate(speedFactor[_moveRate]);
    RA::setSlewRate(speedFactor[_moveRate]);
}

/////////////////////////////////
//
// setHA
//
/////////////////////////////////
void Mount::setHA(const DayTime &haTime)
{
    LOGV2(DEBUG_MOUNT, F("[MOUNT]: setHA:  HA is %s"), haTime.ToString());
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
    // LOGV1(DEBUG_MOUNT_VERBOSE, F("[MOUNT]: Get HA."));
    // LOGV2(DEBUG_MOUNT_VERBOSE, F("[MOUNT]: Polaris adjust: %s"), DayTime(POLARIS_RA_HOUR, POLARIS_RA_MINUTE, POLARIS_RA_SECOND).ToString());
    DayTime ha = _LST;
    // LOGV2(DEBUG_MOUNT_VERBOSE, F("[MOUNT]: LST: %s"), _LST.ToString());
    ha.subtractTime(DayTime(POLARIS_RA_HOUR, POLARIS_RA_MINUTE, POLARIS_RA_SECOND));
    // LOGV2(DEBUG_MOUNT, F("[MOUNT]: GetHA: LST-Polaris is HA %s"), ha.ToString());
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
#ifdef OAM
    _zeroPosRA.addHours(6);  // shift allcoordinates by 90° for EQ mount movement
#endif
    LOGV2(DEBUG_MOUNT, F("[MOUNT]: Set LST and ZeroPosRA to: %s"), _LST.ToString());
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
    float hourPos = -RA::position().deg();

    // LOGV2(DEBUG_MOUNT_VERBOSE, F("[MOUNT]: CurrentRA: POS        : %s"), String(hourPos).c_str());
    hourPos += _zeroPosRA.getTotalHours();
    // LOGV2(DEBUG_MOUNT_VERBOSE,F("[MOUNT]: CurrentRA: ZeroPos    : %s"), _zeroPosRA.ToString());
    // LOGV2(DEBUG_MOUNT_VERBOSE,F("[MOUNT]: CurrentRA: POS (+zp)  : %s"), DayTime(hourPos).ToString());

    float degreePos = DEC::position().deg();
    degreePos += _zeroPosDEC;
    if (NORTHERN_HEMISPHERE ? degreePos < 0 : degreePos > 0)
    {
        hourPos += 12;
        if (hourPos > 24)
            hourPos -= 24;
        // LOGV2(DEBUG_MOUNT_VERBOSE,F("[MOUNT]: CurrentRA: RA (+12h): %s"), DayTime(hourPos).ToString());
    }

    // Make sure we are normalized
    if (hourPos < 0)
        hourPos += 24;
    if (hourPos > 24)
        hourPos -= 24;

    // LOGV2(DEBUG_MOUNT_VERBOSE,F("[MOUNT]: CurrentRA: RA Pos  -> : %s"), DayTime(hourPos).ToString());
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
    float degreePos = DEC::position().deg();
    //LOGV2(DEBUG_MOUNT_VERBOSE,F("[MOUNT]: CurrentDEC: POS        : %s"), String(degreePos).c_str());

    degreePos += _zeroPosDEC;

    if (NORTHERN_HEMISPHERE ? degreePos > 0 : degreePos < 0)
    {
        degreePos = -degreePos;
        //LOGV1(DEBUG_MOUNT_VERBOSE,F("[MOUNT]: CurrentDEC: Greater Zero, flipping."));
    }

    //LOGV2(DEBUG_MOUNT_VERBOSE,F("[MOUNT]: CurrentDEC: POS      : %s"), Declination(degreePos).ToString());
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
    float solutions[6];
    _targetDEC = dec;
    _targetRA  = ra;

    // Adjust the home RA position by the delta sync position.
    float raAdjust = ra.getTotalHours() - currentRA().getTotalHours();
    while (raAdjust > 12)
    {
        raAdjust = raAdjust - 24;
    }
    while (raAdjust < -12)
    {
        raAdjust = raAdjust + 24;
    }
    _zeroPosRA.addHours(raAdjust);

    // Adjust the home DEC position by the delta between the sync'd target and current position.
    float decAdjust = dec.getTotalDegrees() - currentDEC().getTotalDegrees();
    _zeroPosDEC += decAdjust;

    Angle targetRAPosition, targetDECPosition;
    LOGV3(DEBUG_MOUNT, F("[MOUNT]: Sync Position to RA: %s and DEC: %s"), ra.ToString(), _targetDEC.ToString());
    calculateRAandDECSteppers(targetRAPosition, targetDECPosition, solutions);

    LOGV3(DEBUG_STEPPERS, F("[STEPPERS]: syncPosition: Solution 1: RA %l and DEC: %l"), solutions[0], solutions[1]);
    LOGV3(DEBUG_STEPPERS, F("[STEPPERS]: syncPosition: Solution 2: RA %l and DEC: %l"), solutions[2], solutions[3]);
    LOGV3(DEBUG_STEPPERS, F("[STEPPERS]: syncPosition: Solution 3: RA %l and DEC: %l"), solutions[4], solutions[5]);
    LOGV3(DEBUG_STEPPERS, F("[STEPPERS]: syncPosition: Chose solution RA: %l and DEC: %l"), targetRAPosition, targetDECPosition);
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
    LOGV1(DEBUG_STEPPERS, F("[STEPPERS]: startSlewingToTarget: Set RA and DEC to Rate 1.0"));
    DEC::setSlewRate(1.0f);
    RA::setSlewRate(1.0f);

    // Calculate new RA stepper target (and DEC). We are never in guding mode here.
    _currentDECStepperPosition = DEC::position();
    _currentRAStepperPosition  = RA::position();
    Angle targetRAPosition, targetDECPosition;
    calculateRAandDECSteppers(targetRAPosition, targetDECPosition);

    moveSteppersTo(targetRAPosition, targetDECPosition);  // u-steps (in slew mode)

    _mountStatus |= STATUS_SLEWING | STATUS_SLEWING_TO_TARGET;
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
    LOGV1(DEBUG_STEPPERS, F("[STEPPERS]: startSlewingToHome: Set RA and DEC to Rate 1.0"));
    DEC::setSlewRate(1.0f);
    RA::setSlewRate(1.0f);

    _currentDECStepperPosition = DEC::position();
    _currentRAStepperPosition  = RA::position();

    // Take any syncs that have happened into account
    Angle targetRAPosition;
    Angle targetDECPosition;

    _slewingToHome = true;
    // Take tracking into account
    Angle trackingOffset = RA::trackingPosition();
    targetRAPosition     = targetRAPosition - trackingOffset;
    LOGV3(DEBUG_STEPPERS,
          F("[STEPPERS]: startSlewingToHome: Adjusted with tracking distance: %f, result: %f"),
          trackingOffset.deg(),
          targetRAPosition);

    moveSteppersTo(targetRAPosition, targetDECPosition);  // u-steps (in slew mode)

    _mountStatus |= STATUS_SLEWING | STATUS_SLEWING_TO_TARGET;
}

/////////////////////////////////
//
// stopGuiding
//
/////////////////////////////////
void Mount::stopGuiding(bool ra, bool dec)
{
    if (isGuiding())
    {
        // Stop RA guide first, since it's just a speed change back to tracking speed
        if (ra && (_mountStatus & STATUS_GUIDE_PULSE_RA))
        {
            LOGV1(DEBUG_STEPPERS, F("[STEPPERS]: stopGuiding(RA): switch to Tracking"));
            RA::track(true);
            _mountStatus &= ~STATUS_GUIDE_PULSE_RA;
        }

        if (dec && (_mountStatus & STATUS_GUIDE_PULSE_DEC))
        {
            LOGV1(DEBUG_STEPPERS, F("[STEPPERS]: stopGuiding(DEC): Stop motor"));
            // Stop DEC guiding and wait for it to stop.
            DEC::stopSlewing();
            LOGV2(DEBUG_STEPPERS, F("[STEPPERS]: stopGuiding(DEC): Stopped at %l"), DEC::position());
            _mountStatus &= ~STATUS_GUIDE_PULSE_DEC;
        }

        //disable pulse state if no direction is active
        if ((_mountStatus & STATUS_GUIDE_PULSE_DIR) == 0)
        {
            _mountStatus &= ~STATUS_GUIDE_PULSE_MASK;
        }
    }
}

/////////////////////////////////
//
// guidePulse
//
/////////////////////////////////
void Mount::guidePulse(byte direction, int durationMs)
{
    LOGV3(DEBUG_STEPPERS, F("[STEPPERS]: guidePulse: > Guide Pulse %d for %dms"), direction, durationMs);

    switch (direction)
    {
        case NORTH:
            DEC::guide(1, durationMs);
            _mountStatus |= STATUS_GUIDE_PULSE | STATUS_GUIDE_PULSE_DEC;
            break;
        case SOUTH:
            DEC::guide(-1, durationMs);
            _mountStatus |= STATUS_GUIDE_PULSE | STATUS_GUIDE_PULSE_DEC;
            break;
        case EAST:
            RA::guide(1, durationMs);
            _mountStatus |= STATUS_GUIDE_PULSE | STATUS_GUIDE_PULSE_RA;
            break;
        case WEST:
            RA::guide(-1, durationMs);
            _mountStatus |= STATUS_GUIDE_PULSE | STATUS_GUIDE_PULSE_RA;
            break;
    }

    LOGV1(DEBUG_STEPPERS, F("[STEPPERS]: guidePulse: < Guide Pulse"));
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
        waitUntilStopped(ALL_DIRECTIONS);
        _mountStatus |= STATUS_SLEWING | STATUS_SLEWING_MANUAL;
    }
    else
    {
        _mountStatus &= ~STATUS_SLEWING_MANUAL;
        stopSlewing(ALL_DIRECTIONS);
        waitUntilStopped(ALL_DIRECTIONS);
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
#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    if (which == FOCUS_STEPS)
    {
        LOGV2(DEBUG_MOUNT | DEBUG_FOCUS, F("[FOCUS]: setSpeed() Focuser setSpeed %f"), speedDegsPerSec);
        if (speedDegsPerSec != 0)
        {
            LOGV2(DEBUG_STEPPERS | DEBUG_FOCUS,
                  F("[FOCUS]: Mount:setSpeed(): Enabling motor, setting speed to %f. Continuous"),
                  speedDegsPerSec);
            enableFocusMotor();
            _stepperFocus->setMaxSpeed(speedDegsPerSec);
            _stepperFocus->moveTo(sign(speedDegsPerSec) * 300000);
            _focuserMode = FOCUS_TO_TARGET;
        }
        else
        {
            LOGV1(DEBUG_STEPPERS | DEBUG_FOCUS, F("[FOCUS]: setSpeed(): Stopping motor."));
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
    return AZ::isRunning();
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
    return ALT::isRunning();
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
        AZ::move(stepsToMove);
    }
    #endif
    #if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    if (direction == ALTITUDE_STEPS)
    {
        enableAzAltMotors();
        int stepsToMove = arcMinutes * ALTITUDE_STEPS_PER_ARC_MINUTE;
        ALT::move(stepsToMove);
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
    AZ::stop();
    #endif
    #if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    ALT::stop();
    #endif

    #if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    while (AZ::isRunning())
    {
        loop();
    }
    #endif

    #if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    while (ALT::isRunning())
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
    LOGV5(DEBUG_FOCUS,
          F("[FOCUS]: focusSetSpeedByRate: rate is %d, factor %f, maxspeed %f -> %f"),
          _focusRate,
          speedFactor[_focusRate],
          _maxFocusSpeed,
          _maxFocusRateSpeed);
    _stepperFocus->setMaxSpeed(_maxFocusRateSpeed);

    if (_stepperFocus->isRunning())
    {
        LOGV1(DEBUG_FOCUS, F("[FOCUS]: focusSetSpeedByRate: stepper is already running so should adjust speed?"));
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
    LOGV3(DEBUG_FOCUS, F("[FOCUS]: focusContinuousMove: direction is %d, maxspeed %f"), direction, _maxFocusRateSpeed);
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
    LOGV3(DEBUG_FOCUS, F("[FOCUS]: focusMoveBy: move by %l steps to %l. Target Mode."), steps, targetPosition);
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
    LOGV1(DEBUG_FOCUS, F("[FOCUS]: disableFocusMotor: stopping motor and waiting."));
    _stepperFocus->stop();
    waitUntilStopped(FOCUSING);

    #if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
        #if FOCUSER_ALWAYS_ON == 0
            #if (FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)

    LOGV1(DEBUG_FOCUS, F("[FOCUS]: disableFocusMotor: TMC2209U disabling enable pin"));
    digitalWrite(FOCUS_EN_PIN, HIGH);  // Logic HIGH to disable driver
            #else
    LOGV1(DEBUG_FOCUS, F("[FOCUS]: disableFocusMotor: non-TMC2209U disabling enable pin"));
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
    LOGV1(DEBUG_FOCUS, F("[FOCUS]: enableFocusMotor: enabling driver pin."));
    digitalWrite(FOCUS_EN_PIN, LOW);  // Logic LOW to enable driver
}

/////////////////////////////////
//
// focusStop
//
/////////////////////////////////
void Mount::focusStop()
{
    LOGV1(DEBUG_FOCUS, F("[FOCUS]: focusStop: stopping motor."));
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

    disp += " RA:" + String(RA::position().deg());
    disp += " DEC:" + String(DEC::position().deg());

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
            disp[0] = RA::direction() < 0 ? 'R' : 'r';
        if (slew & SLEWING_DEC)
            disp[1] = DEC::direction() < 0 ? 'D' : 'd';
        if (slew & SLEWING_TRACKING)
            disp[2] = 'T';
    }
    else if (isSlewingTRK())
    {
        disp[2] = 'T';
    }
#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    if (AZ::isRunning())
        disp[3] = AZ::direction() < 0 ? 'Z' : 'z';
#endif
#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    if (ALT::isRunning())
        disp[4] = ALT::direction() < 0 ? 'A' : 'a';
#endif

#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    if (_stepperFocus->isRunning())
        disp[5] = _stepperFocus->direction() < 0 ? 'F' : 'f';
#endif

    status += disp;
    status += String(RA::position().deg()) + ",";
    status += String(DEC::position().deg()) + ",";
    status += String(RA::trackingPosition().deg()) + ",";

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
    byte slewState = RA::isRunning() ? SLEWING_RA : NOT_SLEWING;
    slewState |= DEC::isRunning() ? SLEWING_DEC : NOT_SLEWING;

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
        stopGuiding();

        if (direction & TRACKING)
        {
            // Start tracking
            RA::track(true);
            _mountStatus |= STATUS_TRACKING;
            if (_recentTrackingStartTime == 0UL)
            {
                _recentTrackingStartTime = millis();
            }
        }
        else
        {
            // Start slewing
            int sign = NORTHERN_HEMISPHERE ? 1 : -1;

            // Set move rate to last commanded slew rate
            setSlewRate(_moveRate);

            if (direction & NORTH)
            {
                Angle targetLocation = Angle::deg(180);
                if (_decUpperLimit.deg() != 0)
                {
                    targetLocation = _decUpperLimit;
                    LOGV3(DEBUG_STEPPERS,
                          F("[STEPPERS]: startSlewing(N): DEC has upper limit of %f. targetMoveTo is now %f"),
                          _decUpperLimit.deg(),
                          targetLocation.deg());
                }
                else
                {
                    LOGV2(DEBUG_STEPPERS, F("[STEPPERS]: startSlewing(N): initial targetMoveTo is %f"), targetLocation.deg());
                }

                DEC::slewTo(targetLocation);
                _mountStatus |= STATUS_SLEWING;
            }

            if (direction & SOUTH)
            {
                Angle targetLocation = Angle::deg(-180);
                if (_decLowerLimit.deg() != 0)
                {
                    targetLocation = _decLowerLimit;
                    LOGV3(DEBUG_STEPPERS,
                          F("[STEPPERS]: startSlewing(S): DEC has lower limit of %f. targetMoveTo is now %f"),
                          _decLowerLimit.deg(),
                          targetLocation.deg());
                }
                else
                {
                    LOGV2(DEBUG_STEPPERS, F("[STEPPERS]: startSlewing(S): initial targetMoveTo is %f"), targetLocation.deg());
                }

                DEC::slewTo(targetLocation);
                _mountStatus |= STATUS_SLEWING;
            }

            if (direction & EAST)
            {
                LOGV2(DEBUG_STEPPERS, F("[STEPPERS]: startSlewing(E): initial targetMoveTo is %f"), -sign * 180.0f);
                RA::slewTo(Angle::deg(-sign * 180.0f));
                _mountStatus |= STATUS_SLEWING;
            }
            if (direction & WEST)
            {
                LOGV2(DEBUG_STEPPERS, F("[STEPPERS]: startSlewing(W): initial targetMoveTo is %f"), sign * 180.0f);
                RA::slewTo(Angle::deg(sign * 180.0f));
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
        if (_recentTrackingStartTime > 0UL)
        {
            _totalTrackingTime += millis() - _recentTrackingStartTime;
            _recentTrackingStartTime = 0UL;
        }

        LOGV1(DEBUG_STEPPERS, F("[STEPPERS]: stopSlewing: RA(trk) stepper stop()"));
        RA::track(false);
    }

    if ((direction & (NORTH | SOUTH)) != 0)
    {
        LOGV1(DEBUG_STEPPERS, F("[STEPPERS]: stopSlewing: DEC stepper stop()"));
        DEC::stopSlewing();
    }

    if ((direction & (WEST | EAST)) != 0)
    {
        LOGV1(DEBUG_STEPPERS, F("[STEPPERS]: stopSlewing: RA stepper stop()"));
        RA::stopSlewing();
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
    while (((direction & (EAST | WEST)) && RA::isRunning()) || ((direction & (NORTH | SOUTH)) && DEC::isRunning())
           || ((direction & TRACKING) && (((_mountStatus & STATUS_TRACKING) == 0) && RA::isRunning()))
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
Angle Mount::getCurrentStepperPosition(int direction)
{
    if (direction & TRACKING)
    {
        return RA::trackingPosition();
    }
    if (direction & (NORTH | SOUTH))
    {
        return DEC::position();
    }
    if (direction & (EAST | WEST))
    {
        return RA::position();
    }
    return Angle::deg(0.0f);
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
        LOGV2(DEBUG_MOUNT, F("[MOUNT]: setHomingOffset(RA): Offset: %l"), offset);
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
                    LOGV2(DEBUG_STEPPERS,
                          F("[HOMING]: Initiating stop at requested time. Advance to state %s"),
                          getHomingState(HomingState::HOMING_WAIT_FOR_STOP).c_str());
                    _homing.state = HomingState::HOMING_WAIT_FOR_STOP;
                    RA::stop([]() {
                        _homing.state     = _homing.nextState;
                        _homing.nextState = HomingState::HOMING_NOT_ACTIVE;
                    });
                }
            }
            break;

        case HomingState::HOMING_WAIT_FOR_STOP:
            {
                // if (!RA::isRunning())
                // {
                //     LOGV2(DEBUG_STEPPERS,
                //           F("[HOMING]: Stepper has stopped as expected, advancing to next state %s"),
                //           getHomingState(_homing.nextState).c_str());
                //     _homing.state     = _homing.nextState;
                //     _homing.nextState = HomingState::HOMING_NOT_ACTIVE;
                // }
            }
            break;

        case HomingState::HOMING_MOVE_OFF:
            {
                LOGV3(DEBUG_STEPPERS,
                      F("[HOMING]: Currently over Sensor, so moving off of it by reverse 1h. (%l steps). Advance to %s"),
                      (long) (-_homing.initialDir * _stepsPerRADegree * siderealDegreesInHour),
                      getHomingState(HomingState::HOMING_MOVING_OFF).c_str());
                moveStepperBy(StepperAxis::RA_STEPS, -_homing.initialDir * _stepsPerRADegree * siderealDegreesInHour);
                _homing.state = HomingState::HOMING_MOVING_OFF;
            }
            break;

        case HomingState::HOMING_MOVING_OFF:
            {
                if (RA::isRunning())
                {
                    int homingPinState = digitalRead(RA_HOMING_SENSOR_PIN);
                    if (homingPinState == HIGH)
                    {
                        LOGV2(DEBUG_STEPPERS,
                              F("[HOMING]: Stepper has moved off sensor... stopping in 2s. Advance to %s"),
                              getHomingState(HomingState::HOMING_STOP_AT_TIME).c_str());
                        _homing.stopAt    = millis() + 2000;
                        _homing.state     = HomingState::HOMING_STOP_AT_TIME;
                        _homing.nextState = HomingState::HOMING_START_FIND_START;
                    }
                }
                else
                {
                    LOGV2(DEBUG_STEPPERS,
                          F("[HOMING]: Stepper was unable to move off sensor... homing failed! Advance to %s"),
                          getHomingState(HomingState::HOMING_FAILED).c_str());
                    _homing.state = HomingState::HOMING_FAILED;
                }
            }
            break;

        case HomingState::HOMING_START_FIND_START:
            {
                long distance = (long) (_homing.initialDir * _stepsPerRADegree * siderealDegreesInHour * _homing.searchDistance);
                LOGV4(DEBUG_STEPPERS,
                      F("[HOMING]: Finding start on forward pass by moving RA by %dh (%l steps). Advance to %s"),
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
                if (RA::isRunning())
                {
                    int homingPinState = digitalRead(RA_HOMING_SENSOR_PIN);
                    if (_homing.lastPinState != homingPinState)
                    {
                        LOGV2(DEBUG_STEPPERS,
                              F("[HOMING]: Found start of sensor, continuing until end is found. Advance to %s"),
                              getHomingState(HomingState::HOMING_FINDING_END).c_str());
                        // Found the start of the sensor, keep going until we find the end
                        _homing.position[HOMING_START_PIN_POSITION] = RA::position();
                        _homing.lastPinState                        = homingPinState;
                        _homing.state                               = HomingState::HOMING_FINDING_END;
                    }
                }
                else
                {
                    // Did not find start. Go reverse direction for twice the distance
                    long distance = (long) (-_homing.initialDir * _stepsPerRADegree * siderealDegreesInHour * _homing.searchDistance * 2);
                    LOGV4(DEBUG_STEPPERS,
                          F("[HOMING]: Hall not found on forward pass. Moving RA reverse by %dh (%l steps). Advance to %s"),
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
                if (RA::isRunning())
                {
                    int homingPinState = digitalRead(RA_HOMING_SENSOR_PIN);
                    if (_homing.lastPinState != homingPinState)
                    {
                        LOGV2(DEBUG_STEPPERS,
                              F("[HOMING]: Found start of sensor reverse, continuing until end is found. Advance to %s"),
                              getHomingState(HomingState::HOMING_FINDING_END).c_str());
                        _homing.position[HOMING_START_PIN_POSITION] = RA::position();
                        _homing.lastPinState                        = homingPinState;
                        _homing.state                               = HomingState::HOMING_FINDING_END;
                    }
                }
                else
                {
                    // Did not find start in either direction, abort.
                    LOGV2(DEBUG_STEPPERS,
                          F("[HOMING]: Sensor not found on reverse pass either. Homing Failed. Advance to %s"),
                          getHomingState(HomingState::HOMING_FAILED).c_str());
                    _homing.state = HomingState::HOMING_FAILED;
                }
            }
            break;

        case HomingState::HOMING_FINDING_END:
            {
                if (RA::isRunning())
                {
                    int homingPinState = digitalRead(RA_HOMING_SENSOR_PIN);
                    if (_homing.lastPinState != homingPinState)
                    {
                        LOGV2(DEBUG_STEPPERS,
                              F("[HOMING]: Found end of sensor, stopping... Advance to %s"),
                              getHomingState(HomingState::HOMING_WAIT_FOR_STOP).c_str());
                        _homing.position[HOMING_END_PIN_POSITION] = RA::position();
                        _homing.lastPinState                      = homingPinState;
                        RA::stop();
                        _homing.state     = HomingState::HOMING_WAIT_FOR_STOP;
                        _homing.nextState = HomingState::HOMING_RANGE_FOUND;
                    }
                }
                else
                {
                    LOGV2(DEBUG_STEPPERS,
                          F("[HOMING]: End of sensor not found! Advance to %s"),
                          getHomingState(HomingState::HOMING_FAILED).c_str());
                    _homing.state = HomingState::HOMING_FAILED;
                }
            }
            break;

        case HomingState::HOMING_RANGE_FOUND:
            {
                LOGV4(DEBUG_STEPPERS,
                      F("[HOMING]: Stepper stopped, Hall sensor found! Range: [%l to %l] size: %l"),
                      _homing.position[HOMING_START_PIN_POSITION],
                      _homing.position[HOMING_END_PIN_POSITION],
                      _homing.position[HOMING_START_PIN_POSITION] - _homing.position[HOMING_END_PIN_POSITION]);

                long midPos = (_homing.position[HOMING_START_PIN_POSITION] + _homing.position[HOMING_END_PIN_POSITION]) / 2;

                LOGV5(DEBUG_STEPPERS,
                      F("[HOMING]: Moving RA to home by %l - (%l) - (%l) steps. Advance to %s"),
                      midPos,
                      RA::position(),
                      _homing.offsetRA,
                      getHomingState(HomingState::HOMING_WAIT_FOR_STOP).c_str());

                moveStepperBy(StepperAxis::RA_STEPS, midPos - RA::position() - _homing.offsetRA);

                _homing.state     = HomingState::HOMING_WAIT_FOR_STOP;
                _homing.nextState = HomingState::HOMING_SUCCESSFUL;
            }
            break;

        case HomingState::HOMING_SUCCESSFUL:
            {
                LOGV2(DEBUG_STEPPERS,
                      F("[HOMING]: Successfully homed! Setting home and restoring Rate setting. Advance to %s"),
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
                LOGV2(DEBUG_STEPPERS,
                      F("[HOMING]: Failed to home! Restoring Rate setting and slewing to start position. Advance to %s"),
                      getHomingState(HomingState::HOMING_NOT_ACTIVE).c_str());
                setSlewRate(_homing.savedRate);
                _homing.state = HomingState::HOMING_NOT_ACTIVE;
                _mountStatus &= ~STATUS_FINDING_HOME;
                _mountStatus |= STATUS_SLEWING | STATUS_SLEWING_TO_TARGET;
                RA::slewTo(_homing.startPos);
            }
            break;

        default:
            LOGV2(DEBUG_STEPPERS, F("[HOMING]: Unhandled state (%d)! "), _homing.state);
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
    _homing.startPos       = RA::position();
    _homing.savedRate      = _moveRate;
    _homing.initialDir     = initialDirection;
    _homing.searchDistance = searchDistance;

    setSlewRate(4);

    _mountStatus |= STATUS_FINDING_HOME;

    // Check where we are over the sensor already
    if (digitalRead(RA_HOMING_SENSOR_PIN) == LOW)
    {
        _homing.state = HomingState::HOMING_MOVE_OFF;
        LOGV1(DEBUG_STEPPERS, F("[HOMING]: Sensor is signalled, move off sensor started"));
    }
    else
    {
        _homing.state = HomingState::HOMING_START_FIND_START;
        LOGV1(DEBUG_STEPPERS, F("[HOMING]: Sensor is not signalled, find start of range"));
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
#if USE_HALL_SENSOR_RA_AUTOHOME == 1
    if (_mountStatus & STATUS_FINDING_HOME)
    {
        processRAHomingProgress();
    }
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

#if (DEBUG_LEVEL & DEBUG_MOUNT) && (DEBUG_LEVEL & DEBUG_VERBOSE)
    unsigned long now = millis();
    if (now - _lastMountPrint > 2000)
    {
        LOGV2(DEBUG_MOUNT, F("[MOUNT]: Status -> %s"), getStatusString().c_str());
        _lastMountPrint = now;
    }
#endif

#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE) || (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    bool oneIsRunning = false;
    #if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    oneIsRunning |= AZ::isRunning();
    #endif
    #if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    oneIsRunning |= ALT::isRunning();
    #endif

    if (!oneIsRunning && _azAltWasRunning)
    {
        // One of the motors was running last time through the loop, but not anymore, so shutdown the outputs.
        disableAzAltMotors();
        _azAltWasRunning = false;
    }

    oneIsRunning = false;
    #if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    oneIsRunning |= AZ::isRunning();
    #endif
    #if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    oneIsRunning |= ALT::isRunning();
    #endif

    if (oneIsRunning)
    {
        _azAltWasRunning = true;
    }
#endif

#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    // LOGV2(DEBUG_MOUNT, F("[MOUNT]: Focuser running:  %d"), _stepperFocus->isRunning());

    if (_stepperFocus->isRunning())
    {
        LOGV2(DEBUG_FOCUS, F("[MOUNT]: Loop: Focuser running at speed %f"), _stepperFocus->speed());
        _focuserWasRunning = true;
    }
    else if (_focuserWasRunning)
    {
        LOGV1(DEBUG_FOCUS, F("[MOUNT]: Loop: Focuser is stopped, but was running "));
        // If focuser was running last time through the loop, but not this time, it has
        // either been stopped, or reached the target.
        _focuserMode       = FOCUS_IDLE;
        _focuserWasRunning = false;
        LOGV1(DEBUG_FOCUS, F("[MOUNT]: Loop: Focuser is stopped, but was running, disabling"));
        disableFocusMotor();
    }

#endif

    if (isGuiding())
    {
        return;
    }

    if (DEC::isRunning())
    {
        decStillRunning = true;
    }

    if (RA::isRunning())
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
                LOGV3(
                    DEBUG_MOUNT | DEBUG_STEPPERS, F("[MOUNT]: Loop: Reached target. RA:%l, DEC:%l"), RA::position().deg(), DEC::position());
                // Mount is at Target!
                // If we we're parking, we just reached home. Clear the flag, reset the motors and stop tracking.
                if (isParking())
                {
                    stopSlewing(TRACKING);
                    // If we're on the second part of the slew to parking, don't set home here
                    if (!_slewingToPark)
                    {
                        LOGV1(DEBUG_MOUNT | DEBUG_STEPPERS, F("[MOUNT]: Loop:   Was Parking, stop tracking and set home."));
                        setHome(false);
                    }
                    else
                    {
                        LOGV1(DEBUG_MOUNT | DEBUG_STEPPERS, F("[MOUNT]: Loop:   Was Parking, stop tracking."));
                    }
                }

                _currentDECStepperPosition = DEC::position();
                _currentRAStepperPosition  = RA::position();

                if (!isParking() && !isFindingHome())  // If we're homing, RA must stay in Slew configuration
                {
                    startSlewing(TRACKING);
                }

                LOGV2(DEBUG_MOUNT | DEBUG_STEPPERS, F("[MOUNT]: Loop:   Reached target at %d"), _currentRAStepperPosition);

                if (_slewingToHome)
                {
                    LOGV1(DEBUG_MOUNT | DEBUG_STEPPERS, F("[MOUNT]: Loop:   Was Slewing home, so setting stepper RA and TRK to zero."));
                    RA::setPosition(Angle::deg(0.0F));
                    DEC::setPosition(Angle::deg(0.0F));

                    _targetRA = currentRA();
                    if (isParking())
                    {
                        LOGV1(DEBUG_MOUNT | DEBUG_STEPPERS,
                              F("[MOUNT]: Loop:   Was parking, so no tracking. Proceeding to park position..."));
                        _mountStatus &= ~STATUS_PARKING;
                        _slewingToPark = true;
                        RA::slewTo(_raParkingPos);
                        DEC::slewTo(_decParkingPos);
                        LOGV3(DEBUG_MOUNT | DEBUG_STEPPERS,
                              F("[MOUNT]: Loop:   Park Position is R:%f  D:%f"),
                              _raParkingPos.deg(),
                              _decParkingPos.deg());
                        if ((DEC::slewingProgress() != 1.0f) || (RA::slewingProgress() != 1.0f))
                        {
                            _mountStatus |= STATUS_PARKING_POS | STATUS_SLEWING;
                        }
                    }
                    else
                    {
                        LOGV1(DEBUG_MOUNT | DEBUG_STEPPERS, F("[MOUNT]: Loop:   Restart tracking."));
                        startSlewing(TRACKING);
                    }
                    _slewingToHome = false;
                }
                else if (_slewingToPark)
                {
                    LOGV1(DEBUG_MOUNT | DEBUG_STEPPERS, F("[MOUNT]: Loop:   Arrived at park position..."));
                    _mountStatus &= ~(STATUS_PARKING_POS | STATUS_SLEWING_TO_TARGET);
                    _slewingToPark = false;
                }

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
    // TODO: Take tracking into account?
    _raParkingPos = RA::position();

    // TODO: Take guide pulses on DEC into account
    _decParkingPos = DEC::position();

    LOGV3(DEBUG_MOUNT, F("[MOUNT]: setParkingPos: parking RA: %f  DEC:%f"), _raParkingPos.deg(), _decParkingPos.deg());

    EEPROMStore::storeRAParkingPos(_raParkingPos.deg());
    EEPROMStore::storeDECParkingPos(_decParkingPos.deg());
}

/////////////////////////////////
//
// getDecParkingOffset
//
/////////////////////////////////
float Mount::getDecParkingOffset()
{
    return EEPROMStore::getDECParkingPos();
}

/////////////////////////////////
//
// setDecParkingOffset
//
/////////////////////////////////
void Mount::setDecParkingOffset(float offset)
{
    EEPROMStore::storeDECParkingPos(offset);
}

/////////////////////////////////
//
// setDecLimitPosition
//
/////////////////////////////////
void Mount::setDecLimitPosition(bool upper)
{
    setDecLimitPositionAbs(upper, DEC::position().deg());
}

/////////////////////////////////
//
// setDecLimitPositionAbs
//
/////////////////////////////////
void Mount::setDecLimitPositionAbs(bool upper, float pos)
{
    if (upper)
    {
        // TODO: use angle
        _decUpperLimit = Angle::deg(DEC_LIMIT_UP);
        EEPROMStore::storeDECUpperLimit(_decUpperLimit.deg());
        LOGV3(DEBUG_MOUNT, F("[MOUNT]: setDecLimitPosition(Upper): limit DEC: %f -> %f"), _decLowerLimit.deg(), _decUpperLimit.deg());
    }
    else
    {
        _decLowerLimit = Angle::deg(-(DEC_LIMIT_DOWN));
        EEPROMStore::storeDECLowerLimit(_decLowerLimit.deg());
        LOGV3(DEBUG_MOUNT, F("[MOUNT]: setDecLimitPosition(Lower): limit DEC: %f -> %f"), _decLowerLimit.deg(), _decUpperLimit.deg());
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
        _decUpperLimit = Angle::deg(DEC_LIMIT_UP);
        EEPROMStore::storeDECUpperLimit(_decUpperLimit.deg());
        LOGV3(DEBUG_MOUNT, F("[MOUNT]: clearDecLimitPosition(Upper): limit DEC: %f -> %f"), _decLowerLimit.deg(), _decUpperLimit.deg());
    }
    else
    {
        _decLowerLimit = Angle::deg(-(DEC_LIMIT_DOWN));
        EEPROMStore::storeDECLowerLimit(_decLowerLimit.deg());
        LOGV3(DEBUG_MOUNT, F("[MOUNT]: clearDecLimitPosition(Lower): limit DEC: %f -> %f"), _decLowerLimit.deg(), _decUpperLimit.deg());
    }
}

/////////////////////////////////
//
// getDecLimitPositions
//
/////////////////////////////////
void Mount::getDecLimitPositions(Angle &lowerLimit, Angle &upperLimit)
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
    LOGV1(DEBUG_MOUNT, F("[MOUNT]: setHome() called"));
    //LOGV2(DEBUG_MOUNT_VERBOSE,F("[MOUNT]: setHomePre: currentRA is %s"), currentRA().ToString());
    //LOGV2(DEBUG_MOUNT_VERBOSE,F("[MOUNT]: setHomePre: targetRA is %s"), targetRA().ToString());
    //LOGV2(DEBUG_MOUNT_VERBOSE,F("[MOUNT]: setHomePre: zeroPos is %s"), _zeroPosRA.ToString());
    _zeroPosRA = clearZeroPos ? DayTime(POLARIS_RA_HOUR, POLARIS_RA_MINUTE, POLARIS_RA_SECOND) : calculateLst();
#ifdef OAM
    _zeroPosRA.addHours(6);  // shift allcoordinates by 90° for EQ mount movement
#endif
    _zeroPosDEC = 0.0f;

    RA::setPosition(Angle::deg(0.0f));
    DEC::setPosition(Angle::deg(0.0f));

    _targetRA = currentRA();

    //LOGV2(DEBUG_MOUNT_VERBOSE,F("[MOUNT]: setHomePost: currentRA is %s"), currentRA().ToString());
    //LOGV2(DEBUG_MOUNT_VERBOSE,F("[MOUNT]: setHomePost: zeroPos is %s"), _zeroPosRA.ToString());
    //LOGV2(DEBUG_MOUNT_VERBOSE,F("[MOUNT]: setHomePost: targetRA is %s"), targetRA().ToString());
}

/////////////////////////////////
//
// calculateStepperPositions
//
// This code calculates the stepper locations to move to, given the right ascension and declination
/////////////////////////////////
void Mount::calculateStepperPositions(float raCoord, float decCoord, Angle &raPos, Angle &decPos)
{
    DayTime savedRA      = _targetRA;
    Declination savedDec = _targetDEC;
    _targetRA            = DayTime(raCoord);
    _targetDEC           = Declination::FromSeconds(decCoord * 3600.0f);
    calculateRAandDECSteppers(raPos, decPos);
    _targetRA  = savedRA;
    _targetDEC = savedDec;
}

float Mount::getTrackedTime() const
{
    return _totalTrackingTime / 3600000.0f;
}

/////////////////////////////////
//
// calculateRAandDECSteppers
//
// This code tells the steppers to what location to move to, given the select right ascension and declination
/////////////////////////////////
void Mount::calculateRAandDECSteppers(Angle &targetRA, Angle &targetDEC, float pSolutions[6]) const
{
    // LOGV3(DEBUG_MOUNT_VERBOSE, F("[MOUNT]: CalcSteppersPre: Current: RA: %s, DEC: %s"), currentRA().ToString(), currentDEC().ToString());
    // LOGV3(DEBUG_MOUNT_VERBOSE, F("[MOUNT]: CalcSteppersPre: Target : RA: %s, DEC: %s"), _targetRA.ToString(), _targetDEC.ToString());
    // LOGV2(DEBUG_MOUNT_VERBOSE, F("[MOUNT]: CalcSteppersPre: ZeroRA : %s"), _zeroPosRA.ToString());
    // LOGV4(DEBUG_MOUNT_VERBOSE,
    //       F("[MOUNT]: CalcSteppersPre: Stepper: RA: %l, DEC: %l, TRK: %l"),
    //       RA::currentPosition(),
    //       DEC::position(),
    //       _stepperTRK->currentPosition());
    DayTime raTarget      = _targetRA;
    Declination decTarget = _targetDEC;

    raTarget.subtractTime(_zeroPosRA);
    LOGV3(DEBUG_MOUNT_VERBOSE,
          F("[MOUNT]: CalcSteppersIn: Adjust RA by Zeropos. New Target RA: %s, DEC: %s"),
          raTarget.ToString(),
          _targetDEC.ToString());

    // Where do we want to move RA to?
    float moveRA = raTarget.getTotalHours();
    if (!NORTHERN_HEMISPHERE)
    {
        moveRA += 12;
    }

    // Total hours of tracking-to-date
    float trackedHours = getTrackedTime();
    LOGV2(DEBUG_MOUNT_VERBOSE, F("[MOUNT]: CalcSteppersIn: Tracked time is %f h."), trackedHours);

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
        LOGV3(DEBUG_MOUNT_VERBOSE,
              F("[MOUNT]: CalcSteppersIn: RA>12 so -24. New Target RA: %s, DEC: %s"),
              DayTime(moveRA).ToString(),
              _targetDEC.ToString());
    }

    // Where do we want to move DEC to?
    // the variable targetDEC 0deg for the celestial pole (90deg), and goes negative only.
    float moveDEC = (decTarget.getTotalDegrees() - _zeroPosDEC);  // deg

    LOGV3(DEBUG_MOUNT_VERBOSE, F("[MOUNT]: CalcSteppersIn: Target hrs pos RA: %f, DEC: %f"), moveRA, moveDEC);

    /*
  * Current RA wheel has a rotation limit of around 7 hours in each direction from home position.
  * Since tracking does not trigger the meridian flip, we try to extend the possible tracking time 
  * without reaching the RA ring end by executing the meridian flip before slewing to the target.
  * For this flip the RA and DEC rings have to be flipped by 180° (which is 12 RA hours). Based
  * on the physical RA ring limits, this means that the flip can only be executed during +/-[5h to 7h]
  * sections around the home position of RA. The tracking time will still be limited to around 2h in
  * worst case if the target is located right before the 5h mark during slewing. 
  */
    LOGV2(DEBUG_MOUNT_VERBOSE, F("[MOUNT]: CalcSteppersIn: TrackedHours: %f"), trackedHours);
#if NORTHERN_HEMISPHERE == 1
    float const RALimitL = -RA_LIMIT_LEFT;
    float const RALimitR = RA_LIMIT_RIGHT;
#else
    float const RALimitL = -RA_LIMIT_RIGHT;
    float const RALimitR = RA_LIMIT_LEFT;
#endif
    LOGV3(DEBUG_MOUNT_VERBOSE, F("[MOUNT]: CalcSteppersIn: Limits are : %f to %f"), RALimitL, RALimitR);

    if (pSolutions != nullptr)
    {
        pSolutions[0] = -moveRA;
        pSolutions[1] = moveDEC;
        pSolutions[2] = -(moveRA - 12.0f);
        pSolutions[3] = -moveDEC;
        pSolutions[4] = -(moveRA + 12.0f);
        pSolutions[5] = -moveDEC;
    }

    LOGV3(DEBUG_MOUNT_VERBOSE, F("[MOUNT]: CalcSteppersIn: Solution 1: %f, %f"), moveRA, moveDEC);
    LOGV3(DEBUG_MOUNT_VERBOSE, F("[MOUNT]: CalcSteppersIn: Solution 2: %f, %f"), (moveRA - 12.0f), -moveDEC);
    LOGV3(DEBUG_MOUNT_VERBOSE, F("[MOUNT]: CalcSteppersIn: Solution 3: %f, %f"), (moveRA + 12.0f), -moveDEC);

    // If we reach the limit in the positive direction ...
    if (homeTargetDeltaRA > RALimitR)
    {
        LOGV3(DEBUG_MOUNT_VERBOSE, F("[MOUNT]: CalcSteppersIn: RA %f is past max limit %f  (solution 2)"), moveRA, RALimitR);

        // ... turn both RA and DEC axis around
        moveRA -= 12.0f;
        moveDEC = -moveDEC;
        LOGV3(DEBUG_MOUNT_VERBOSE, F("[MOUNT]: CalcSteppersIn: Adjusted Target. RA: %f, DEC: %f"), moveRA, moveDEC);
    }
    // If we reach the limit in the negative direction...
    else if (homeTargetDeltaRA < RALimitL)
    {
        LOGV3(DEBUG_MOUNT_VERBOSE, F("[MOUNT]: CalcSteppersIn: RA %f is past min limit: %f, (solution 3)"), moveRA, RALimitL);
        // ... turn both RA and DEC axis around

        moveRA += 12.0f;
        moveDEC = -moveDEC;
        LOGV3(DEBUG_MOUNT_VERBOSE, F("[MOUNT]: CalcSteppersIn: Adjusted Target. RA: %f, DEC: %f"), moveRA, moveDEC);
    }
    else
    {
        LOGV3(DEBUG_MOUNT_VERBOSE, F("[MOUNT]: CalcSteppersIn: RA is in range: %f, DEC: %f  (solution 1)"), moveRA, moveDEC);
    }

    LOGV3(DEBUG_MOUNT, F("[MOUNT]: CalcSteppersPost: Target Steps RA: %f, DEC: %f"), -moveRA, moveDEC);
    targetRA  = Angle::deg(-moveRA);
    targetDEC = Angle::deg(moveDEC);
}

/////////////////////////////////
//
// moveSteppersTo
//
/////////////////////////////////
void Mount::moveSteppersTo(Angle targetRASteps, Angle targetDECSteps)
{  // Units are u-steps (in slew mode)
    // Show time: tell the steppers where to go!
    LOGV3(DEBUG_MOUNT, F("[MOUNT]: MoveSteppersTo: RA  From: %l  To: %f"), RA::position().deg(), targetRASteps.deg());
    LOGV3(DEBUG_MOUNT, F("[MOUNT]: MoveSteppersTo: DEC From: %l  To: %f"), DEC::position().deg(), targetDECSteps.deg());

    RA::slewTo(targetRASteps);

    if (_decUpperLimit.deg() != 0)
    {
        if (_decUpperLimit.deg() < targetDECSteps.deg())
        {
            targetDECSteps = _decUpperLimit;
        }
        LOGV2(DEBUG_MOUNT, F("[MOUNT]: MoveSteppersTo: DEC Upper Limit enforced. To: %f"), targetDECSteps.deg());
    }
    if (_decLowerLimit.deg() != 0)
    {
        if (_decLowerLimit.deg() > targetDECSteps.deg())
        {
            targetDECSteps = _decLowerLimit;
        }
        LOGV2(DEBUG_MOUNT, F("[MOUNT]: MoveSteppersTo: DEC Lower Limit enforced. To: %f"), targetDECSteps.deg());
    }

    DEC::slewTo(targetDECSteps);
}

/////////////////////////////////
//
// moveStepperBy
//
/////////////////////////////////
void Mount::moveStepperBy(StepperAxis direction, Angle steps)
{
    LOGV2(DEBUG_STEPPERS, F("[STEPPERS]: moveStepperBy: %f"), steps.deg());
    switch (direction)
    {
        case RA_STEPS:
            RA::slewBy(steps);
            _mountStatus |= STATUS_SLEWING | STATUS_SLEWING_TO_TARGET;
            break;
        case DEC_STEPS:
            DEC::slewBy(steps);
            _mountStatus |= STATUS_SLEWING | STATUS_SLEWING_TO_TARGET;
            break;
        case FOCUS_STEPS:
#if FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE
            focusMoveBy(steps);
#endif
            break;
        case AZIMUTH_STEPS:
#if AZ_STEPPER_TYPE != STEPPER_TYPE_NONE
            enableAzAltMotors();
            LOGV3(DEBUG_STEPPERS, "[STEPPERS]: moveStepperBy: AZ from %l to %l", AZ::position(), AZ::position() + steps);
            AZ::slewBy(steps);
#endif
            break;
        case ALTITUDE_STEPS:
#if ALT_STEPPER_TYPE != STEPPER_TYPE_NONE
            enableAzAltMotors();
            ALT::slewBy(steps);
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

    if ((DEC::slewingProgress() < 1.0f) && (RA::slewingProgress() < 1.0f))
    {
        // Both axes moving to target
        float decDist = 100.0f * DEC::slewingProgress();
        float raDist  = 100.0f * RA::slewingProgress();

        sprintf(scratchBuffer, "R %s %d%%", RAString(LCD_STRING | CURRENT_STRING).c_str(), (int) raDist);
        _lcdMenu->setCursor(0, 0);
        _lcdMenu->printMenu(String(scratchBuffer));
        sprintf(scratchBuffer, "D%s %d%%", DECString(LCD_STRING | CURRENT_STRING).c_str(), (int) decDist);
        _lcdMenu->setCursor(0, 1);
        _lcdMenu->printMenu(String(scratchBuffer));
        return;
    }

    if (DEC::slewingProgress() < 1.0f)
    {
        // Only DEC moving to target
        float decDist = 100.0f * DEC::slewingProgress();
        sprintf(scratchBuffer, "D%s %d%%", DECString(LCD_STRING | CURRENT_STRING).c_str(), (int) decDist);
        _lcdMenu->setCursor(0, 1);
        _lcdMenu->printMenu(String(scratchBuffer));
    }
    else if (RA::slewingProgress() < 1.0f)
    {
        // Only RAmoving to target
        float raDist = 100.0f * RA::slewingProgress();
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
        //LOGV1(DEBUG_MOUNT_VERBOSE,F("[MOUNT]: DECString: TARGET!"));
        dec = _targetDEC;
    }
    else
    {
        //LOGV1(DEBUG_MOUNT_VERBOSE,F("[MOUNT]: DECString: CURRENT!"));
        dec = currentDEC();
    }
    //LOGV5(DEBUG_INFO,F("[MOUNT]: DECString: Precheck  : %s   %s  %dm %ds"), dec.ToString(), dec.getDegreesDisplay().c_str(), dec.getMinutes(), dec.getSeconds());
    // dec.checkHours();
    // LOGV2(DEBUG_MOUNT_VERBOSE,F("[MOUNT]: DECString: Postcheck : %s"), dec.ToString());

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
    // //microsteps per driver clock tick (From TMC2209 datasheet: v[Hz] (microstep/s) = VACTUAL[2209] * 0.715Hz)
    // const int speed = (RA_STEPPER_SPEED / 2) / 0.715255737f;
    // //Duration in ms to move X degrees at half of the max speed
    // const int duration = UART_CONNECTION_TEST_TX_DEG * (_stepsPerRADegree / (RA_STEPPER_SPEED / 2)) * 1000;
    // LOGV2(DEBUG_STEPPERS, F("[STEPPERS]: uartTest: Switching RA driver to microsteps(%d) for UART test"), RA_SLEW_MICROSTEPPING);
    // _driverRA->microsteps(RA_SLEW_MICROSTEPPING == 1 ? 0 : RA_SLEW_MICROSTEPPING);
    // testUART_vactual(_driverRA, speed, duration);
    // LOGV2(DEBUG_STEPPERS, F("[STEPPERS]: uartTest: Switching RA driver to microsteps(%d) after UART test"), RA_TRACKING_MICROSTEPPING);
    _driverRA->microsteps(RA_TRACKING_MICROSTEPPING == 1 ? 0 : RA_TRACKING_MICROSTEPPING);
}
    #endif

    #if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
void Mount::testDEC_UART_TX()
{
    // //microsteps per driver clock tick (From TMC2209 datasheet: v[Hz] (microstep/s) = VACTUAL[2209] * 0.715Hz)
    // const int speed = (DEC_STEPPER_SPEED / 2) / 0.715255737f;
    // //Duration in ms to move X degrees at half of the max speed
    // const int duration = UART_CONNECTION_TEST_TX_DEG * (_stepsPerDECDegree / (DEC_STEPPER_SPEED / 2)) * 1000;
    // LOGV2(DEBUG_STEPPERS, F("[STEPPERS]: uartTest: Switching DEC driver to microsteps(%d) for UART test"), DEC_SLEW_MICROSTEPPING);
    // _driverDEC->microsteps(DEC_SLEW_MICROSTEPPING == 1 ? 0 : DEC_SLEW_MICROSTEPPING);
    // testUART_vactual(_driverDEC, speed, duration);
    // LOGV2(DEBUG_STEPPERS, F("[STEPPERS]: uartTest: Switching DEC driver to microsteps(%d) after UART test"), DEC_GUIDE_MICROSTEPPING);
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
    if (millis() - _lastTRKCheck > 5000)
    {
        float trackedHours       = RA::trackingPosition().deg() / 15.0f;
        float homeRA             = _zeroPosRA.getTotalHours() + trackedHours;
        float const RALimit      = RA_TRACKING_LIMIT;
        float homeCurrentDeltaRA = homeRA - currentRA().getTotalHours();

        LOGV2(DEBUG_MOUNT_VERBOSE, F("[MOUNT]: checkRALimit: homeRAdelta: %f"), homeCurrentDeltaRA);
        while (homeCurrentDeltaRA > 12)
            homeCurrentDeltaRA = homeCurrentDeltaRA - 24;
        while (homeCurrentDeltaRA < -12)
            homeCurrentDeltaRA = homeCurrentDeltaRA + 24;

        if (homeCurrentDeltaRA > RALimit)
        {
            LOGV1(DEBUG_MOUNT, F("[MOUNT]: checkRALimit: Tracking limit reached"));
            stopSlewing(TRACKING);
        }
        _lastTRKCheck = millis();
    }
}

template <> Angle Mount::position<Mount::RA>()
{
    auto trackedTime = _totalTrackingTime + ((_recentTrackingStartTime) ? millis() - _recentTrackingStartTime : 0);
    return RA::position() - (RA::TRACKING_SPEED * trackedTime);
}

template <> Angle Mount::trackingPosition<Mount::RA>()
{
    auto trackedTime = _totalTrackingTime + ((_recentTrackingStartTime) ? millis() - _recentTrackingStartTime : 0);
    return RA::TRACKING_SPEED * trackedTime;
}

template <> void Mount::setPosition<Mount::RA>(Angle value)
{
    Mount::RA::setPosition(value);
    _totalTrackingTime       = 0;
    _recentTrackingStartTime = millis();
}
