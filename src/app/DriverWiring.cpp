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

namespace
{
/// No pin: the board does not have this line.
constexpr int kNoPin = -1;

/// What `initDriverPins()` needs to know about one axis, and nothing else.
struct AxisPinPlan {
    const char *startupLog;  ///< Logged before the pins are touched; nullptr for a silent axis.
    const char *uartLog;     ///< Logged when the axis has a hardware UART to open; may be nullptr.
    bool logsAtFocusLevel;   ///< Those two go through `DEBUG_FOCUS` rather than `DEBUG_ANY`.
    int enablePin;
    bool enableIsLow;     ///< LOW enables RA/DEC; HIGH is the idle state for AZ/ALT/focus.
    int ms0, ms1, ms2;    ///< Microstep-select lines, `kNoPin` when absent or unused.
    int diag;             ///< TMC2209 DIAG input, `kNoPin` when absent.
    void (*beginUart)();  ///< Opens this axis's hardware UART; nullptr when it has none.
};

/// Opens a driver UART at the baud this platform wants. One copy for five axes: it was written out
/// per axis before, so changing it meant changing five blocks. Templated because the concrete type
/// differs — `HardwareSerial` on AVR/ESP32, `SerialUART` on RP2040.
template <typename SerialType> void beginDriverUart(SerialType &uart, int rxPin, int txPin)
{
    (void) rxPin;
    (void) txPin;
#ifdef OAE
    // The OAE ESP32 carrier picks its UART pins at run time, so they go to begin().
    uart.begin(57600, SERIAL_8N1, rxPin, txPin);
#elif defined(ARDUINO_ARCH_RP2040)
    uart.begin(115200);  // TMCStepper requires 115200 baud on RP2040
#else
    uart.begin(57600);  // Start HardwareSerial comms with driver
#endif
}

AxisPinPlan axisPlan(app::DriverAxis axis)
{
    AxisPinPlan plan = {};
    plan.ms0 = plan.ms1 = plan.ms2 = kNoPin;
    plan.diag                      = kNoPin;
    plan.beginUart                 = nullptr;

    switch (axis)
    {
        case app::DriverAxis::RightAscension:
            plan.startupLog  = "[SYSTEM]: Initializing RA microstepping/driver pins...";
            plan.uartLog     = "[SYSTEM]: Initializing TMC2209 UART pins and Serial port for RA...";
            plan.enablePin   = RA_EN_PIN;
            plan.enableIsLow = true;
#if (RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE) || (RA_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC)
    #ifdef RA_MS0_PIN
            plan.ms0 = RA_MS0_PIN;
    #endif
    #ifdef RA_MS1_PIN
            plan.ms1 = RA_MS1_PIN;
    #endif
    #ifdef RA_MS2_PIN
            plan.ms2 = RA_MS2_PIN;
    #endif
#endif
#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    #ifdef RA_DIAG_PIN
            plan.diag = RA_DIAG_PIN;
    #endif
    #ifdef RA_SERIAL_PORT
        #ifdef OAE
            plan.beginUart = []() { beginDriverUart(RA_SERIAL_PORT, RA_RX_PIN, RA_TX_PIN); };
        #else
            plan.beginUart = []() { beginDriverUart(RA_SERIAL_PORT, -1, -1); };
        #endif
    #endif
#endif
            break;

        case app::DriverAxis::Declination:
            // The DEC startup line carries its pin number, so `setup()` logs it at the call site.
            plan.startupLog  = nullptr;
            plan.uartLog     = "[SYSTEM]: Initializing TMC2209 UART pins and Serial port for DEC...";
            plan.enablePin   = DEC_EN_PIN;
            plan.enableIsLow = true;
#if (DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE) || (DEC_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC)
    #ifdef DEC_MS0_PIN
            plan.ms0 = DEC_MS0_PIN;
    #endif
    #ifdef DEC_MS1_PIN
            plan.ms1 = DEC_MS1_PIN;
    #endif
    #ifdef DEC_MS2_PIN
            plan.ms2 = DEC_MS2_PIN;
    #endif
#endif
#if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    #ifdef DEC_DIAG_PIN
            plan.diag = DEC_DIAG_PIN;
    #endif
    #ifdef DEC_SERIAL_PORT
        #ifdef OAE
            plan.beginUart = []() { beginDriverUart(DEC_SERIAL_PORT, DEC_RX_PIN, DEC_TX_PIN); };
        #else
            plan.beginUart = []() { beginDriverUart(DEC_SERIAL_PORT, -1, -1); };
        #endif
    #endif
#endif
            break;

        case app::DriverAxis::Azimuth:
#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
            plan.startupLog  = "[SYSTEM]: Initializing AZ microstepping/driver pins...";
            plan.enablePin   = AZ_EN_PIN;
            plan.enableIsLow = false;
    #if AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
        #ifdef AZ_DIAG_PIN
            plan.diag = AZ_DIAG_PIN;
        #endif
        #ifdef AZ_SERIAL_PORT
            plan.beginUart = []() { beginDriverUart(AZ_SERIAL_PORT, -1, -1); };
        #endif
    #endif
#endif
            break;

        case app::DriverAxis::Altitude:
#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
            plan.startupLog  = "[SYSTEM]: Initializing ALT microstepping/driver pins...";
            plan.enablePin   = ALT_EN_PIN;
            plan.enableIsLow = false;
    #if ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
        #ifdef ALT_DIAG_PIN
            plan.diag = ALT_DIAG_PIN;
        #endif
        #ifdef ALT_SERIAL_PORT
            plan.beginUart = []() { beginDriverUart(ALT_SERIAL_PORT, -1, -1); };
        #endif
    #endif
#endif
            break;

        case app::DriverAxis::Focuser:
#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
            plan.startupLog       = "[FOCUS]: setup(): focus disabling enable pin";
            plan.uartLog          = "[FOCUS]: setup(): focus TMC2209U starting comms";
            plan.logsAtFocusLevel = true;
            plan.enablePin        = FOCUS_EN_PIN;
            plan.enableIsLow      = false;
    #if FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
        #ifdef FOCUS_SERIAL_PORT
            plan.beginUart = []() { beginDriverUart(FOCUS_SERIAL_PORT, -1, -1); };
        #endif
    #endif
#endif
            break;
    }

    return plan;
}

}  // namespace

namespace app
{

void initDriverPins(DriverAxis axis)
{
    const AxisPinPlan plan = axisPlan(axis);

    if (plan.startupLog != nullptr)
    {
        if (plan.logsAtFocusLevel)
        {
            LOG(DEBUG_FOCUS, "%s", plan.startupLog);
        }
        else
        {
            LOG(DEBUG_ANY, "%s", plan.startupLog);
        }
    }

    pinMode(plan.enablePin, OUTPUT);
    digitalWrite(plan.enablePin, plan.enableIsLow ? LOW : HIGH);

    // The MS lines are only wired on the standalone drivers; a UART driver takes its microstep
    // count over the serial link instead.
    if (plan.ms0 != kNoPin)
    {
        digitalWrite(plan.ms0, HIGH);
    }
    if (plan.ms1 != kNoPin)
    {
        digitalWrite(plan.ms1, HIGH);
    }
    if (plan.ms2 != kNoPin)
    {
        digitalWrite(plan.ms2, HIGH);
    }
    if (plan.diag != kNoPin)
    {
        pinMode(plan.diag, INPUT);
    }

    if (plan.beginUart == nullptr)
    {
        return;
    }
    if (plan.uartLog != nullptr)
    {
        if (plan.logsAtFocusLevel)
        {
            LOG(DEBUG_FOCUS, "%s", plan.uartLog);
        }
        else
        {
            LOG(DEBUG_ANY, "%s", plan.uartLog);
        }
    }
    plan.beginUart();
}

}  // namespace app
