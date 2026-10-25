// Entry point. `setup()` used to live in `src/b_setup.hpp`, a header the menu screens also
// included; the object graph it builds now lives in `app/BoardWiring.cpp`, and this file is
// the only translation unit that also pulls in the UI.

#include "../inc/Globals.hpp"
#include "../../Configuration.hpp"
#include "../Utility.hpp"
#include "MountExecutor.hpp"

#include "BoardWiring.hpp"

#if (INFO_DISPLAY_TYPE == INFO_DISPLAY_TYPE_I2C_SSD1306_128x64)
    #include "../adapters/Ssd1306InfoDisplay.hpp"
#endif
#include "DriverWiring.hpp"
#include "MotorWiring.hpp"
#include "SensorWiring.hpp"

#include "../a_inits.hpp"
#if defined(OAT_DEBUG_BUILD)
    #if BOARD < 1000
        #include "avr8-stub.h"
    #else
        #error "Debugging not supported on this platform"
    #endif
#endif

#if TEST_VERIFY_MODE == 1
    #include "../testmenu.hpp"
#endif

#ifndef NEW_STEPPER_LIB
    #include "../hal/InterruptCallback.hpp"
#endif
#include "../LcdSlewStatus.hpp"
#include "../c65_startup.hpp"
#include "../c70_menuRA.hpp"
#include "../c71_menuDEC.hpp"
#include "../c722_menuPOI.hpp"
#include "../c725_menuHOME.hpp"
#include "../c72_menuHA.hpp"
#include "../c72_menuHA_GPS.hpp"
#include "../c75_menuCTRL.hpp"
#include "../c76_menuCAL.hpp"
#include "../c78_menuINFO.hpp"
#include "../c_buttons.hpp"
#include "../app/SerialPump.hpp"

void setup()
{
#if defined(OAT_DEBUG_BUILD)
    #if BOARD < 1000
    debug_init();  // Setup avr-stub
    breakpoint();  // Set a breakpoint as soon as possible
    #else
        #error "Debugging not supported on this platform"
    #endif
#else
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

#if USE_GPS == 1
    LOG(DEBUG_ANY, "[SYSTEM]: Initializing GPS...");
    #if (INFO_DISPLAY_TYPE != INFO_DISPLAY_TYPE_NONE)
    int gpsLine = addConsoleText(F("Initialize GPS..."));
    #endif
    #if defined(ARDUINO_ARCH_RP2040) && defined(RP2040_GPS_TX_PIN) && defined(RP2040_GPS_RX_PIN)
    GPS_SERIAL_PORT.setTX(RP2040_GPS_TX_PIN);
    GPS_SERIAL_PORT.setRX(RP2040_GPS_RX_PIN);
    #endif
    GPS_SERIAL_PORT.begin(GPS_BAUD_RATE);
    #if (INFO_DISPLAY_TYPE != INFO_DISPLAY_TYPE_NONE)
    updateConsoleText(gpsLine, F("Initialize GPS... OK"));
    #endif
#endif

//Turn on dew heater
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

#if defined(RP2040_FAN1_PIN)
    pinMode(RP2040_FAN1_PIN, OUTPUT);
    analogWrite(RP2040_FAN1_PIN, (RP2040_FAN1_POWER_PERCENT * 255) / 100);
#endif

#if (USE_RA_END_SWITCH == 1 || USE_DEC_END_SWITCH == 1)
    int endSwitchesLine = addConsoleText(F("Init End Switches..."));
    LOG(DEBUG_ANY, "[SYSTEM]: Init EndSwitches...");
    updateConsoleText(endSwitchesLine, F("Init End Switches... OK"));
#endif

    // End switches and hall sensors are built by the composition root; a board may have either,
    // both or neither, so the call itself is unconditional.
    app::attachSensors(mount);

    /////////////////////////////////
    //   RP2040: UART pin remapping
    /////////////////////////////////
    // RP2040_UART1_TX_PIN / RP2040_UART1_RX_PIN are defined in the board's
    // pins file only for RP2040 targets that use a non-default UART1 mapping.
#if defined(ARDUINO_ARCH_RP2040) && defined(RP2040_UART1_TX_PIN) && defined(RP2040_UART1_RX_PIN)
    Serial2.setTX(RP2040_UART1_TX_PIN);
    Serial2.setRX(RP2040_UART1_RX_PIN);
    Serial2.setPollingMode(true);  // Must be called before begin(); avoids IRQ/CoreMutex blocking in TMCStepper read path
    Serial2.begin(115200);         // TMCStepper HW-serial path never calls begin(); must do it here.
#endif

    /////////////////////////////////
    //   Microstepping/driver pins
    /////////////////////////////////
    int raLine = addConsoleText(F("Init RA axis..."));
    LOG(DEBUG_ANY, "[SYSTEM]: Initializing RA microstepping/driver pins...");
    pinMode(RA_EN_PIN, OUTPUT);
    digitalWrite(RA_EN_PIN, LOW);  // ENABLE, LOW to enable
#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE || RA_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC
    #if defined(RA_MS0_PIN)
    digitalWrite(RA_MS0_PIN, HIGH);  // MS0
    #endif
    #if defined(RA_MS1_PIN)
    digitalWrite(RA_MS1_PIN, HIGH);  // MS1
    #endif
    #if defined(RA_MS2_PIN)
    digitalWrite(RA_MS2_PIN, HIGH);  // MS2
    #endif
#endif
#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    LOG(DEBUG_ANY, "[SYSTEM]: Initializing TMC2209 UART pins and Serial port for RA...");
    // include TMC2209 UART pins
    #if defined(RA_DIAG_PIN)
    pinMode(RA_DIAG_PIN, INPUT);
    #endif

    #ifdef RA_SERIAL_PORT
        #ifdef OAE
    RA_SERIAL_PORT.begin(57600, SERIAL_8N1, RA_RX_PIN, RA_TX_PIN);
        #elif defined(ARDUINO_ARCH_RP2040)
    RA_SERIAL_PORT.begin(115200);  // TMCStepper requires 115200 baud on RP2040
        #else
    RA_SERIAL_PORT.begin(57600);  // Start HardwareSerial comms with driver
        #endif
    //
    #endif
#endif
    updateConsoleText(raLine, F("Init RA axis... OK"));

    int decLine = addConsoleText(F("Init DEC axis..."));
    LOG(DEBUG_ANY, "[SYSTEM]: Initializing DEC driver pin %s...", String(DEC_EN_PIN).c_str());
    pinMode(DEC_EN_PIN, OUTPUT);
    digitalWrite(DEC_EN_PIN, LOW);  // ENABLE, LOW to enable
#if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE || DEC_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC
    #if defined(DEC_MS0_PIN)
    digitalWrite(DEC_MS0_PIN, HIGH);  // MS1
    #endif
    #if defined(DEC_MS1_PIN)
    digitalWrite(DEC_MS1_PIN, HIGH);  // MS2
    #endif
    #if defined(DEC_MS2_PIN)
    digitalWrite(DEC_MS2_PIN, HIGH);  // MS3
    #endif
#endif

#if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    LOG(DEBUG_ANY, "[SYSTEM]: Initializing TMC2209 UART pins and Serial port for DEC...");
    // include TMC2209 UART pins
    #if defined(DEC_DIAG_PIN)
    pinMode(DEC_DIAG_PIN, INPUT);
    #endif
    #ifdef DEC_SERIAL_PORT
        #ifdef OAE
    DEC_SERIAL_PORT.begin(57600, SERIAL_8N1, DEC_RX_PIN, DEC_TX_PIN);
        #elif defined(ARDUINO_ARCH_RP2040)
    DEC_SERIAL_PORT.begin(115200);  // TMCStepper requires 115200 baud on RP2040
        #else
    DEC_SERIAL_PORT.begin(57600);  // Start HardwareSerial comms with driver
        #endif
    #endif
#endif
    updateConsoleText(decLine, F("Init DEC axis... OK"));
    LOG(DEBUG_ANY, "[SYSTEM]: RA/DEC init complete...");

#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    int azLine = addConsoleText(F("Init AZ axis..."));
    LOG(DEBUG_ANY, "[SYSTEM]: Initializing AZ microstepping/driver pins...");
    pinMode(AZ_EN_PIN, OUTPUT);
    digitalWrite(AZ_EN_PIN, HIGH);  // Logic HIGH to disable the driver initally
    #if AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
        // include TMC2209 UART pins
        #if defined(AZ_DIAG_PIN)
    pinMode(AZ_DIAG_PIN, INPUT);
        #endif
        #ifdef AZ_SERIAL_PORT
            #if defined(ARDUINO_ARCH_RP2040)
    AZ_SERIAL_PORT.begin(115200);  // TMCStepper requires 115200 baud on RP2040
            #else
    AZ_SERIAL_PORT.begin(57600);  // Start HardwareSerial comms with driver
            #endif
        #endif
    #endif
    updateConsoleText(azLine, F("Init AZ axis... OK"));
#endif

#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    int altLine = addConsoleText(F("Init ALT axis..."));
    LOG(DEBUG_ANY, "[SYSTEM]: Initializing ALT microstepping/driver pins...");
    pinMode(ALT_EN_PIN, OUTPUT);
    digitalWrite(ALT_EN_PIN, HIGH);  // Logic HIGH to disable the driver initally
    #if ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
        // include TMC2209 UART pins
        #if defined(ALT_DIAG_PIN)
    pinMode(ALT_DIAG_PIN, INPUT);
        #endif
        #ifdef ALT_SERIAL_PORT
            #if defined(ARDUINO_ARCH_RP2040)
    ALT_SERIAL_PORT.begin(115200);  // TMCStepper requires 115200 baud on RP2040
            #else
    ALT_SERIAL_PORT.begin(57600);  // Start HardwareSerial comms with driver
            #endif
        #endif
    #endif
    updateConsoleText(altLine, F("Init ALT axis... OK"));
#endif

#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    int focusLine = addConsoleText(F("Init Focuser..."));
    LOG(DEBUG_FOCUS, "[FOCUS]: setup(): focus disabling enable pin");
    pinMode(FOCUS_EN_PIN, OUTPUT);
    digitalWrite(FOCUS_EN_PIN, HIGH);  // Logic HIGH to disable the driver initally
    #if FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
        // include TMC2209 UART pins
        #ifdef FOCUS_SERIAL_PORT
    LOG(DEBUG_FOCUS, "[FOCUS]: setup(): focus TMC2209U starting comms");
            #if defined(ARDUINO_ARCH_RP2040)
    FOCUS_SERIAL_PORT.begin(115200);  // TMCStepper requires 115200 baud on RP2040
            #else
    FOCUS_SERIAL_PORT.begin(57600);  // Start HardwareSerial comms with driver
            #endif
        #endif
    #endif
    updateConsoleText(focusLine, F("Init Focuser... OK"));
#endif

#if defined(RP2040_SHARED_EN_PIN)
    // On boards where all stepper EN lines share one GPIO (e.g. JackW01 carrier),
    // the per-axis init sequence leaves that GPIO HIGH (logic disabled) after the
    // AZ/ALT blocks run.  Force it LOW here so every driver is enabled once all
    // UART addresses have been configured.  Per-axis power management is handled
    // via the TMC2209 IHOLD register over UART rather than toggling this pin.
    pinMode(RP2040_SHARED_EN_PIN, OUTPUT);
    digitalWrite(RP2040_SHARED_EN_PIN, LOW);
#endif
    // end microstepping -------------------

#if USE_HALL_SENSOR_RA_AUTOHOME == 1 || USE_HALL_SENSOR_DEC_AUTOHOME == 1
    int homingLine = addConsoleText(F("Init homing sensors..."));
    #if USE_HALL_SENSOR_RA_AUTOHOME == 1
    pinMode(RA_HOMING_SENSOR_PIN, INPUT);
    #endif

    #if USE_HALL_SENSOR_DEC_AUTOHOME == 1
    pinMode(DEC_HOMING_SENSOR_PIN, INPUT);
    #endif
    updateConsoleText(homingLine, F("Init homing sensors... OK"));
#endif

    LOG(DEBUG_ANY, "[SYSTEM]: Initializing EEPROM store...");
    int eepromLine = addConsoleText(F("INIT EEPROM..."));

    initPersistentStore();
    LOG(DEBUG_ANY, "[SYSTEM]: EEPROM store ready!");
    updateConsoleText(eepromLine, F("INIT EEPROM... OK"));

// Calling the LCD startup here, I2C can't be found if called earlier
#if DISPLAY_TYPE != DISPLAY_TYPE_NONE
    LOG(DEBUG_ANY, "[SYSTEM]: Get LCD ready...");
    int lcdLine = addConsoleText(F("Init LCD..."));
    lcdMenu.startup();

    // Show a splash screen
    lcdMenu.setCursor(0, 0);
    #ifdef OAM
    lcdMenu.printMenu(F(" OpenAstroMount"));
    #elif defined(OAE)
    lcdMenu.printMenu(F("OpenAstroExplorer"));
    #else
    lcdMenu.printMenu(F("OpenAstroTracker"));
    #endif
    lcdMenu.setCursor(5, 1);
    lcdMenu.printMenu(VERSION);
    delay(1000);  // Pause on splash screen

    // Check for EEPROM reset (Button down during boot)
    long lcdCheckStart = millis();
    if (lcdButtons.currentState() == btnDOWN)
    {
        // Wait for button release
        lcdMenu.setCursor(13, 1);
        lcdMenu.printMenu(F("CLR"));
        LOG(DEBUG_INFO, "[SYSTEM]: DOWN pressed, waiting for button release!");
        bool timedOut = false;
        while (lcdButtons.currentState() != btnNONE)
        {
            delay(10);
            // Wait 3s maximum for button to be released.
            if (millis() - lcdCheckStart > 3000)
            {
                LOG(DEBUG_INFO, "[SYSTEM]: Timedout waiting for button release. LCD not installed?");
                timedOut = true;
                break;
            }
        }
        if (!timedOut)
        {
            LOG(DEBUG_INFO, "[SYSTEM]: Erasing configuration in EEPROM!");
            mount.clearConfiguration();
            LOG(DEBUG_INFO, "[SYSTEM]: Button released, continuing");
        }
    }

    // Create the LCD top-level menu items
    lcdMenu.addItem("RA", RA_Menu);
    lcdMenu.addItem("DEC", DEC_Menu);

    #if SUPPORT_POINTS_OF_INTEREST == 1
    lcdMenu.addItem("GO", POI_Menu);
    #else
    lcdMenu.addItem("GO", Home_Menu);
    #endif

    lcdMenu.addItem("HA", HA_Menu);

    #if SUPPORT_MANUAL_CONTROL == 1
    lcdMenu.addItem("CTRL", Control_Menu);
    #endif

    #if SUPPORT_CALIBRATION == 1
    lcdMenu.addItem("CAL", Calibration_Menu);
    #endif

    #if FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE
    lcdMenu.addItem("FOC", Focuser_Menu);
    #endif

    #if SUPPORT_INFO_DISPLAY == 1
    lcdMenu.addItem("INFO", Status_Menu);
    #endif
    updateConsoleText(lcdLine, F("Init LCD... OK"));

#endif  // DISPLAY_TYPE > 0

    LOG(DEBUG_ANY, "[SYSTEM]: Hardware: %s", mount.getMountHardwareInfo());

    // Create the command processor singleton
    LOG(DEBUG_ANY, "[SYSTEM]: Initialize LX200 handler...");
    int commandProcessorLine = addConsoleText(F("Init MEADE handler..."));
    MeadeCommandPorts commandPorts;
    commandPorts.configuration = &mount;
    commandPorts.axes          = &mount;
    commandPorts.readModel     = &mount.readout();
    commandPorts.siteTime      = &siteTimePort;
    commandPorts.events        = &mountCoordinator;
    commandPorts.clock         = &clockPort;
    commandPorts.gyro          = gyro;
    MeadeCommandAdapter::createProcessor(commandPorts, &lcdMenu);
    updateConsoleText(commandProcessorLine, F("Init MEADE handler... OK"));

#if (WIFI_ENABLED == 1)
    LOG(DEBUG_ANY, "[SYSTEM]: Setup Wifi...");
    wifiControl.setup();
#endif

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

    int configureLine = addConsoleText(F("Configure Mount..."));

    LOG(DEBUG_ANY, "[SYSTEM]: Read Configuration...");

    // The mount uses EEPROM storage locations 0-10 that it reads during construction
    // The LCD uses EEPROM storage location 11
    mount.readConfiguration();

    // Read other persisted values and set in mount
    DayTime haTime(settingsStore.getHATime());

    LOG(DEBUG_INFO, "[SYSTEM]: SpeedCal: %s", String(mount.getSpeedCalibration(), 5).c_str());
    LOG(DEBUG_INFO, "[SYSTEM]: TRKSpeed: %s", String(mount.getSpeed(TRACKING), 5).c_str());

    siteTimePort.setHA(haTime);

    // For LCD screen, it's better to initialize the target to where we are (RA)
    mount.targetRA() = mount.currentRA();

// Setup service to periodically service the steppers.
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

    updateConsoleText(configureLine, F("Configure Mount... OK"));

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

    LOG(DEBUG_ANY, "[SYSTEM]: Setting %s hemisphere...", inNorthernHemisphere ? "northern" : "southern");
    mount.configureHemisphere(inNorthernHemisphere, true);

#if TRACK_ON_BOOT == 1
    // Start the tracker.
    LOG(DEBUG_ANY, "[SYSTEM]: Start Tracking...");
    mount.startSlewing(TRACKING);
#endif

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

#if TEST_VERIFY_MODE == 1
    TestMenu::getCurrentMenu()->display();
#endif
}
