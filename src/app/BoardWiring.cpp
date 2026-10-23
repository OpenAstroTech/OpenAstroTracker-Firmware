// The composition root: the only place that picks concrete backends and hands them to the
// domain as ports. Everything the firmware constructs, constructed once, here.
//
// Definition order is construction order, so the infrastructure is declared before the mount
// that depends on it.

#include "../../Configuration.hpp"
#include "BoardWiring.hpp"

#include "EPROMStore.hpp"
#include "Utility.hpp"
#include "app/MountConfigBuilder.hpp"
#include "hal/InMemoryEeprom.hpp"
#include "hal/WarningGuard.hpp"
#include "hal/arduino/ArduinoEeprom.hpp"

#include "adapters/EepromPersistentStore.hpp"
#include "adapters/MountSettingsStore.hpp"
#include "adapters/NullLogger.hpp"
#include "adapters/SerialLogger.hpp"
#include "adapters/Mpu6050Gyro.hpp"
#include "hal/arduino/ArduinoI2cBus.hpp"

PUSH_NO_WARNINGS
#include <EEPROM.h>
POP_NO_WARNINGS

#if (INFO_DISPLAY_TYPE == INFO_DISPLAY_TYPE_I2C_SSD1306_128x64)
    #include "SSD1306_128x64_Display.hpp"
#endif

// --- UI state, declared in `a_inits.hpp` -----------------------------------------------

#if USE_GPS == 1
TinyGPSPlus gps;
#endif

#if SUPPORT_GUIDED_STARTUP == 1
bool inStartup = true;  // Start with a guided startup
#else
bool inStartup = false;  // Start with a guided startup
#endif

bool okToUpdateMenu                = true;   // Can be used to supress rendering the first line of the menu.
bool quitSerialOnNextButtonRelease = false;  // Used to detect SELECT button to quit Serial mode.

int RAselect;
int DECselect;
int HAselect;

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
#else
adapters::NullLogger loggerPort;
ports::ILogger *const logSink = &loggerPort;
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

// `EEPROMStore` is the single writer of the on-device format; the settings port is the typed
// view the domain consumes.
EEPROMStore eepromStore(persistentStore, *logSink);
adapters::MountSettingsStore settingsStore(eepromStore);

// --- the display and menu singletons ---------------------------------------------------
//
// After the settings store: the menu asks it for the persisted backlight level.

LcdMenu lcdMenu(16, 2, MAXMENUITEMS, &settingsStore);
#if DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD
LcdButtons lcdButtons(LCD_KEY_SENSE_PIN, &lcdMenu);
#endif

#if DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23017 || DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23008                           \
    || DISPLAY_TYPE == DISPLAY_TYPE_LCD_JOY_I2C_SSD1306
LcdButtons lcdButtons(&lcdMenu);
#endif

// Driver enable lines. Only declared where `Mount` drives them: the RA/DEC writes
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
MountDeps makeMountDeps()
{
    MountDeps deps;
    deps.clock     = &clockPort;
    deps.scheduler = &schedulerPort;
    deps.logger    = logSink;
    deps.settings  = &settingsStore;
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

#ifdef ESP32
DRAM_ATTR Mount mount(&lcdMenu, makeMountDeps(), app::buildMountConfig());
#else
Mount mount(&lcdMenu, makeMountDeps(), app::buildMountConfig());
#endif

// The optional status panel is owned here, not by the mount: the mount publishes events and the
// loop ticks the panel (Phase 6). A board without the panel compiles none of this and the helpers
// below are empty.
#if (INFO_DISPLAY_TYPE != INFO_DISPLAY_TYPE_NONE)
InfoDisplayRender *infoDisplay               = nullptr;
adapters::InfoDisplayPanel *infoDisplayPanel = nullptr;
#endif

#if (WIFI_ENABLED == 1)
WifiControl wifiControl(&mount, &lcdMenu);
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
// It should do very minimal work, only calling Mount::interruptLoop() to step the stepper motors as needed.
// This task function is run on Core 0 of the ESP32 and never returns
void IRAM_ATTR stepperControlTask(void *payload)
{
    Mount *mountCopy = reinterpret_cast<Mount *>(payload);
    for (;;)
    {
        mountCopy->interruptLoop();
    }
}

#else
    #ifndef NEW_STEPPER_LIB

// This is the callback function for the timer interrupt on ATMega platforms.
// It should do very minimal work, only calling Mount::interruptLoop() to step the stepper motors as needed.
// It is called every 500 us (2 kHz rate)
void stepperControlTimerCallback(void *payload)
{
    Mount *mountCopy = reinterpret_cast<Mount *>(payload);
    if (mountCopy)
        mountCopy->interruptLoop();
}
    #endif
#endif

void initPersistentStore()
{
    persistentStore.begin();
    eepromStore.initialize();
}

#if (INFO_DISPLAY_TYPE != INFO_DISPLAY_TYPE_NONE)
int addConsoleText(String text)
{
    return (infoDisplay != nullptr) ? infoDisplay->addConsoleText(text, false) : -1;
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
