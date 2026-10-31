// The boot sequence. `main.cpp` calls `app::bootstrap()` from `setup()` and nothing else; every
// step below is one function, in the order the firmware has always run them, so the sketch no
// longer carries the board conditionals that decide what to bring up.

#include "../../Configuration.hpp"

#include "AppBootstrap.hpp"
#include "BoardWiring.hpp"
#include "DriverWiring.hpp"
#include "MenuSurface.hpp"
#include "MotorWiring.hpp"
#include "MountExecutor.hpp"
#include "SensorWiring.hpp"
#include "UiScreens.hpp"
#include "adapters/MeadeCommandAdapter.hpp"
#include "ports/ILogger.hpp"

#if (INFO_DISPLAY_TYPE == INFO_DISPLAY_TYPE_I2C_SSD1306_128x64)
    #include "adapters/Ssd1306InfoDisplay.hpp"
#endif

#if defined(OAT_DEBUG_BUILD)
    #if BOARD < 1000
        #include "avr8-stub.h"
    #else
        #error "Debugging not supported on this platform"
    #endif
#endif

#if TEST_VERIFY_MODE == 1
    #include "test/TestMenu.hpp"
#endif

#ifndef NEW_STEPPER_LIB
    #include "../hal/InterruptCallback.hpp"
#endif

namespace
{

/// A debug build hands the early boot to avr-stub instead of the serial port, so the two are one
/// decision: `startSerial()` is compiled out for exactly the builds that run `startDebug()`.
void startDebug()
{
#if defined(OAT_DEBUG_BUILD)
    #if BOARD < 1000
    debug_init();  // Setup avr-stub
    breakpoint();  // Set a breakpoint as soon as possible
    #else
        #error "Debugging not supported on this platform"
    #endif
#endif
}

void startSerial()
{
#if !defined(OAT_DEBUG_BUILD)
    Serial.begin(SERIAL_BAUDRATE);
    #if defined(ARDUINO_ARCH_RP2040)
    // OATControl (and other apps using C# SerialPort) open the port with DTR=false.
    // The Philhower USB CDC SerialUSB::write() gates TX on tud_cdc_connected() which
    // requires DTR=true, so all responses are silently dropped for those hosts.
    // ignoreFlowControl(true) bypasses the DTR check so replies always flow.
    // Bounded by the core: with no (or a non-DTR) host, writes stop as soon as the
    // ~257-byte CDC TX FIFO is full (tud_cdc_connected() false breaks the write
    // loop immediately); a connected host that stops draining is dropped after
    // 1 second without progress. Headless boots never stall on logging.
    Serial.ignoreFlowControl(true);
    #endif
    #if DEBUG_LEVEL != 0 && DEBUG_SEPARATE_SERIAL == 1
    DEBUG_SERIAL_PORT.begin(DEBUG_SERIAL_BAUDRATE);
    #endif
    // RP2040 USB CDC: give a console up to 3 seconds to attach so the boot
    // log stays visible. Bounded, never infinite: a mount powered externally
    // in the field must boot with no USB host attached. With no host attached
    // the CDC TX FIFO holds only ~250 bytes and further writes are dropped, so
    // a headless debug build simply boots invisibly (reset with the console
    // open to see the full log). Release builds (DEBUG_LEVEL == 0) skip this
    // entirely.
    #if defined(ARDUINO_ARCH_RP2040) && DEBUG_LEVEL != 0
    uint32_t consoleWaitStart = millis();
    while (!Serial && (millis() - consoleWaitStart < 3000UL))
    { /* spin until the console connects or the timeout expires */
    }
    #endif
#endif
}

void logBootBanner()
{
#if TEST_VERIFY_MODE == 1
    #ifdef OAM
    Serial.print(F("Booting OAM Firmware "));
    #elif defined(OAE)
    Serial.print(F("Booting OAE Firmware "));
    #else
    Serial.print(F("Booting OAT Firmware "));
    #endif
    Serial.print(VERSION);
    Serial.println(F(" ..."));
#else
    #ifdef OAM
    LOG(DEBUG_ANY, "[SYSTEM]: Hello, universe, this is OAM Firmware %s!", VERSION);
    #else
    LOG(DEBUG_ANY, "[SYSTEM]: Hello, universe, this is OAT Firmware %s!", VERSION);
    #endif
#endif
}

void startInfoDisplay()
{
#if (INFO_DISPLAY_TYPE != INFO_DISPLAY_TYPE_NONE)
    LOG(DEBUG_ANY, "[SYSTEM]: Get OLED info screen ready...");
    #if (INFO_DISPLAY_TYPE == INFO_DISPLAY_TYPE_I2C_SSD1306_128x64)
    LOG(DEBUG_ANY, "[SYSTEM]: SSD1306 OLED created... initializing");
    infoDisplay->begin();
    infoDisplay->setConsoleMode(true);
    LOG(DEBUG_DISPLAY, "[DISPLAY]: Created and initialized SSD1306 OLED class...");
    #endif
    if (infoDisplay != nullptr)
    {
        infoDisplayPanel = new adapters::InfoDisplayPanel(mount.readout(), *infoDisplay, clockPort, mountCoordinator);
        infoDisplayPanel->begin();
        addConsoleText(F("BOOTING " VERSION));
    }
    LOG(DEBUG_ANY, "[SYSTEM]: OLED info screen ready!");
#endif
}

void startGps()
{
#if USE_GPS == 1
    LOG(DEBUG_ANY, "[SYSTEM]: Initializing GPS...");
    // `addConsoleText` is the panel's null helper: it returns -1 on a board without one, so the
    // two lines need no `#if` of their own.
    int gpsLine = addConsoleText(F("Initialize GPS..."));
    #if defined(ARDUINO_ARCH_RP2040) && defined(RP2040_GPS_TX_PIN) && defined(RP2040_GPS_RX_PIN)
    GPS_SERIAL_PORT.setTX(RP2040_GPS_TX_PIN);
    GPS_SERIAL_PORT.setRX(RP2040_GPS_RX_PIN);
    #endif
    GPS_SERIAL_PORT.begin(GPS_BAUD_RATE);
    updateConsoleText(gpsLine, F("Initialize GPS... OK"));
#endif
}

void startDewHeater()
{
#if DEW_HEATER == 1
    LOG(DEBUG_ANY, "[SYSTEM]: Initializing dew heater...");
    int dewHeaterLine = addConsoleText(F("Enable Dew Heater..."));
    #if defined(DEW_HEATER_1_PIN)
    digitalWrite(DEW_HEATER_1_PIN, HIGH);
    #endif
    #if defined(DEW_HEATER_2_PIN)
    digitalWrite(DEW_HEATER_2_PIN, HIGH);
    #endif
    updateConsoleText(dewHeaterLine, F("Enable Dew Heater... OK"));
#endif
}

void startFan()
{
#if defined(RP2040_FAN1_PIN)
    pinMode(RP2040_FAN1_PIN, OUTPUT);
    analogWrite(RP2040_FAN1_PIN, (RP2040_FAN1_POWER_PERCENT * 255) / 100);
#endif
}

/// End switches and hall sensors are built by the composition root; a board may have either,
/// both or neither, so the call itself is unconditional.
void startSensors()
{
    if (app::kHasEndSwitch)
    {
        int endSwitchesLine = addConsoleText(F("Init End Switches..."));
        LOG(DEBUG_ANY, "[SYSTEM]: Init EndSwitches...");
        updateConsoleText(endSwitchesLine, F("Init End Switches... OK"));
    }

    app::attachSensors(mount);
}

/// RP2040_UART1_TX_PIN / RP2040_UART1_RX_PIN are defined in the board's pins file only for RP2040
/// targets that use a non-default UART1 mapping.
void remapUartPins()
{
#if defined(ARDUINO_ARCH_RP2040) && defined(RP2040_UART1_TX_PIN) && defined(RP2040_UART1_RX_PIN)
    Serial2.setTX(RP2040_UART1_TX_PIN);
    Serial2.setRX(RP2040_UART1_RX_PIN);
    Serial2.setPollingMode(true);  // Must be called before begin(); avoids IRQ/CoreMutex blocking in TMCStepper read path
    Serial2.begin(115200);         // TMCStepper HW-serial path never calls begin(); must do it here.
#endif
}

/// The microstepping/driver pins: every axis this board has, in the order the firmware brings
/// them up.
void startDriverPins()
{
    int raLine = addConsoleText(F("Init RA axis..."));
    app::initDriverPins(app::DriverAxis::RightAscension);
    updateConsoleText(raLine, F("Init RA axis... OK"));

    int decLine = addConsoleText(F("Init DEC axis..."));
    LOG(DEBUG_ANY, "[SYSTEM]: Initializing DEC driver pin %s...", String(DEC_EN_PIN).c_str());
    app::initDriverPins(app::DriverAxis::Declination);
    updateConsoleText(decLine, F("Init DEC axis... OK"));
    LOG(DEBUG_ANY, "[SYSTEM]: RA/DEC init complete...");

    if (app::kHasAzStepper)
    {
        int azLine = addConsoleText(F("Init AZ axis..."));
        app::initDriverPins(app::DriverAxis::Azimuth);
        updateConsoleText(azLine, F("Init AZ axis... OK"));
    }

    if (app::kHasAltStepper)
    {
        int altLine = addConsoleText(F("Init ALT axis..."));
        app::initDriverPins(app::DriverAxis::Altitude);
        updateConsoleText(altLine, F("Init ALT axis... OK"));
    }

    if (app::kHasFocusStepper)
    {
        int focusLine = addConsoleText(F("Init Focuser..."));
        app::initDriverPins(app::DriverAxis::Focuser);
        updateConsoleText(focusLine, F("Init Focuser... OK"));
    }
}

/// On boards where all stepper EN lines share one GPIO (e.g. JackW01 carrier), the per-axis init
/// sequence leaves that GPIO HIGH (logic disabled) after the AZ/ALT blocks run. Force it LOW here so
/// every driver is enabled once all UART addresses have been configured. Per-axis power management
/// is handled via the TMC2209 IHOLD register over UART rather than toggling this pin.
void enableSharedDrivers()
{
#if defined(RP2040_SHARED_EN_PIN)
    pinMode(RP2040_SHARED_EN_PIN, OUTPUT);
    digitalWrite(RP2040_SHARED_EN_PIN, LOW);
#endif
}

void startHomingSensors()
{
    if (app::kHasHomingSensor)
    {
        int homingLine = addConsoleText(F("Init homing sensors..."));
        app::initHomingSensorPins();
        updateConsoleText(homingLine, F("Init homing sensors... OK"));
    }
}

void startPersistentStore()
{
    LOG(DEBUG_ANY, "[SYSTEM]: Initializing EEPROM store...");
    int eepromLine = addConsoleText(F("INIT EEPROM..."));

    initPersistentStore();
    LOG(DEBUG_ANY, "[SYSTEM]: EEPROM store ready!");
    updateConsoleText(eepromLine, F("INIT EEPROM... OK"));
}

/// Calling the LCD startup here: I2C can't be found if it runs earlier.
void startMenu()
{
    if (app::kHasMenu)
    {
        LOG(DEBUG_ANY, "[SYSTEM]: Get LCD ready...");
        int lcdLine = addConsoleText(F("Init LCD..."));
        menu.startup();

        menu.showSplash();

        // Check for EEPROM reset (button down during boot). The surface paints the request;
        // clearing the store is the mount's, so it stays here.
        if (menu.awaitFactoryResetRequest())
        {
            LOG(DEBUG_INFO, "[SYSTEM]: Erasing configuration in EEPROM!");
            mount.clearConfiguration();
            LOG(DEBUG_INFO, "[SYSTEM]: Button released, continuing");
        }

        app::registerMenuItems(menu);

        updateConsoleText(lcdLine, F("Init LCD... OK"));
    }
}

void logHardwareInfo()
{
    LOG(DEBUG_ANY, "[SYSTEM]: Hardware: %s", mount.getMountHardwareInfo());
}

void startCommandProcessor()
{
    LOG(DEBUG_ANY, "[SYSTEM]: Initialize LX200 handler...");
    int commandProcessorLine = addConsoleText(F("Init MEADE handler..."));
    MeadeCommandPorts commandPorts;
    commandPorts.configuration        = &mount;
    commandPorts.axes                 = &mount;
    commandPorts.readModel            = &mount.readout();
    commandPorts.siteTime             = &siteTimePort;
    commandPorts.events               = &mountCoordinator;
    commandPorts.clock                = &clockPort;
    commandPorts.gyro                 = gyro;
    commandPorts.gps                  = gpsPort;
    commandPorts.logRetention         = logRetention;
    commandPorts.serialControlChanged = [](bool active, void *) { uiSession.serialControl = active; };
    MeadeCommandAdapter::createProcessor(commandPorts, menuDisplayPort);
    updateConsoleText(commandProcessorLine, F("Init MEADE handler... OK"));
}

void startWifi()
{
#if (WIFI_ENABLED == 1)
    LOG(DEBUG_ANY, "[SYSTEM]: Setup Wifi...");
    wifiControl.setup();
#endif
}

void startSteppers()
{
    int stepperLine = addConsoleText(F("Energize Steppers..."));

    // Configure the mount
    // Delay for a while to get UARTs booted...
    delay(1000);

    LOG(DEBUG_ANY, "[SYSTEM]: Configure steppers...");

    // Every axis engine is built by the composition root's wiring module; see app/MotorWiring.cpp.
    app::attachSteppers(mount);

    // The TMC2209 UART drivers, and their register programming, are board knowledge: the
    // composition root's driver wiring builds them; see app/DriverWiring.cpp.
    app::attachDrivers(mount);

#if defined(RP2040_SHARED_EN_PIN)
    // Re-assert EN LOW after all driver configs.  Each configureXXXdriver() that
    // fails its UART connection test calls digitalWrite(X_EN_PIN, HIGH) as a
    // safety measure; on JackW01 all EN lines share one GPIO so all four failures
    // leave that GPIO HIGH (motors disabled).  Override it here unconditionally so
    // the motors are always enabled after initialization.
    digitalWrite(RP2040_SHARED_EN_PIN, LOW);
#endif

    LOG(DEBUG_ANY, "[SYSTEM]: Energize Steppers... OK");
    updateConsoleText(stepperLine, F("Energize Steppers... OK"));
}

/// Reads the persisted settings and seeds the target from where the mount is. Returns the console
/// line the caller finishes once the stepper service is running, so the panel keeps showing
/// "Configure Mount..." while the timer/task is set up.
int loadConfiguration()
{
    int configureLine = addConsoleText(F("Configure Mount..."));

    LOG(DEBUG_ANY, "[SYSTEM]: Read Configuration...");

    // The mount uses EEPROM storage locations 0-10 that it reads during construction
    // The LCD uses EEPROM storage location 11
    mount.readConfiguration();

    // Read other persisted values and set in mount
    const types::DayTime haTime = eepromStore.getHATime();

    LOG(DEBUG_INFO, "[SYSTEM]: SpeedCal: %s", String(mount.getSpeedCalibration(), 5).c_str());
    LOG(DEBUG_INFO, "[SYSTEM]: TRKSpeed: %s", String(mount.getSpeed(TRACKING), 5).c_str());

    siteTimePort.setHA(haTime);

    // For LCD screen, it's better to initialize the target to where we are (RA)
    mount.targetRA() = mount.currentRA();

    return configureLine;
}

/// Setup service to periodically service the steppers.
void startStepperService()
{
#if defined(ESP32)
    LOG(DEBUG_ANY, "[SYSTEM]: Setup StepperControlTask on Core 0...");
    disableCore0WDT();
    xTaskCreatePinnedToCore(stepperControlTask,  // Function to run on this core
                            "StepperControl",    // Name of this task
                            32767,               // Stack space in bytes
                            &stepPump,           // payload
                            1,                   // Priority (2 is higher than 1)
                            &StepperTask,        // The location that receives the thread id
                            0);                  // The core to run this on
#else
    #ifndef NEW_STEPPER_LIB
    // Stepper servicing rate: 2 kHz on AVR (higher rates there interfere with serial
    // communications and mess up OATControl), 5 kHz on RP2040 (see STEPPER_CONTROL_INTERVAL_MS).
    if (!InterruptCallback::setInterval(STEPPER_CONTROL_INTERVAL_MS, stepperControlTimerCallback, &stepPump))
    {
        LOG(DEBUG_MOUNT, "[SYSTEM]: CANNOT setup interrupt timer!");
    }
    #endif
#endif
}

void runUartSelfTest()
{
#if UART_CONNECTION_TEST_TX == 1
    int testLine = addConsoleText(F("Test UARTs..."));
    #if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    LOG(DEBUG_STEPPERS, "[STEPPERS]: Moving RA axis using UART commands...");
    mount.testRA_UART_TX();
    LOG(DEBUG_STEPPERS, "[STEPPERS]: Finished moving RA axis using UART commands.");
    #endif

    #if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    LOG(DEBUG_STEPPERS, "[STEPPERS]: Moving DEC axis using UART commands...");
    mount.testDEC_UART_TX();
    LOG(DEBUG_STEPPERS, "[STEPPERS]: Finished moving DEC axis using UART commands.");
    #endif
    updateConsoleText(testLine, F("Test UARTs... OK"));
#endif
}

void finishBoot()
{
    LOG(DEBUG_ANY, "[SYSTEM]: Setting %s hemisphere...", mount.readout().isNorthernHemisphere() ? "northern" : "southern");
    mount.configureHemisphere(mount.readout().isNorthernHemisphere(), true);

    if (app::kTrackOnBoot)
    {
        // Start the tracker.
        LOG(DEBUG_ANY, "[SYSTEM]: Start Tracking...");
        mount.startSlewing(TRACKING);
    }

    mount.bootComplete();
    LOG(DEBUG_ANY, "[SYSTEM]: Boot complete!");
    addConsoleText(F("BOOT COMPLETE!"));
#if (INFO_DISPLAY_TYPE != INFO_DISPLAY_TYPE_NONE)
    delay(500);
    if (infoDisplay != nullptr)
    {
        infoDisplay->setConsoleMode(false);
    }
#endif
}

void showTestMenu()
{
#if TEST_VERIFY_MODE == 1
    TestMenu::getCurrentMenu()->display();
#endif
}

}  // namespace

namespace app
{

void bootstrap()
{
    startDebug();
    startSerial();
    logBootBanner();
    startInfoDisplay();
    startGps();
    startDewHeater();
    startFan();
    startSensors();
    remapUartPins();
    startDriverPins();
    enableSharedDrivers();
    startHomingSensors();
    startPersistentStore();
    startMenu();
    logHardwareInfo();
    startCommandProcessor();
    startWifi();
    startSteppers();

    const int configureLine = loadConfiguration();
    startStepperService();
    updateConsoleText(configureLine, F("Configure Mount... OK"));

    runUartSelfTest();
    finishBoot();
    showTestMenu();
}

}  // namespace app
