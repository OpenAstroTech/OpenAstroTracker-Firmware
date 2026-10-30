#pragma once

// The composition root's object graph.
//
// One definition of every singleton the firmware runs on, in one translation unit, so
// construction order is the order written here rather than an accident of include order.
// Nothing below `app/` includes this header: the domain sees ports, the UI sees the objects
// the root hands it.
//
// This is the extracted half of the old `b_setup.hpp`; `setup()` lives in `main.cpp` now.

#include "MountExecutor.hpp"
#include "LoopState.hpp"
#include "core/menu/MenuModel.hpp"
#include "ports/IButtons.hpp"
#include "ports/IDisplay.hpp"

#include "adapters/ArduinoClock.hpp"
#include "adapters/EepromSettingsFormat.hpp"
#include "adapters/ArduinoScheduler.hpp"
#include "adapters/InfoDisplayPanel.hpp"
#include "core/SiteTimeService.hpp"
#include "UiPorts.hpp"
#include "UiSession.hpp"
#include "StepPump.hpp"
#include "adapters/NullGyro.hpp"
#include "ports/IGps.hpp"
#include "ports/ILogRetention.hpp"
#include "ports/IGyro.hpp"
#include "hal/arduino/ArduinoSystemClock.hpp"
#include "hal/arduino/ArduinoGpioPin.hpp"
#include "hal/arduino/ArduinoSerialPort.hpp"

#if (WIFI_ENABLED == 1)
    #include "adapters/WifiControl.hpp"
#endif

// --- the display and menu singletons ---------------------------------------------------

/// The menu model, or null on a board with no panel.
extern core::MenuModel *menuModelPort;

/// The debounced keys, or null on a board with no panel.
extern ports::IButtons *menuButtons;

/// The menu display, which is also what the screens paint through (null on a headless board).
extern ports::IDisplay *menuDisplayPort;

/// The retained log `:D#` dumps, or null when the build logs nothing.
extern ports::ILogRetention *const logRetention;

// --- the infrastructure the domain runs on --------------------------------------------

extern hal::ArduinoSystemClock arduinoSystemClock;
extern adapters::ArduinoClock clockPort;
extern adapters::ArduinoScheduler schedulerPort;

// --- the optional levelling sensor ------------------------------------------------------
//
// Null when the board has none: the menus and the command layer ask it, and get a working
// "not available" answer instead of a compile-time branch.

extern ports::IGyro *gyro;

/// The GPS receiver, or null on a board with no receiver wired. The HA screen is the only consumer;
/// a null receiver turns it into the manual hour-angle editor.
extern ports::IGps *gpsPort;

// --- persistence -----------------------------------------------------------------------

/// The on-device settings format, and the `ports::IMountSettings` the domain consumes: the
/// class implements that port directly, so there is no separate facade object.
extern adapters::EepromSettingsFormat eepromStore;

/// The one log sink, so the wiring modules can log without owning a logger.

// --- the mount and everything bound to it ---------------------------------------------

#ifdef ESP32
extern DRAM_ATTR app::MountExecutor mount;
#else
extern app::MountExecutor mount;
#endif

/// The site/time port the menus and the command layer use. Reaching the mount only for its
/// `SiteTime` and its settings store is what keeps those consumers off the facade.
#ifdef ESP32
extern DRAM_ATTR core::SiteTimeService siteTimePort;
#else
extern core::SiteTimeService siteTimePort;
#endif

/// The controllers the mount drives, owned here so the rest of the firmware can reach the
/// controller half without going through `mount`.
#ifdef ESP32
extern DRAM_ATTR app::MountExecutor::Coordinator mountCoordinator;
#else
extern app::MountExecutor::Coordinator mountCoordinator;
#endif

#if defined(ESP32) || !defined(NEW_STEPPER_LIB)
    /// The step pump the timer ISR (or the ESP32 task) runs. Only exists on boards without a
    /// driver-side step generator. It reads the axes through `mount.axes()`, so it needs no copy of
    /// them here.
    #ifdef ESP32
extern DRAM_ATTR app::StepPump stepPump;
    #else
extern app::StepPump stepPump;
    #endif
#endif

#if (WIFI_ENABLED == 1)
extern WifiControl wifiControl;
#endif

// --- the optional status panel ---------------------------------------------------------

#if (INFO_DISPLAY_TYPE != INFO_DISPLAY_TYPE_NONE)
    #if (INFO_DISPLAY_TYPE == INFO_DISPLAY_TYPE_I2C_SSD1306_128x64)
namespace adapters
{
class Ssd1306InfoDisplay;
}
extern adapters::Ssd1306InfoDisplay *infoDisplay;
    #endif
extern adapters::InfoDisplayPanel *infoDisplayPanel;

/// Appends a line to the panel's console. Returns the line index, or -1 on a board
/// without a panel, so callers need no `#if`.
int addConsoleText(String text);

/// Rewrites one console line in place. A no-op on a board without a panel.
void updateConsoleText(int line, String newText);

/// Gives the status panel its turn. A no-op on a board without one, so the main loop
/// calls it unconditionally rather than carrying an `#if` for a peripheral.
void serviceInfoDisplay();
#else
inline int addConsoleText(String)
{
    return -1;
}

inline void updateConsoleText(int, String)
{
}

inline void serviceInfoDisplay()
{
}
#endif

/// Runs the driver's UART self-test, as the TEST_VERIFY_MODE screen does. A bring-up
/// diagnostic: it lives with the mount's hardware wiring, not on a domain port.
bool connectToDriver(const char *driverKind, uint16_t *rmsCurrent);

/// Wires and initialises the EEPROM facade. The facade is a static class the menus and the
/// command layer still call, so the root hands it the store and the logger here instead of
/// letting each call site reach for the singletons.
void initPersistentStore();

// --- the step generator ----------------------------------------------------------------

#if defined(ESP32)
extern TaskHandle_t StepperTask;

/// FreeRTOS task on core 0 that runs `app::StepPump::pump()` in a tight loop.
void IRAM_ATTR stepperControlTask(void *payload);
#else
    #ifndef NEW_STEPPER_LIB

/// Timer ISR callback (5 kHz RP2040) that runs `app::StepPump::pump()`.
void stepperControlTimerCallback(void *payload);
    #endif
#endif
