#include "DriverWiring.hpp"

#include "../../Configuration.hpp"
#include "ports/ILogger.hpp"
/// The TMC2209 sense resistor every StepStick on these boards uses.
constexpr float kRSense = 0.11F;

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

/// Whether any axis on this board is a UART TMC2209. Without this the shared helpers below would be
/// defined and never called on a board that runs A4988s throughout, which `-Werror` rejects.
#if (RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART) || (DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)                                          \
    || ((AZ_STEPPER_TYPE != STEPPER_TYPE_NONE) && (AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART))                                            \
    || ((ALT_STEPPER_TYPE != STEPPER_TYPE_NONE) && (ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART))                                          \
    || ((FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE) && (FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART))

/// How one driver's UART is reached. `SW_SERIAL_UART` is a board constant, so exactly one of these
/// forms exists per build and the transport choice never reaches the axis builders.
struct Tmc2209Port {
    #if SW_SERIAL_UART == 0
    explicit Tmc2209Port(Stream *uart) : serial(uart)
    {
    }
    Stream *serial;
    #else
    Tmc2209Port(uint16_t rxPin, uint16_t txPin) : rx(rxPin), tx(txPin)
    {
    }
    uint16_t rx;
    uint16_t tx;
    #endif
};

    /// `TMC2209_PORT(RA)` is `Tmc2209Port(&RA_SERIAL_PORT)` on hardware-serial boards and
    /// `Tmc2209Port(RA_SERIAL_PORT_RX, RA_SERIAL_PORT_TX)` on the software-serial ones. One `#if`
    /// instead of one per axis in both `attachDrivers()` and the builders.
    #if SW_SERIAL_UART == 0
        #define TMC2209_PORT(prefix) Tmc2209Port(&prefix##_SERIAL_PORT)
    #else
        #define TMC2209_PORT(prefix) Tmc2209Port(prefix##_SERIAL_PORT_RX, prefix##_SERIAL_PORT_TX)
    #endif

/// The register writes that are per-axis rather than shared. Each one is a single register the
/// board's configuration turns on for that axis only; everything outside this struct is the one
/// sequence in `buildTmc2209Axis()`.
struct Tmc2209Traits {
    bool pdnDisable;           ///< RA and DEC: hold the driver out of standby with the UART enabled.
    bool standstillPowerDown;  ///< DEC only: `iholddelay`/`TPOWERDOWN` ramp the hold current down.
    bool trimClock;            ///< RA only: `fclktrim(4)`.
};

/// Everything `buildTmc2209Axis()` needs to know about one axis.
struct Tmc2209Axis {
    const char *name;            ///< "RA", "DEC", "AZ", "ALT", "Focus" — used in the rms diagnostics.
    const char *connectionName;  ///< The shorter "FOC" the self-test message has always used.
    const char *logPrefix;       ///< "[MOUNT]" or "[FOCUS]".
    uint16_t logLevel;           ///< `DEBUG_STEPPERS`, or `DEBUG_STEPPERS | DEBUG_FOCUS`.
    int rmsCurrent;
    float holdMultiplier;
    int stallValue;
    bool spreadCycle;
    uint16_t microsteps;  ///< Register value, already converted from the microstep setting.
    int enablePin;        ///< Driven high when the UART self-test fails, so the motor cannot run.
    Tmc2209Traits traits;
};

/// Opens the transport and applies its two preamble registers. The software-serial driver needs
/// them before the first UART write can be understood; the hardware-serial one needs nothing.
ports::IMotorDriver *openTmc2209(const Tmc2209Port &port, float rsense, byte address)
{
    #if SW_SERIAL_UART == 0
    auto *driver = new adapters::Tmc2209Driver(port.serial, rsense, address);
    driver->begin();
    #else
    auto *driver = new adapters::Tmc2209Driver(port.rx, port.tx, rsense, address);
    driver->beginSerial(19200);
    driver->mstep_reg_select(true);
    driver->pdn_disable(true);
    #endif
    return driver;
}

/// The one configuration sequence every TMC2209 axis shares.
///
/// The order is the hardware-serial one. The software-serial builders used to write `semin` before
/// `microsteps`; `semin` (CoolStep minimum) and `microsteps` are different registers, so the swap
/// changed nothing, and one order for both transports is the point of this function.
ports::IMotorDriver *buildTmc2209Axis(const Tmc2209Port &port, float rsense, byte address, const Tmc2209Axis &axis)
{
    ports::IMotorDriver *driver = openTmc2209(port, rsense, address);

    bool uartRxConnected = false;
    #if UART_CONNECTION_TEST_TXRX == 1
    uartRxConnected = testDriverConnection(driver, axis.connectionName);
    if (!uartRxConnected)
    {
        // Disable the motor for safety if the UART connection fails, so it cannot run at whatever
        // rms_current the driver happens to be sitting at.
        hal::ArduinoGpioPin(axis.enablePin).write(hal::PinLevel::High);
    }
    #endif

    driver->toff(0);
    #if USE_VREF == 0  // By default Vref is ignored when using UART to specify rms current.
    driver->I_scale_analog(false);
    #endif
    OAT_LOG(logSink, axis.logLevel, "%s: Requested %s motor rms_current: %d mA", axis.logPrefix, axis.name, axis.rmsCurrent);
    driver->rms_current(axis.rmsCurrent, axis.holdMultiplier);
    if (axis.traits.pdnDisable)
    {
        driver->pdn_disable(1);
    }
    if (axis.traits.standstillPowerDown)
    {
        driver->iholddelay(15);
        driver->TPOWERDOWN(255);
    }
    driver->toff(adapters::kTmc2209Toff);
    driver->en_spreadCycle(axis.spreadCycle);
    driver->blank_time(24);
    driver->microsteps(axis.microsteps);
    if (axis.traits.trimClock)
    {
        driver->fclktrim(4);
    }
    driver->TCOOLTHRS(0xFFFFF);
    driver->semin(0);  // disable CoolStep so that current is consistent
    driver->SGTHRS(axis.stallValue);

    if (uartRxConnected)
    {
        OAT_LOG(logSink, axis.logLevel, "%s: Actual %s motor rms_current: %d mA", axis.logPrefix, axis.name, driver->rms_current());
        OAT_LOG(logSink, axis.logLevel, "%s: Actual %s CS value: %d", axis.logPrefix, axis.name, driver->cs_actual());
        OAT_LOG(logSink, axis.logLevel, "%s: Actual %s vsense: %d", axis.logPrefix, axis.name, driver->vsense());
    }
    return driver;
}

#endif

/////////////////////////////////
//
// The five axis builders. Each one is the axis's own facts: which pin enables it, which microstep
// count it starts in, whether its board wants stealthChop, and the register traits above.
//
/////////////////////////////////

#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
ports::IMotorDriver *buildRADriver(
    const core::MountConfig &config, const Tmc2209Port &port, float rsense, byte address, int rmsCurrent, int stallValue)
{
    const Tmc2209Axis axis = {"RA",
                              "RA",
                              "[MOUNT]",
                              DEBUG_STEPPERS,
                              rmsCurrent,
                              1.0f,  // holdMultiplier = 1 so ihold == irun
                              stallValue,
                              RA_UART_STEALTH_MODE == 0,
                              static_cast<uint16_t>(core::microstepRegisterValue(config.raTrackingMicrosteps())),  // starts in tracking
                              RA_EN_PIN,
                              {/*pdnDisable=*/true, /*standstillPowerDown=*/false, /*trimClock=*/true}};
    return buildTmc2209Axis(port, rsense, address, axis);
}
#endif

#if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
/// Only the hardware-serial DEC variant has ever programmed `iholddelay`/`TPOWERDOWN`; the
/// software-serial one never did, and the two have diverged since. This keeps each board exactly as
/// it was — unifying them would change the standstill hold current on the RAMPS/MKS family, which is
/// a behaviour change and not part of this refactor. Worth a follow-up: the hardware-serial form is
/// the intended one.
constexpr bool kDecStandstillPowerDown = (SW_SERIAL_UART == 0);

ports::IMotorDriver *buildDECDriver(
    const core::MountConfig &config, const Tmc2209Port &port, float rsense, byte address, int rmsCurrent, int stallValue)
{
    const Tmc2209Axis axis = {"DEC",
                              "DEC",
                              "[MOUNT]",
                              DEBUG_STEPPERS,
                              rmsCurrent,
                              1.0f,
                              stallValue,
                              DEC_UART_STEALTH_MODE == 0,
                              static_cast<uint16_t>(core::microstepRegisterValue(config.decGuidingMicrosteps())),  // starts in guiding
                              DEC_EN_PIN,
                              {/*pdnDisable=*/true, /*standstillPowerDown=*/kDecStandstillPowerDown, /*trimClock=*/false}};
    return buildTmc2209Axis(port, rsense, address, axis);
}
#endif

#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE) && (AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
ports::IMotorDriver *buildAZDriver(const Tmc2209Port &port, float rsense, byte address, int rmsCurrent, int stallValue)
{
    const Tmc2209Axis axis = {"AZ",
                              "AZ",
                              "[MOUNT]",
                              DEBUG_STEPPERS,
                              rmsCurrent,
                              AZ_MOTOR_HOLD_SETTING / 100.0,
                              stallValue,
                              false,
                              static_cast<uint16_t>(core::microstepRegisterValue(AZ_MICROSTEPPING)),
                              AZ_EN_PIN,
                              {/*pdnDisable=*/false, /*standstillPowerDown=*/false, /*trimClock=*/false}};
    return buildTmc2209Axis(port, rsense, address, axis);
}
#endif

#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE) && (ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
ports::IMotorDriver *buildALTDriver(const Tmc2209Port &port, float rsense, byte address, int rmsCurrent, int stallValue)
{
    const Tmc2209Axis axis = {"ALT",
                              "ALT",
                              "[MOUNT]",
                              DEBUG_STEPPERS,
                              rmsCurrent,
                              ALT_MOTOR_HOLD_SETTING / 100.0,
                              stallValue,
                              false,
                              static_cast<uint16_t>(core::microstepRegisterValue(ALT_MICROSTEPPING)),
                              ALT_EN_PIN,
                              {/*pdnDisable=*/false, /*standstillPowerDown=*/false, /*trimClock=*/false}};
    return buildTmc2209Axis(port, rsense, address, axis);
}
#endif

#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE) && (FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
ports::IMotorDriver *buildFocusDriver(const Tmc2209Port &port, float rsense, byte address, int rmsCurrent, int stallValue)
{
    const Tmc2209Axis axis      = {"Focus",
                                   "FOC",
                                   "[FOCUS]",
                                   DEBUG_STEPPERS | DEBUG_FOCUS,
                                   rmsCurrent,
                                   FOCUSER_MOTOR_HOLD_SETTING / 100.f,
                                   stallValue,
                                   FOCUS_UART_STEALTH_MODE == 0,
                                   static_cast<uint16_t>(core::microstepRegisterValue(FOCUS_MICROSTEPPING)),
                                   FOCUS_EN_PIN,
                                   {/*pdnDisable=*/false, /*standstillPowerDown=*/false, /*trimClock=*/false}};
    ports::IMotorDriver *driver = buildTmc2209Axis(port, rsense, address, axis);

    #if FOCUSER_ALWAYS_ON == 1
    OAT_LOG(logSink, DEBUG_FOCUS, "[FOCUS]: Always on -> TMC2209U enabling driver pin.");
    hal::ArduinoGpioPin(FOCUS_EN_PIN).write(hal::PinLevel::Low);  // Logic LOW to enable driver
    #endif
    return driver;
}
#endif

}  // namespace

namespace app
{

void attachDrivers(app::MountExecutor &mount)
{
    const core::MountConfig config = buildMountConfig();
    app::AxisBank::Drivers drivers;

    (void) config;  // only the RA and DEC UART builders read it

#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    drivers.ra = buildRADriver(config, TMC2209_PORT(RA), kRSense, RA_DRIVER_ADDRESS, RA_RMSCURRENT, RA_STALL_VALUE);
#endif
#if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    drivers.dec = buildDECDriver(config, TMC2209_PORT(DEC), kRSense, DEC_DRIVER_ADDRESS, DEC_RMSCURRENT, DEC_STALL_VALUE);
#endif

#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE) && (AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
    drivers.az = buildAZDriver(TMC2209_PORT(AZ), kRSense, AZ_DRIVER_ADDRESS, AZ_RMSCURRENT, AZ_STALL_VALUE);
#endif
#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE) && (ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
    drivers.alt = buildALTDriver(TMC2209_PORT(ALT), kRSense, ALT_DRIVER_ADDRESS, ALT_RMSCURRENT, ALT_STALL_VALUE);
#endif
#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE) && (FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
    drivers.focus = buildFocusDriver(TMC2209_PORT(FOCUS), kRSense, FOCUS_DRIVER_ADDRESS, FOCUS_RMSCURRENT, FOCUS_STALL_VALUE);
#endif

    mount.attachDrivers(drivers);
}

}  // namespace app
