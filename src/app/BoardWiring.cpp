// The composition root: the only place that picks concrete backends and hands them to the
// domain as ports. Everything the firmware constructs, constructed once, here.
//
// Definition order is construction order, so the infrastructure is declared before the mount
// that depends on it.

#include "../../Configuration.hpp"
#include "BoardWiring.hpp"
#include "app/ui/MenuIds.hpp"

#include "adapters/EepromSettingsFormat.hpp"
#include "adapters/TinyGpsAdapter.hpp"
#include "ports/ILogger.hpp"
#include "app/MountConfigBuilder.hpp"
#include "hal/InMemoryEeprom.hpp"
#include "hal/WarningGuard.hpp"
#include "hal/arduino/ArduinoEeprom.hpp"

#include "adapters/EepromPersistentStore.hpp"
#include "adapters/NullLogger.hpp"
#include "adapters/SerialLogger.hpp"
#include "adapters/Mpu6050Gyro.hpp"
#include "core/SiteTimeService.hpp"
#include "hal/esp32/Esp32WifiStack.hpp"
#include "hal/arduino/ArduinoI2cBus.hpp"
#include "adapters/DebouncedButtons.hpp"
#include "adapters/LcdMenuDisplay.hpp"
#include "adapters/NullDisplay.hpp"
#include "hal/NullButtonMatrix.hpp"
#include "hal/arduino/AnalogKeyPad.hpp"
#include "hal/arduino/CharLcd.hpp"
#include "hal/arduino/I2cCharLcd.hpp"
#include "hal/arduino/I2cExpanderButtons.hpp"
#include "hal/arduino/JoystickButtons.hpp"
#include "hal/arduino/U8x8CharLcd.hpp"

PUSH_NO_WARNINGS
#include <EEPROM.h>
POP_NO_WARNINGS

#if (INFO_DISPLAY_TYPE == INFO_DISPLAY_TYPE_I2C_SSD1306_128x64)
    #include "adapters/Ssd1306InfoDisplay.hpp"
    #include "hal/arduino/Ssd1306Panel.hpp"
#endif

// --- UI state, declared in `a_inits.hpp` -----------------------------------------------

#if USE_GPS == 1
/// The receiver, behind the port the HA screen talks to. `begin()` is a no-op: `setup()` opens the
/// stream, and the adapter only decodes what arrives on it.
adapters::TinyGpsAdapter gpsAdapter(GPS_SERIAL_PORT);
ports::IGps *gpsPort = &gpsAdapter;
#else
ports::IGps *gpsPort = nullptr;
#endif

namespace
{
/// A guided startup owns the display from boot, so the flag starts set on builds that have one.
LoopState makeLoopState()
{
    LoopState state                     = {};
    state.inStartup                     = app::kHasGuidedStartup;
    state.okToUpdateMenu                = true;   // Suppresses rendering the first line of the menu.
    state.quitSerialOnNextButtonRelease = false;  // SELECT quits serial mode on its next release.
    return state;
}
}  // namespace

LoopState loopState = makeLoopState();

// The infrastructure the domain runs on. Declared before `mount` so it is
// constructed first; all of these are stateless or trivially initialised.
hal::ArduinoSystemClock arduinoSystemClock;
adapters::ArduinoClock clockPort(arduinoSystemClock);
adapters::ArduinoScheduler schedulerPort;

// Diagnostics. `DEBUG_LEVEL` is the same mask the legacy `LOG()` macro was compiled
// against, so a build that logged nothing still emits nothing here. With logging
// compiled out the serial sink would be dead weight, so that build gets the null
// object instead: the same pointer type, no vtable of its own, nothing to call.
#if DEBUG_LEVEL != 0
hal::ArduinoSerialPort debugSerialPort(DEBUG_SERIAL_PORT);
adapters::SerialLogger loggerPort(debugSerialPort, arduinoSystemClock, DEBUG_LEVEL);
ports::ILogger *const logSink = &loggerPort;
/// `:D#` can only dump what a sink retained; the null object has nothing to dump.
ports::ILogRetention *const logRetention = &loggerPort;
#else
adapters::NullLogger loggerPort;
ports::ILogger *const logSink            = &loggerPort;
ports::ILogRetention *const logRetention = nullptr;
#endif

// --- the optional levelling sensor ------------------------------------------------------

#if USE_GYRO_LEVEL == 1
hal::ArduinoI2cBus i2cBus;
adapters::Mpu6050Gyro mpu6050Gyro(i2cBus, clockPort);
ports::IGyro *gyro = &mpu6050Gyro;
#else
adapters::NullGyro nullGyro;
ports::IGyro *gyro = &nullGyro;
#endif

// --- persistence -----------------------------------------------------------------------

#if USE_DUMMY_EEPROM == true
// A development switch: boot without touching the board's EEPROM.
hal::InMemoryEeprom<hal::eeprom::STORE_SIZE> eeprom;
#else
hal::ArduinoEeprom eeprom(EEPROM);
#endif

adapters::EepromPersistentStore persistentStore(eeprom, hal::eeprom::STORE_SIZE);

// The on-device format has a single writer; the settings port is the typed view the domain
// consumes.
namespace
{
/// The uncalibrated values a blank store answers with. Board facts, so they are read once here; an
/// axis the board does not have keeps the "no calibration" answer instead of a made-up one.
adapters::EepromSettingsFormat::AxisDefaults makeSettingsDefaults()
{
    adapters::EepromSettingsFormat::AxisDefaults defaults = {
        RA_STEPS_PER_DEGREE,
        DEC_STEPS_PER_DEGREE,
        0.0f,  // AZ, filled in below when the board has one
        0.0f,  // ALT
        RA_SLEW_MICROSTEPPING,
        DEC_SLEW_MICROSTEPPING,
        1,  // AZ microsteps: a divisor, so never zero
        1,  // ALT microsteps
        static_cast<int16_t>(BACKLASH_STEPS),
    };
#if AZ_STEPPER_TYPE != STEPPER_TYPE_NONE
    defaults.azStepsPerDegree = AZIMUTH_STEPS_PER_REV / 360.0f;
    defaults.azMicrosteps     = AZ_MICROSTEPPING;
#endif
#if ALT_STEPPER_TYPE != STEPPER_TYPE_NONE
    defaults.altStepsPerDegree = ALTITUDE_STEPS_PER_REV / 360.0f;
    defaults.altMicrosteps     = ALT_MICROSTEPPING;
#endif
    return defaults;
}
}  // namespace

const adapters::EepromSettingsFormat::AxisDefaults settingsDefaults = makeSettingsDefaults();
adapters::EepromSettingsFormat eepromStore(persistentStore, *logSink, settingsDefaults);

// --- the menu display and its keys -----------------------------------------------------
//
// One `#if` selects the panel family; everything above it sees `ports::IDisplay` and
// `ports::IButtons`, and the libraries stay in `hal/arduino/`. The panel is a board fact that
// nothing discovers at run time, so linking all four drivers to make it a runtime choice would
// only cost flash.

constexpr byte kMenuColumns = 16;
constexpr byte kMenuRows    = 2;

#if DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD
    #if defined(LCD_BRIGHTNESS_PIN)
// The backlight is a PWM pin on the shield. The known-bad boards short it and can only switch it.
hal::ArduinoGpioPin lcdBacklightPin(LCD_BRIGHTNESS_PIN);
const bool lcdBacklightShorted = hal::backlightPinIsShorted(LCD_BRIGHTNESS_PIN);
hal::CharLcd lcdPanel(LCD_PIN8, LCD_PIN9, LCD_PIN4, LCD_PIN5, LCD_PIN6, LCD_PIN7, &lcdBacklightPin, lcdBacklightShorted);
const adapters::LcdMenuDisplay::BacklightRange lcdBacklightRange = {0, static_cast<uint8_t>(lcdBacklightShorted ? 1 : 255)};
    #else
hal::CharLcd lcdPanel(LCD_PIN8, LCD_PIN9, LCD_PIN4, LCD_PIN5, LCD_PIN6, LCD_PIN7);
const adapters::LcdMenuDisplay::BacklightRange lcdBacklightRange = {0, 255};
    #endif
hal::ArduinoGpioPin lcdKeySensePin(LCD_KEY_SENSE_PIN);
hal::AnalogKeyPad lcdKeyPad(lcdKeySensePin);

#elif DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23017 || DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23008
hal::I2cCharLcd lcdPanel(0x20, DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23017);
const adapters::LcdMenuDisplay::BacklightRange lcdBacklightRange = {0, 255};
hal::I2cExpanderButtons lcdKeyPad(lcdPanel);

#elif DISPLAY_TYPE == DISPLAY_TYPE_LCD_JOY_I2C_SSD1306
hal::U8x8CharLcd lcdPanel;
const adapters::LcdMenuDisplay::BacklightRange lcdBacklightRange = {0, 255};
hal::ArduinoGpioPin lcdJoyXPin(LCD_KEY_SENSE_X_PIN);
hal::ArduinoGpioPin lcdJoyYPin(LCD_KEY_SENSE_Y_PIN);
hal::ArduinoGpioPin lcdJoyPushPin(LCD_KEY_SENSE_PUSH_PIN);
hal::JoystickButtons lcdKeyPad(lcdJoyXPin, lcdJoyYPin, lcdJoyPushPin);
#endif

#if DISPLAY_TYPE > 0

adapters::LcdMenuDisplay menuDisplay(lcdPanel, &eepromStore, kMenuColumns, kMenuRows, lcdBacklightRange);

/// The menu's item table. Caller-owned so the menu itself never allocates.
core::MenuEntry menuEntries[MAXMENUITEMS];
core::MenuModel menuModel(menuEntries, MAXMENUITEMS);
core::MenuModel *menuModelPort = &menuModel;

adapters::DebouncedButtons lcdButtonSource(lcdKeyPad, clockPort);
ports::IButtons *menuButtons = &lcdButtonSource;

/// The display as the screens see it: the panel the menu row is painted on.
ports::IDisplay *menuDisplayPort = &menuDisplay;

#else  // Headless: no panel, no model, no keys, so a board without a display pays nothing.

core::MenuModel *menuModelPort   = nullptr;
ports::IButtons *menuButtons     = nullptr;
ports::IDisplay *menuDisplayPort = nullptr;

#endif

// Driver enable lines. Only declared where the executor drives them: the RA/DEC writes
// sit inside the UART self-test, and AZ/ALT/Focus exist only when the axis does.
#if (RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART) && (UART_CONNECTION_TEST_TXRX == 1)
hal::ArduinoGpioPin raEnablePin(RA_EN_PIN);
#endif
#if (DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART) && (UART_CONNECTION_TEST_TXRX == 1)
hal::ArduinoGpioPin decEnablePin(DEC_EN_PIN);
#endif
#if AZ_STEPPER_TYPE != STEPPER_TYPE_NONE
hal::ArduinoGpioPin azEnablePin(AZ_EN_PIN);
#endif
#if ALT_STEPPER_TYPE != STEPPER_TYPE_NONE
hal::ArduinoGpioPin altEnablePin(ALT_EN_PIN);
#endif
#if FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE
hal::ArduinoGpioPin focusEnablePin(FOCUS_EN_PIN);
#endif

namespace
{
app::MountDeps makeMountDeps()
{
    app::MountDeps deps;
    deps.clock     = &clockPort;
    deps.scheduler = &schedulerPort;
    deps.logger    = logSink;
    deps.settings  = &eepromStore;
#if (RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART) && (UART_CONNECTION_TEST_TXRX == 1)
    deps.raEnablePin = &raEnablePin;
#endif
#if (DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART) && (UART_CONNECTION_TEST_TXRX == 1)
    deps.decEnablePin = &decEnablePin;
#endif
#if AZ_STEPPER_TYPE != STEPPER_TYPE_NONE
    deps.azEnablePin = &azEnablePin;
#endif
#if ALT_STEPPER_TYPE != STEPPER_TYPE_NONE
    deps.altEnablePin = &altEnablePin;
#endif
#if FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE
    deps.focusEnablePin = &focusEnablePin;
#endif
    return deps;
}
}  // namespace

// The controllers, built before the mount that drives them and shared through
// `app::mountCoordinator` where a consumer only needs the controller half.
#ifdef ESP32
DRAM_ATTR app::MountExecutor::Coordinator mountCoordinator(&clockPort,
                                                           SIDEREAL_SECONDS_PER_DAY,
                                                           app::buildMountConfig().trackingMicrostepRatio());
#else
app::MountExecutor::Coordinator mountCoordinator(&clockPort, SIDEREAL_SECONDS_PER_DAY, app::buildMountConfig().trackingMicrostepRatio());
#endif

#ifdef ESP32
DRAM_ATTR app::MountExecutor mount(makeMountDeps(), app::buildMountConfig(), mountCoordinator);
#else
app::MountExecutor mount(makeMountDeps(), app::buildMountConfig(), mountCoordinator);
#endif

// The optional status panel is owned here, not by the mount: the mount publishes events and the
// loop ticks the panel (Phase 6). A board without the panel compiles none of this and the helpers
// below are empty.
#if (INFO_DISPLAY_TYPE != INFO_DISPLAY_TYPE_NONE)
    #if (INFO_DISPLAY_TYPE == INFO_DISPLAY_TYPE_I2C_SSD1306_128x64)
/// The 128x64 status panel: the library-facing backend, and the adapter that draws the frames.
hal::Ssd1306Panel infoPanel(INFO_DISPLAY_I2C_ADDRESS, INFO_DISPLAY_I2C_SDA_PIN, INFO_DISPLAY_I2C_SCL_PIN);
adapters::Ssd1306InfoDisplay ssd1306InfoDisplay(infoPanel, clockPort);
adapters::Ssd1306InfoDisplay *infoDisplay = &ssd1306InfoDisplay;
    #endif
adapters::InfoDisplayPanel *infoDisplayPanel = nullptr;
#endif

namespace
{
/// The latitude observer exists so a site change can still reconfigure the hemisphere, which is
/// motion work the mount owns.
void onLatitudeChanged(void *context, bool northernHemisphere)
{
    static_cast<app::MountExecutor *>(context)->configureHemisphere(northernHemisphere);
}
}  // namespace

#ifdef ESP32
DRAM_ATTR core::SiteTimeService siteTimePort(mount.siteTime(), clockPort, eepromStore, {&mount, &onLatitudeChanged},
                                     {RA_ZERO_POINT_SHIFT_HOURS, POLARIS_RA_HOUR, POLARIS_RA_MINUTE, POLARIS_RA_SECOND});
#else
core::SiteTimeService siteTimePort(mount.siteTime(), clockPort, eepromStore, {&mount, &onLatitudeChanged},
                                     {RA_ZERO_POINT_SHIFT_HOURS, POLARIS_RA_HOUR, POLARIS_RA_MINUTE, POLARIS_RA_SECOND});
#endif

#if defined(ESP32) || !defined(NEW_STEPPER_LIB)
    // The bank starts empty and `app::attachSteppers()` fills it in `setup()`; the pump holds a
    // reference, so it sees the axes as soon as they exist. It is a static rather than a local
    // because the timer ISR needs a fixed address.
    #ifdef ESP32
DRAM_ATTR app::StepPump stepPump(mountCoordinator.state(), mountCoordinator.focus(), mount.axes());
    #else
app::StepPump stepPump(mountCoordinator.state(), mountCoordinator.focus(), mount.axes());
    #endif
#endif

app::UiPorts ui;

/// The session flag the command layer sets and the menu reads. Zero-initialised like `ui`.
app::UiSession uiSession;

// The menu screens' view of the mount: five ports, the clock and the scheduler, all backed by the
// one `mount` object until the coordinator owns the controllers.
struct UiPortsInit {
    UiPortsInit()
    {
        ui.motion    = &mount;
        ui.config    = &mount;
        ui.read      = &mount.readout();
        ui.settings  = &eepromStore;
        ui.siteTime  = &siteTimePort;
        ui.axes      = &mount;
        ui.clock     = &clockPort;
        ui.scheduler = &schedulerPort;
        ui.loop      = []() { mount.loop(); };
    }
} uiPortsInit;

#if (WIFI_ENABLED == 1)
hal::Esp32WifiStack wifiStack;
WifiControl wifiControl(wifiStack, []() { mount.loop(); });
#endif

/////////////////////////////////
//   Interrupt handling
/////////////////////////////////
/* There are Two possible configurations for periodically servicing the stepper drives:
 * 1) If ESP32 is #defined then a periodic task is assigned to Core 0 to service the steppers.
 *    stepperControlTask() is scheduled to run every 1 ms (1 kHz rate). On ESP32 the default Arduino
 *    loop() function runs on Core 1, therefore serial and UI activity also runs on Core 1.
 *    Note that Wifi drivers will be sharing Core 0 with stepperControlTask().
 *    This configuration decouples stepper servicing from other OAT activities by using both cores.
 * 2) By default (e.g. for ATmega2560) a periodic timer is configured for a 500 us (2 kHz rate interval).
 *    This timr generates interrupts which are handled by stepperControlCallback(). The stepper
 *    servicing therefore suspends loop() to generate motion, ensuring smooth tracking.
 */
#if defined(ESP32)

TaskHandle_t StepperTask;

// This is the task for simulating periodic interrupts on ESP32 platforms.
// It should do very minimal work, only running app::StepPump::pump() to step the motors as needed.
// This task function is run on Core 0 of the ESP32 and never returns
void IRAM_ATTR stepperControlTask(void *payload)
{
    app::StepPump *pump = reinterpret_cast<app::StepPump *>(payload);
    for (;;)
    {
        pump->pump();
    }
}

#else
    #ifndef NEW_STEPPER_LIB

// This is the callback function for the timer interrupt on the platforms that need one.
// It should do very minimal work, only running app::StepPump::pump() to step the motors as needed.
// It is called every STEPPER_CONTROL_INTERVAL_MS.
void stepperControlTimerCallback(void *payload)
{
    app::StepPump *pump = reinterpret_cast<app::StepPump *>(payload);
    if (pump)
        pump->pump();
}
    #endif
#endif

bool connectToDriver(const char *driverKind, uint16_t *rmsCurrent)
{
    // Only a UART driver has registers to talk to, and the executor does not declare the call on a
    // board without one. The bring-up screen asks unconditionally, so a board with no UART
    // driver answers "not connected" rather than failing to compile.
#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART || DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART                                              \
    || AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART || ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART                                           \
    || FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    return mount.connectToDriver(String(driverKind), rmsCurrent);
#else
    (void) driverKind;
    (void) rmsCurrent;
    return false;
#endif
}

void initPersistentStore()
{
    persistentStore.begin();
    eepromStore.initialize();
}

#if (INFO_DISPLAY_TYPE != INFO_DISPLAY_TYPE_NONE)
int addConsoleText(String text)
{
    return (infoDisplay != nullptr) ? infoDisplay->addConsoleText(text) : -1;
}

void updateConsoleText(int line, String newText)
{
    if (infoDisplay != nullptr)
    {
        infoDisplay->updateConsoleText(line, newText);
    }
}

void serviceInfoDisplay()
{
    if (infoDisplayPanel != nullptr)
    {
        infoDisplayPanel->tick();
    }
}
#endif
