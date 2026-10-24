#include "DriverWiring.hpp"

#include "../../Configuration.hpp"
#include "../Utility.hpp"
#include "../a_inits.hpp"

#include "BoardWiring.hpp"
#include "MountConfigBuilder.hpp"
#include "core/Microsteps.hpp"

#include "adapters/Tmc2209Driver.hpp"
#include "hal/arduino/ArduinoGpioPin.hpp"

// The driver register programming is board knowledge, so it lives here with the pin maps and the
// `Configuration*.hpp` macros rather than in `Mount`. The bodies are the ones `app::MountExecutor::configure*
// driver` used to hold, moved verbatim; only their owner changed.

namespace
{
/// How many times the UART self-test asks a driver to answer before giving up on it.
#define UART_CONNECTION_TEST_RETRIES 5

#if UART_CONNECTION_TEST_TXRX == 1
/// Runs the UART self-test the way the mount used to: retries, then reports the current it read.
///
/// Guarded by the switch that decides whether any caller exists, not by `TEST_VERIFY_MODE`: the
/// bring-up screen reaches the driver through `connectToDriver()`, so a board that enables the
/// test screen without the UART self-test would otherwise define this helper and never call it,
/// which `-Werror` rejects.
bool testDriverConnection(ports::IMotorDriver *driver, const char *driverKind, uint16_t *rmsCurrent = nullptr)
{
    if (driver == nullptr)
    {
        if (rmsCurrent != nullptr)
        {
            *rmsCurrent = 0;
        }
        return false;
    }

    OAT_LOG(logSink, DEBUG_STEPPERS, "[STEPPERS]: Testing UART Connection to %s driver...", driverKind);
    for (int i = 0; i < UART_CONNECTION_TEST_RETRIES; i++)
    {
        if (driver->testConnection())
        {
            OAT_LOG(logSink, DEBUG_STEPPERS, "[STEPPERS]: UART connection to %s driver successful.", driverKind);
            if (rmsCurrent != nullptr)
            {
                *rmsCurrent = driver->rms_current();
            }
            return true;
        }
        delay(500);
    }
    OAT_LOG(logSink, DEBUG_STEPPERS, "[STEPPERS]: UART connection to %s driver failed.", driverKind);
    if (rmsCurrent != nullptr)
    {
        *rmsCurrent = 0;
    }
    return false;
}
#endif

#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    #if SW_SERIAL_UART == 0
ports::IMotorDriver *buildRADriver(
    const core::MountConfig &config, Stream *serial, float rsense, byte driveraddress, int rmscurrent, int stallvalue)
{
    ports::IMotorDriver *raDriver = nullptr;
    raDriver                      = new adapters::Tmc2209Driver(serial, rsense, driveraddress);
    raDriver->begin();
    bool UART_Rx_connected = false;
        #if UART_CONNECTION_TEST_TXRX == 1
    UART_Rx_connected = testDriverConnection(raDriver, "RA");
    if (!UART_Rx_connected)
    {
        // Disable motor for safety reasons if UART connection fails to avoid operating at incorrect rms_current
        hal::ArduinoGpioPin(RA_EN_PIN).write(hal::PinLevel::High);
    }
        #endif
    raDriver->toff(0);
        #if USE_VREF == 0  //By default, Vref is ignored when using UART to specify rms current.
    raDriver->I_scale_analog(false);
        #endif
    OAT_LOG(logSink, DEBUG_STEPPERS, "[MOUNT]: Requested RA motor rms_current: %d mA", rmscurrent);
    raDriver->rms_current(rmscurrent, 1.0f);  //holdMultiplier = 1 to set ihold = irun
    raDriver->pdn_disable(1);
    raDriver->toff(adapters::kTmc2209Toff);
    raDriver->en_spreadCycle(RA_UART_STEALTH_MODE == 0);
    raDriver->blank_time(24);
    raDriver->microsteps(core::microstepRegisterValue(config.raTrackingMicrosteps()));  // System starts in tracking mode
    raDriver->fclktrim(4);
    raDriver->TCOOLTHRS(0xFFFFF);  //xFFFFF);
    raDriver->semin(0);            //disable CoolStep so that current is consistent
    raDriver->SGTHRS(stallvalue);
    if (UART_Rx_connected)
    {
        OAT_LOG(logSink, DEBUG_STEPPERS, "[MOUNT]: Actual RA motor rms_current: %d mA", raDriver->rms_current());
        OAT_LOG(logSink, DEBUG_STEPPERS, "[MOUNT]: Actual RA CS value: %d", raDriver->cs_actual());
        OAT_LOG(logSink, DEBUG_STEPPERS, "[MOUNT]: Actual RA vsense: %d", raDriver->vsense());
    }
    return raDriver;
}

    #elif SW_SERIAL_UART == 1

ports::IMotorDriver *buildRADriver(
    const core::MountConfig &config, uint16_t RA_SW_RX, uint16_t RA_SW_TX, float rsense, byte driveraddress, int rmscurrent, int stallvalue)
{
    ports::IMotorDriver *raDriver = nullptr;
    raDriver                      = new adapters::Tmc2209Driver(RA_SW_RX, RA_SW_TX, rsense, driveraddress);
    raDriver->beginSerial(19200);
    raDriver->mstep_reg_select(true);
    raDriver->pdn_disable(true);
    bool UART_Rx_connected = false;
        #if UART_CONNECTION_TEST_TXRX == 1
    UART_Rx_connected = testDriverConnection(raDriver, "RA");
    if (!UART_Rx_connected)
    {
        // Disable motor for safety reasons if UART connection fails to avoid operating at incorrect rms_current
        hal::ArduinoGpioPin(RA_EN_PIN).write(hal::PinLevel::High);
    }
        #endif
    raDriver->toff(0);
        #if USE_VREF == 0  //By default, Vref is ignored when using UART to specify rms current.
    raDriver->I_scale_analog(false);
        #endif
    OAT_LOG(logSink, DEBUG_STEPPERS, "[MOUNT]: Requested RA motor rms_current: %d mA", rmscurrent);
    raDriver->rms_current(rmscurrent, 1.0f);  //holdMultiplier = 1 to set ihold = irun
    raDriver->toff(adapters::kTmc2209Toff);
    raDriver->en_spreadCycle(RA_UART_STEALTH_MODE == 0);
    raDriver->blank_time(24);
    raDriver->semin(0);                                                                 //disable CoolStep so that current is consistent
    raDriver->microsteps(core::microstepRegisterValue(config.raTrackingMicrosteps()));  // System starts in tracking mode
    raDriver->fclktrim(4);
    raDriver->TCOOLTHRS(0xFFFFF);  //xFFFFF);
    raDriver->SGTHRS(stallvalue);
    if (UART_Rx_connected)
    {
        OAT_LOG(logSink, DEBUG_STEPPERS, "[MOUNT]: Actual RA motor rms_current: %d mA", raDriver->rms_current());
        OAT_LOG(logSink, DEBUG_STEPPERS, "[MOUNT]: Actual RA CS value: %d", raDriver->cs_actual());
        OAT_LOG(logSink, DEBUG_STEPPERS, "[MOUNT]: Actual RA vsense: %d", raDriver->vsense());
    }
    return raDriver;
}
    #endif
#endif

/////////////////////////////////
//
// buildDECDriver
// TMC2209 UART only
/////////////////////////////////
#if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    #if SW_SERIAL_UART == 0
ports::IMotorDriver *buildDECDriver(
    const core::MountConfig &config, Stream *serial, float rsense, byte driveraddress, int rmscurrent, int stallvalue)
{
    ports::IMotorDriver *decDriver = nullptr;
    decDriver                      = new adapters::Tmc2209Driver(serial, rsense, driveraddress);
    decDriver->begin();
    bool UART_Rx_connected = false;
        #if UART_CONNECTION_TEST_TXRX == 1
    UART_Rx_connected = testDriverConnection(decDriver, "DEC");
    if (!UART_Rx_connected)
    {
        // Disable motor for safety reasons if UART connection fails to avoid operating at incorrect rms_current
        hal::ArduinoGpioPin(DEC_EN_PIN).write(hal::PinLevel::High);
    }
        #endif
    decDriver->toff(0);
        #if USE_VREF == 0  //By default, Vref is ignored when using UART to specify rms current.
    decDriver->I_scale_analog(false);
        #endif
    OAT_LOG(logSink, DEBUG_STEPPERS, "[MOUNT]: Requested DEC motor rms_current: %d mA", rmscurrent);
    decDriver->rms_current(rmscurrent, 1.0f);  //holdMultiplier = 1 to set ihold = irun
    decDriver->pdn_disable(1);
    decDriver->iholddelay(15);
    decDriver->TPOWERDOWN(255);
    decDriver->toff(adapters::kTmc2209Toff);
    decDriver->en_spreadCycle(DEC_UART_STEALTH_MODE == 0);
    decDriver->blank_time(24);
    decDriver->microsteps(
        core::microstepRegisterValue(config.decGuidingMicrosteps()));  // If 1 then disable microstepping. Start with Guide microsteps.
    decDriver->TCOOLTHRS(0xFFFFF);
    decDriver->semin(0);  //disable CoolStep so that current is consistent
    decDriver->SGTHRS(stallvalue);
    if (UART_Rx_connected)
    {
        OAT_LOG(logSink, DEBUG_STEPPERS, "[MOUNT]: Actual DEC motor rms_current: %d mA", decDriver->rms_current());
        OAT_LOG(logSink, DEBUG_STEPPERS, "[MOUNT]: Actual DEC CS value: %d", decDriver->cs_actual());
        OAT_LOG(logSink, DEBUG_STEPPERS, "[MOUNT]: Actual DEC vsense: %d", decDriver->vsense());
    }
    return decDriver;
}

    #elif SW_SERIAL_UART == 1

ports::IMotorDriver *buildDECDriver(const core::MountConfig &config,
                                    uint16_t DEC_SW_RX,
                                    uint16_t DEC_SW_TX,
                                    float rsense,
                                    byte driveraddress,
                                    int rmscurrent,
                                    int stallvalue)
{
    ports::IMotorDriver *decDriver = nullptr;
    decDriver                      = new adapters::Tmc2209Driver(DEC_SW_RX, DEC_SW_TX, rsense, driveraddress);
    decDriver->beginSerial(19200);
    decDriver->mstep_reg_select(true);
    decDriver->pdn_disable(true);
    bool UART_Rx_connected = false;
        #if UART_CONNECTION_TEST_TXRX == 1
    UART_Rx_connected = testDriverConnection(decDriver, "DEC");
    if (!UART_Rx_connected)
    {
        // Disable motor for safety reasons if UART connection fails to avoid operating at incorrect rms_current
        hal::ArduinoGpioPin(DEC_EN_PIN).write(hal::PinLevel::High);
    }
        #endif
    decDriver->toff(0);
        #if USE_VREF == 0  //By default, Vref is ignored when using UART to specify rms current.
    decDriver->I_scale_analog(false);
        #endif
    OAT_LOG(logSink, DEBUG_STEPPERS, "[MOUNT]: Requested DEC motor rms_current: %d mA", rmscurrent);
    decDriver->rms_current(rmscurrent, 1.0f);  //holdMultiplier = 1 to set ihold = irun
    decDriver->toff(adapters::kTmc2209Toff);
    decDriver->en_spreadCycle(DEC_UART_STEALTH_MODE == 0);
    decDriver->blank_time(24);
    decDriver->microsteps(
        core::microstepRegisterValue(config.decGuidingMicrosteps()));  // If 1 then disable microstepping. Start with Guide microsteps
    decDriver->TCOOLTHRS(0xFFFFF);
    decDriver->semin(0);  //disable CoolStep so that current is consistent
    decDriver->SGTHRS(stallvalue);
    if (UART_Rx_connected)
    {
        OAT_LOG(logSink, DEBUG_STEPPERS, "[MOUNT]: Actual DEC motor rms_current: %d mA", decDriver->rms_current());
        OAT_LOG(logSink, DEBUG_STEPPERS, "[MOUNT]: Actual DEC CS value: %d", decDriver->cs_actual());
        OAT_LOG(logSink, DEBUG_STEPPERS, "[MOUNT]: Actual DEC vsense: %d", decDriver->vsense());
    }
    return decDriver;
}
    #endif
#endif

/////////////////////////////////
//
// buildAZDriver
// TMC2209 UART only
/////////////////////////////////
#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE) && (AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
    #if SW_SERIAL_UART == 0
ports::IMotorDriver *buildAZDriver(Stream *serial, float rsense, byte driveraddress, int rmscurrent, int stallvalue)
{
    ports::IMotorDriver *azDriver = nullptr;
    azDriver                      = new adapters::Tmc2209Driver(serial, rsense, driveraddress);
    azDriver->begin();
    bool UART_Rx_connected = false;
        #if UART_CONNECTION_TEST_TXRX == 1
    UART_Rx_connected = testDriverConnection(azDriver, "AZ");
    if (!UART_Rx_connected)
    {
        // Disable motor for safety reasons if UART connection fails to avoid operating at incorrect rms_current
        hal::ArduinoGpioPin(AZ_EN_PIN).write(hal::PinLevel::High);
    }
        #endif
    azDriver->toff(0);
        #if USE_VREF == 0  //By default, Vref is ignored when using UART to specify rms current.
    azDriver->I_scale_analog(false);
        #endif
    OAT_LOG(logSink, DEBUG_STEPPERS, "[MOUNT]: Requested AZ motor rms_current: %d mA", rmscurrent);
    azDriver->rms_current(rmscurrent, AZ_MOTOR_HOLD_SETTING / 100.0);  //holdMultiplier = 1 to set ihold = irun
    azDriver->toff(adapters::kTmc2209Toff);
    azDriver->en_spreadCycle(0);
    azDriver->blank_time(24);
    azDriver->microsteps(core::microstepRegisterValue(AZ_MICROSTEPPING));  // If 1 then disable microstepping
    azDriver->TCOOLTHRS(0xFFFFF);                                          //xFFFFF);
    azDriver->semin(0);                                                    //disable CoolStep so that current is consistent
    azDriver->SGTHRS(stallvalue);
    if (UART_Rx_connected)
    {
        OAT_LOG(logSink, DEBUG_STEPPERS, "[MOUNT]: Actual AZ motor rms_current: %d mA", azDriver->rms_current());
        OAT_LOG(logSink, DEBUG_STEPPERS, "[MOUNT]: Actual AZ CS value: %d", azDriver->cs_actual());
        OAT_LOG(logSink, DEBUG_STEPPERS, "[MOUNT]: Actual AZ vsense: %d", azDriver->vsense());
    }
    return azDriver;
}

    #elif SW_SERIAL_UART == 1

ports::IMotorDriver *buildAZDriver(uint16_t AZ_SW_RX, uint16_t AZ_SW_TX, float rsense, byte driveraddress, int rmscurrent, int stallvalue)
{
    ports::IMotorDriver *azDriver = nullptr;
    azDriver                      = new adapters::Tmc2209Driver(AZ_SW_RX, AZ_SW_TX, rsense, driveraddress);
    azDriver->beginSerial(19200);
    azDriver->mstep_reg_select(true);
    azDriver->pdn_disable(true);
    bool UART_Rx_connected = false;
        #if UART_CONNECTION_TEST_TXRX == 1
    UART_Rx_connected = testDriverConnection(azDriver, "AZ");
    if (!UART_Rx_connected)
    {
        // Disable motor for safety reasons if UART connection fails to avoid operating at incorrect rms_current
        hal::ArduinoGpioPin(AZ_EN_PIN).write(hal::PinLevel::High);
    }
        #endif
    azDriver->toff(0);
        #if USE_VREF == 0  //By default, Vref is ignored when using UART to specify rms current.
    azDriver->I_scale_analog(false);
        #endif
    OAT_LOG(logSink, DEBUG_STEPPERS, "[MOUNT]: Requested AZ motor rms_current: %d mA", rmscurrent);
    azDriver->rms_current(rmscurrent, AZ_MOTOR_HOLD_SETTING / 100.0);  //holdMultiplier = 1 to set ihold = irun
    azDriver->toff(adapters::kTmc2209Toff);
    azDriver->en_spreadCycle(0);
    azDriver->blank_time(24);
    azDriver->microsteps(core::microstepRegisterValue(AZ_MICROSTEPPING));  // If 1 then disable microstepping
    azDriver->TCOOLTHRS(0xFFFFF);                                          //xFFFFF);
    azDriver->semin(0);                                                    //disable CoolStep so that current is consistent
    azDriver->SGTHRS(stallvalue);
    if (UART_Rx_connected)
    {
        OAT_LOG(logSink, DEBUG_STEPPERS, "[MOUNT]: Actual AZ motor rms_current: %d mA", azDriver->rms_current());
        OAT_LOG(logSink, DEBUG_STEPPERS, "[MOUNT]: Actual AZ CS value: %d", azDriver->cs_actual());
        OAT_LOG(logSink, DEBUG_STEPPERS, "[MOUNT]: Actual AZ vsense: %d", azDriver->vsense());
    }
    return azDriver;
}
    #endif
#endif

/////////////////////////////////
//
// buildALTDriver
// TMC2209 UART only
/////////////////////////////////
#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE) && (ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
    #if SW_SERIAL_UART == 0
ports::IMotorDriver *buildALTDriver(Stream *serial, float rsense, byte driveraddress, int rmscurrent, int stallvalue)
{
    ports::IMotorDriver *altDriver = nullptr;
    altDriver                      = new adapters::Tmc2209Driver(serial, rsense, driveraddress);
    altDriver->begin();
    bool UART_Rx_connected = false;
        #if UART_CONNECTION_TEST_TXRX == 1
    UART_Rx_connected = testDriverConnection(altDriver, "ALT");
    if (!UART_Rx_connected)
    {
        // Disable motor for safety reasons if UART connection fails to avoid operating at incorrect rms_current
        hal::ArduinoGpioPin(ALT_EN_PIN).write(hal::PinLevel::High);
    }
        #endif
    altDriver->toff(0);
        #if USE_VREF == 0  //By default, Vref is ignored when using UART to specify rms current.
    altDriver->I_scale_analog(false);
        #endif
    OAT_LOG(logSink, DEBUG_STEPPERS, "[MOUNT]: Requested ALT motor rms_current: %d mA", rmscurrent);
    altDriver->rms_current(rmscurrent, ALT_MOTOR_HOLD_SETTING / 100.0);  //holdMultiplier = 1 to set ihold = irun
    altDriver->toff(adapters::kTmc2209Toff);
    altDriver->en_spreadCycle(0);
    altDriver->blank_time(24);
    altDriver->microsteps(core::microstepRegisterValue(ALT_MICROSTEPPING));  // If 1 then disable microstepping
    altDriver->TCOOLTHRS(0xFFFFF);                                           //xFFFFF);
    altDriver->semin(0);                                                     //disable CoolStep so that current is consistent
    altDriver->SGTHRS(stallvalue);
    if (UART_Rx_connected)
    {
        OAT_LOG(logSink, DEBUG_STEPPERS, "[MOUNT]: Actual ALT motor rms_current: %d mA", altDriver->rms_current());
        OAT_LOG(logSink, DEBUG_STEPPERS, "[MOUNT]: Actual ALT CS value: %d", altDriver->cs_actual());
        OAT_LOG(logSink, DEBUG_STEPPERS, "[MOUNT]: Actual ALT vsense: %d", altDriver->vsense());
    }
    return altDriver;
}

    #elif SW_SERIAL_UART == 1

ports::IMotorDriver *buildALTDriver(
    uint16_t ALT_SW_RX, uint16_t ALT_SW_TX, float rsense, byte driveraddress, int rmscurrent, int stallvalue)
{
    ports::IMotorDriver *altDriver = nullptr;
    altDriver                      = new adapters::Tmc2209Driver(ALT_SW_RX, ALT_SW_TX, rsense, driveraddress);
    altDriver->beginSerial(19200);
    altDriver->mstep_reg_select(true);
    altDriver->pdn_disable(true);
        #if UART_CONNECTION_TEST_TXRX == 1
    bool UART_Rx_connected = false;
    UART_Rx_connected      = testDriverConnection(altDriver, "ALT");
    if (!UART_Rx_connected)
    {
        // Disable motor for safety reasons if UART connection fails to avoid operating at incorrect rms_current
        hal::ArduinoGpioPin(ALT_EN_PIN).write(hal::PinLevel::High);
    }
        #endif
    altDriver->toff(0);
        #if USE_VREF == 0  //By default, Vref is ignored when using UART to specify rms current.
    altDriver->I_scale_analog(false);
        #endif
    OAT_LOG(logSink, DEBUG_STEPPERS, "[MOUNT]: Requested ALT motor rms_current: %d mA", rmscurrent);
    altDriver->rms_current(rmscurrent, ALT_MOTOR_HOLD_SETTING / 100.0);  //holdMultiplier = 1 to set ihold = irun
    altDriver->toff(adapters::kTmc2209Toff);
    altDriver->en_spreadCycle(0);
    altDriver->blank_time(24);
    altDriver->microsteps(core::microstepRegisterValue(ALT_MICROSTEPPING));  // If 1 then disable microstepping
    altDriver->TCOOLTHRS(0xFFFFF);                                           //xFFFFF);
    altDriver->semin(0);                                                     //disable CoolStep so that current is consistent
    altDriver->SGTHRS(stallvalue);
        #if UART_CONNECTION_TEST_TXRX == 1
    if (UART_Rx_connected)
    {
        OAT_LOG(logSink, DEBUG_STEPPERS, "[MOUNT]: Actual ALT motor rms_current: %d mA", altDriver->rms_current());
        OAT_LOG(logSink, DEBUG_STEPPERS, "[MOUNT]: Actual ALT CS value: %d", altDriver->cs_actual());
        OAT_LOG(logSink, DEBUG_STEPPERS, "[MOUNT]: Actual ALT vsense: %d", altDriver->vsense());
    }
        #endif
    return altDriver;
}
    #endif
#endif

/////////////////////////////////
//
// buildFocusDriver
// TMC2209 UART only
/////////////////////////////////
#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE) && (FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
    #if SW_SERIAL_UART == 0
ports::IMotorDriver *buildFocusDriver(Stream *serial, float rsense, byte driveraddress, int rmscurrent, int stallvalue)
{
    ports::IMotorDriver *focusDriver = nullptr;
    focusDriver                      = new adapters::Tmc2209Driver(serial, rsense, driveraddress);
    focusDriver->begin();
        #if UART_CONNECTION_TEST_TXRX == 1
    bool UART_Rx_connected = false;
    UART_Rx_connected      = testDriverConnection(focusDriver, "FOC");
    if (!UART_Rx_connected)
    {
        // Disable motor for safety reasons if UART connection fails to avoid operating at incorrect rms_current
        hal::ArduinoGpioPin(FOCUS_EN_PIN).write(hal::PinLevel::High);
    }
        #endif
    focusDriver->toff(0);
        #if USE_VREF == 0  //By default, Vref is ignored when using UART to specify rms current.
    focusDriver->I_scale_analog(false);
        #endif
    OAT_LOG(logSink, DEBUG_STEPPERS | DEBUG_FOCUS, "[FOCUS]: Requested Focus motor rms_current: %d mA", rmscurrent);
    focusDriver->rms_current(rmscurrent, FOCUSER_MOTOR_HOLD_SETTING / 100.f);  //holdMultiplier = 1 to set ihold = irun
    focusDriver->toff(adapters::kTmc2209Toff);
    focusDriver->en_spreadCycle(FOCUS_UART_STEALTH_MODE == 0);
    focusDriver->blank_time(24);
    focusDriver->microsteps(core::microstepRegisterValue(FOCUS_MICROSTEPPING));  // If 1 then disable microstepping
    focusDriver->TCOOLTHRS(0xFFFFF);                                             //xFFFFF);
    focusDriver->semin(0);                                                       //disable CoolStep so that current is consistent
    focusDriver->SGTHRS(stallvalue);
        #if UART_CONNECTION_TEST_TXRX == 1
    if (UART_Rx_connected)
    {
        OAT_LOG(logSink, DEBUG_STEPPERS | DEBUG_FOCUS, "[FOCUS]: Actual Focus motor rms_current: %d mA", focusDriver->rms_current());
        OAT_LOG(logSink, DEBUG_STEPPERS | DEBUG_FOCUS, "[FOCUS]: Actual Focus CS value: %d", focusDriver->cs_actual());
        OAT_LOG(logSink, DEBUG_STEPPERS | DEBUG_FOCUS, "[FOCUS]: Actual Focus vsense: %d", focusDriver->vsense());
    }
        #endif

        #if FOCUSER_ALWAYS_ON == 1
    OAT_LOG(logSink, DEBUG_FOCUS, "[FOCUS]: Always on -> TMC2209U enabling driver pin.");
    hal::ArduinoGpioPin(FOCUS_EN_PIN).write(hal::PinLevel::Low);  // Logic LOW to enable driver
        #endif
    return focusDriver;
}

    #elif SW_SERIAL_UART == 1

ports::IMotorDriver *buildFocusDriver(
    uint16_t FOCUS_SW_RX, uint16_t FOCUS_SW_TX, float rsense, byte driveraddress, int rmscurrent, int stallvalue)
{
    ports::IMotorDriver *focusDriver = nullptr;
    focusDriver                      = new adapters::Tmc2209Driver(FOCUS_SW_RX, FOCUS_SW_TX, rsense, driveraddress);
    focusDriver->beginSerial(19200);
    focusDriver->mstep_reg_select(true);
    focusDriver->pdn_disable(true);
        #if UART_CONNECTION_TEST_TXRX == 1
    bool UART_Rx_connected = false;
    UART_Rx_connected      = testDriverConnection(focusDriver, "FOC");
    if (!UART_Rx_connected)
    {
        // Disable motor for safety reasons if UART connection fails to avoid operating at incorrect rms_current
        hal::ArduinoGpioPin(FOCUS_EN_PIN).write(hal::PinLevel::High);
    }
        #endif
    focusDriver->toff(0);
        #if USE_VREF == 0  //By default, Vref is ignored when using UART to specify rms current.
    focusDriver->I_scale_analog(false);
        #endif
    OAT_LOG(logSink, DEBUG_STEPPERS | DEBUG_FOCUS, "[FOCUS]: Requested Focus motor rms_current: %d mA", rmscurrent);
    focusDriver->rms_current(rmscurrent, FOCUSER_MOTOR_HOLD_SETTING / 100.f);  //holdMultiplier = 1 to set ihold = irun
    focusDriver->toff(adapters::kTmc2209Toff);
    focusDriver->en_spreadCycle(FOCUS_UART_STEALTH_MODE == 0);
    focusDriver->blank_time(24);
    focusDriver->microsteps(core::microstepRegisterValue(FOCUS_MICROSTEPPING));  // If 1 then disable microstepping
    focusDriver->TCOOLTHRS(0xFFFFF);                                             //xFFFFF);
    focusDriver->semin(0);                                                       //disable CoolStep so that current is consistent
    focusDriver->SGTHRS(stallvalue);
        #if UART_CONNECTION_TEST_TXRX == 1
    if (UART_Rx_connected)
    {
        OAT_LOG(logSink, DEBUG_STEPPERS | DEBUG_FOCUS, "[FOCUS]: Actual Focus motor rms_current: %d mA", focusDriver->rms_current());
        OAT_LOG(logSink, DEBUG_STEPPERS | DEBUG_FOCUS, "[FOCUS]: Actual Focus CS value: %d", focusDriver->cs_actual());
        OAT_LOG(logSink, DEBUG_STEPPERS | DEBUG_FOCUS, "[FOCUS]: Actual Focus vsense: %d", focusDriver->vsense());
    }
        #endif
        #if FOCUSER_ALWAYS_ON == 1
    OAT_LOG(logSink, DEBUG_FOCUS, "[FOCUS]: Always on -> TMC2209U enabling driver pin.");
    hal::ArduinoGpioPin(FOCUS_EN_PIN).write(hal::PinLevel::Low);  // Logic LOW to enable driver
        #endif
    return focusDriver;
}
    #endif
#endif

}  // namespace

namespace app
{

void attachDrivers(app::MountExecutor &mount)
{
    const core::MountConfig config = buildMountConfig();
    app::AxisBank::Drivers drivers;

    (void) config;  // only the UART builds read it

#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    #if SW_SERIAL_UART == 0
    drivers.ra = buildRADriver(config, &RA_SERIAL_PORT, R_SENSE, RA_DRIVER_ADDRESS, RA_RMSCURRENT, RA_STALL_VALUE);
    #elif SW_SERIAL_UART == 1
    drivers.ra = buildRADriver(config, RA_SERIAL_PORT_RX, RA_SERIAL_PORT_TX, R_SENSE, RA_DRIVER_ADDRESS, RA_RMSCURRENT, RA_STALL_VALUE);
    #endif
#endif
#if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    #if SW_SERIAL_UART == 0
    drivers.dec = buildDECDriver(config, &DEC_SERIAL_PORT, R_SENSE, DEC_DRIVER_ADDRESS, DEC_RMSCURRENT, DEC_STALL_VALUE);
    #elif SW_SERIAL_UART == 1
    drivers.dec
        = buildDECDriver(config, DEC_SERIAL_PORT_RX, DEC_SERIAL_PORT_TX, R_SENSE, DEC_DRIVER_ADDRESS, DEC_RMSCURRENT, DEC_STALL_VALUE);
    #endif
#endif

#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE) && (AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
    #if SW_SERIAL_UART == 0
    drivers.az = buildAZDriver(&AZ_SERIAL_PORT, R_SENSE, AZ_DRIVER_ADDRESS, AZ_RMSCURRENT, AZ_STALL_VALUE);
    #elif SW_SERIAL_UART == 1
    drivers.az = buildAZDriver(AZ_SERIAL_PORT_RX, AZ_SERIAL_PORT_TX, R_SENSE, AZ_DRIVER_ADDRESS, AZ_RMSCURRENT, AZ_STALL_VALUE);
    #endif
#endif
#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE) && (ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
    #if SW_SERIAL_UART == 0
    drivers.alt = buildALTDriver(&ALT_SERIAL_PORT, R_SENSE, ALT_DRIVER_ADDRESS, ALT_RMSCURRENT, ALT_STALL_VALUE);
    #elif SW_SERIAL_UART == 1
    drivers.alt = buildALTDriver(ALT_SERIAL_PORT_RX, ALT_SERIAL_PORT_TX, R_SENSE, ALT_DRIVER_ADDRESS, ALT_RMSCURRENT, ALT_STALL_VALUE);
    #endif
#endif
#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE) && (FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
    #if SW_SERIAL_UART == 0
    drivers.focus = buildFocusDriver(&FOCUS_SERIAL_PORT, R_SENSE, FOCUS_DRIVER_ADDRESS, FOCUS_RMSCURRENT, FOCUS_STALL_VALUE);
    #elif SW_SERIAL_UART == 1
    drivers.focus
        = buildFocusDriver(FOCUS_SERIAL_PORT_RX, FOCUS_SERIAL_PORT_TX, R_SENSE, FOCUS_DRIVER_ADDRESS, FOCUS_RMSCURRENT, FOCUS_STALL_VALUE);
    #endif
#endif

    (void) config;
    mount.attachDrivers(drivers);
}

}  // namespace app
