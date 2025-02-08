// Create the LCD menu variable and initialize the LCD (16x2 characters)

#pragma once

#if defined(OAT_DEBUG_BUILD)
PUSH_NO_WARNINGS
    #if BOARD < 1000
        #include "avr8-stub.h"
    #else
        #error "Debugging not supported on this platform"
    #endif
POP_NO_WARNINGS
#endif

#if TEST_VERIFY_MODE == 1
    #include "testmenu.hpp"
#endif

#ifndef NEW_STEPPER_LIB
    #include "InterruptCallback.hpp"
#endif

#include "Utility.hpp"
#include "EPROMStore.hpp"
#include "a_inits.hpp"
#include "LcdMenu.hpp"
#include "LcdButtons.hpp"
#if (INFO_DISPLAY_TYPE == INFO_DISPLAY_TYPE_I2C_SSD1306_128x64)
    #include "SSD1306_128x64_Display.hpp"
#endif

LcdMenu lcdMenu(16, 2, MAXMENUITEMS);
#if DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD
LcdButtons lcdButtons(LCD_KEY_SENSE_PIN, &lcdMenu);
#endif

#if DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23017 || DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23008                           \
    || DISPLAY_TYPE == DISPLAY_TYPE_LCD_JOY_I2C_SSD1306
LcdButtons lcdButtons(&lcdMenu);
#endif

#ifdef ESP32
DRAM_ATTR Mount mount(&lcdMenu);
#else
Mount mount(&lcdMenu);
#endif

#if (WIFI_ENABLED == 1)
    #include "WifiControl.hpp"
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

/////////////////////////////////
//
// Main program setup
//
/////////////////////////////////
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
    #if DEBUG_LEVEL > 0 && DEBUG_SEPARATE_SERIAL == 1
    DEBUG_SERIAL_PORT.begin(DEBUG_SERIAL_BAUDRATE);
    #endif
#endif

#if TEST_VERIFY_MODE == 1
    #ifdef OAM
    Serial.print(F("Booting OAM Firmware "));
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
    mount.setupInfoDisplay();
    LOG(DEBUG_ANY, "[SYSTEM]: OLED info screen ready!");
    mount.getInfoDisplay()->addConsoleText(F("BOOTING " VERSION), false);
#endif

#if USE_GPS == 1
    GPS_SERIAL_PORT.begin(GPS_BAUD_RATE);
#endif

//Turn on dew heater
#if DEW_HEATER == 1
    #if defined(DEW_HEATER_1_PIN)
    digitalWrite(DEW_HEATER_1_PIN, HIGH);
    #endif
    #if defined(DEW_HEATER_2_PIN)
    digitalWrite(DEW_HEATER_2_PIN, HIGH);
    #endif
#endif

#if (USE_RA_END_SWITCH == 1 || USE_DEC_END_SWITCH == 1)
    LOG(DEBUG_ANY, "[SYSTEM]: Init EndSwitches...");
    mount.setupEndSwitches();
#endif

    /////////////////////////////////
    //   Microstepping/driver pins
    /////////////////////////////////
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
    // include TMC2209 UART pins
    pinMode(RA_DIAG_PIN, INPUT);
    #ifdef RA_SERIAL_PORT
    RA_SERIAL_PORT.begin(57600);  // Start HardwareSerial comms with driver
    #endif
#endif
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
    // include TMC2209 UART pins
    pinMode(DEC_DIAG_PIN, INPUT);
    #ifdef DEC_SERIAL_PORT
    DEC_SERIAL_PORT.begin(57600);  // Start HardwareSerial comms with driver
    #endif
#endif

#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    pinMode(AZ_EN_PIN, OUTPUT);
    digitalWrite(AZ_EN_PIN, HIGH);  // Logic HIGH to disable the driver initally
    #if AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    // include TMC2209 UART pins
    pinMode(AZ_DIAG_PIN, INPUT);
        #ifdef AZ_SERIAL_PORT
    AZ_SERIAL_PORT.begin(57600);  // Start HardwareSerial comms with driver
        #endif
    #endif
#endif

#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    pinMode(ALT_EN_PIN, OUTPUT);
    digitalWrite(ALT_EN_PIN, HIGH);  // Logic HIGH to disable the driver initally
    #if ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    // include TMC2209 UART pins
    pinMode(ALT_DIAG_PIN, INPUT);
        #ifdef ALT_SERIAL_PORT
    ALT_SERIAL_PORT.begin(57600);  // Start HardwareSerial comms with driver
        #endif
    #endif
#endif

#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    LOG(DEBUG_FOCUS, "[FOCUS]: setup(): focus disabling enable pin");
    pinMode(FOCUS_EN_PIN, OUTPUT);
    digitalWrite(FOCUS_EN_PIN, HIGH);  // Logic HIGH to disable the driver initally
    #if FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
        // include TMC2209 UART pins
        #ifdef FOCUS_SERIAL_PORT
    LOG(DEBUG_FOCUS, "[FOCUS]: setup(): focus TMC2209U starting comms");
    FOCUS_SERIAL_PORT.begin(57600);  // Start HardwareSerial comms with driver
        #endif
    #endif
#endif
    // end microstepping -------------------

#if USE_HALL_SENSOR_RA_AUTOHOME == 1
    pinMode(RA_HOMING_SENSOR_PIN, INPUT);
#endif

#if USE_HALL_SENSOR_DEC_AUTOHOME == 1
    pinMode(DEC_HOMING_SENSOR_PIN, INPUT);
#endif

#if (INFO_DISPLAY_TYPE != INFO_DISPLAY_TYPE_NONE)
    int eepromLine = mount.getInfoDisplay()->addConsoleText(F("INIT EEPROM..."));
#endif

    LOG(DEBUG_ANY, "[SYSTEM]: Get EEPROM store ready...");
    EEPROMStore::initialize();
    LOG(DEBUG_ANY, "[SYSTEM]: EEPROM store ready!");

#if (INFO_DISPLAY_TYPE != INFO_DISPLAY_TYPE_NONE)
    mount.getInfoDisplay()->updateConsoleText(eepromLine, F("INIT EEPROM... OK"));
#endif

// Calling the LCD startup here, I2C can't be found if called earlier
#if DISPLAY_TYPE != DISPLAY_TYPE_NONE
    LOG(DEBUG_ANY, "[SYSTEM]: Get LCD ready...");
    lcdMenu.startup();

    // Show a splash screen
    lcdMenu.setCursor(0, 0);
    #ifdef OAM
    lcdMenu.printMenu(" OpenAstroMount");
    #else
    lcdMenu.printMenu("OpenAstroTracker");
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
        lcdMenu.printMenu("CLR");
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

#endif  // DISPLAY_TYPE > 0

    LOG(DEBUG_ANY, "[SYSTEM]: Hardware: %s", mount.getMountHardwareInfo().c_str());

    // Create the command processor singleton
    LOG(DEBUG_ANY, "[SYSTEM]: Initialize LX200 handler...");
    MeadeCommandProcessor::createProcessor(&mount, &lcdMenu);

#if (WIFI_ENABLED == 1)
    LOG(DEBUG_ANY, "[SYSTEM]: Setup Wifi...");
    wifiControl.setup();
#endif

#if (INFO_DISPLAY_TYPE != INFO_DISPLAY_TYPE_NONE)
    int stepperLine = mount.getInfoDisplay()->addConsoleText(F("INIT STEPPERS..."));
#endif

    // Configure the mount
    // Delay for a while to get UARTs booted...
    delay(1000);

    LOG(DEBUG_ANY, "[SYSTEM]: Configure steppers...");

// Set the stepper motor parameters
#if (RA_STEPPER_TYPE != STEPPER_TYPE_NONE)
    LOG(DEBUG_ANY, "[STEPPERS]: Configure RA stepper NEMA.");
    LOG(DEBUG_ANY, "[STEPPERS]: Stepper SPR     : %d", RA_STEPPER_SPR);
    LOG(DEBUG_ANY, "[STEPPERS]: Slew Microsteps : %d", RA_SLEW_MICROSTEPPING);
    LOG(DEBUG_ANY, "[STEPPERS]: Trk Microsteps  : %d", RA_TRACKING_MICROSTEPPING);
    LOG(DEBUG_ANY, "[STEPPERS]: Stepper SPR     : %d", RA_STEPPER_SPR);
    LOG(DEBUG_ANY, "[STEPPERS]: Transmission    : %f", RA_TRANSMISSION);
    #ifdef NEW_STEPPER_LIB
    LOG(DEBUG_ANY, "[STEPPERS]: Driver Slew SPR : %l", config::Ra::DRIVER_SPR_SLEW);
    LOG(DEBUG_ANY, "[STEPPERS]: Driver Trk SPR  : %l", config::Ra::DRIVER_SPR_TRK);
    LOG(DEBUG_ANY, "[STEPPERS]: SPR Slew        : %f", config::Ra::SPR_SLEW);
    LOG(DEBUG_ANY, "[STEPPERS]: SPR Trk         : %f", config::Ra::SPR_TRK);
    LOG(DEBUG_ANY, "[STEPPERS]: Speed Slew      : %f", config::Ra::SPEED_SLEW);
    LOG(DEBUG_ANY, "[STEPPERS]: Accel Slew      : %f", config::Ra::ACCEL_SLEW);
    LOG(DEBUG_ANY, "[STEPPERS]: Speed Trk       : %f", config::Ra::SPEED_TRK);
    LOG(DEBUG_ANY, "[STEPPERS]: Speed TrkComp   : %f", config::Ra::SPEED_COMPENSATION);
    LOG(DEBUG_ANY, "[STEPPERS]: Configure RA stepper NEMA...");
    mount.configureRAStepper(RAmotorPin1, RAmotorPin2, config::Ra::SPEED_SLEW, config::Ra::ACCEL_SLEW);
    #else
    LOG(DEBUG_ANY, "[STEPPERS]: Configure RA stepper NEMA...");
    mount.configureRAStepper(RAmotorPin1, RAmotorPin2, RA_STEPPER_SPEED, RA_STEPPER_ACCELERATION);
    #endif
#endif

#if (DEC_STEPPER_TYPE != STEPPER_TYPE_NONE)
    LOG(DEBUG_ANY, "[STEPPERS]: Configure DEC stepper NEMA.");
    LOG(DEBUG_ANY, "[STEPPERS]: Slew Microsteps : %d", DEC_SLEW_MICROSTEPPING);
    LOG(DEBUG_ANY, "[STEPPERS]: Stepper SPR     : %d", DEC_STEPPER_SPR);
    LOG(DEBUG_ANY, "[STEPPERS]: Transmission    : %f", DEC_TRANSMISSION);
    #ifdef NEW_STEPPER_LIB
    LOG(DEBUG_ANY, "[STEPPERS]: Driver Slew SPR : %l", config::Dec::DRIVER_SPR_SLEW);
    LOG(DEBUG_ANY, "[STEPPERS]: Driver Trk SPR  : %l", config::Dec::DRIVER_SPR_TRK);
    LOG(DEBUG_ANY, "[STEPPERS]: SPR Slew        : %f", config::Dec::SPR_SLEW);
    LOG(DEBUG_ANY, "[STEPPERS]: SPR Trk         : %f", config::Dec::SPR_TRK);
    LOG(DEBUG_ANY, "[STEPPERS]: Speed Slew      : %f", config::Dec::SPEED_SLEW);
    LOG(DEBUG_ANY, "[STEPPERS]: Accel Slew      : %f", config::Dec::ACCEL_SLEW);
    LOG(DEBUG_ANY, "[STEPPERS]: Speed Trk       : %f", config::Dec::SPEED_TRK);
    LOG(DEBUG_ANY, "[STEPPERS]: Configure DEC stepper NEMA...");
    mount.configureDECStepper(DECmotorPin1, DECmotorPin2, config::Dec::SPEED_SLEW, config::Dec::ACCEL_SLEW);
    #else
    LOG(DEBUG_ANY, "[STEPPERS]: Configure DEC stepper NEMA...");
    mount.configureDECStepper(DECmotorPin1, DECmotorPin2, DEC_STEPPER_SPEED, DEC_STEPPER_ACCELERATION);
    #endif
#endif

#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    LOG(DEBUG_ANY, "[STEPPERS]: Configure RA driver TMC2209 UART...");
    #if SW_SERIAL_UART == 0
    mount.configureRAdriver(&RA_SERIAL_PORT, R_SENSE, RA_DRIVER_ADDRESS, RA_RMSCURRENT, RA_STALL_VALUE);
    #elif SW_SERIAL_UART == 1
    mount.configureRAdriver(RA_SERIAL_PORT_RX, RA_SERIAL_PORT_TX, R_SENSE, RA_DRIVER_ADDRESS, RA_RMSCURRENT, RA_STALL_VALUE);
    #endif
#endif
#if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    LOG(DEBUG_ANY, "[STEPPERS]: Configure DEC driver TMC2209 UART...");
    #if SW_SERIAL_UART == 0
    mount.configureDECdriver(&DEC_SERIAL_PORT, R_SENSE, DEC_DRIVER_ADDRESS, DEC_RMSCURRENT, DEC_STALL_VALUE);
    #elif SW_SERIAL_UART == 1
    mount.configureDECdriver(DEC_SERIAL_PORT_RX, DEC_SERIAL_PORT_TX, R_SENSE, DEC_DRIVER_ADDRESS, DEC_RMSCURRENT, DEC_STALL_VALUE);
    #endif
#endif

#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    LOG(DEBUG_ANY, "[STEPPERS]: Configure AZ stepper...");
    mount.configureAZStepper(AZmotorPin1, AZmotorPin2, AZ_STEPPER_SPEED, AZ_STEPPER_ACCELERATION);
    #if AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    LOG(DEBUG_ANY, "[STEPPERS]: Configure AZ driver...");
        #if SW_SERIAL_UART == 0
    mount.configureAZdriver(&AZ_SERIAL_PORT, R_SENSE, AZ_DRIVER_ADDRESS, AZ_RMSCURRENT, AZ_STALL_VALUE);
        #elif SW_SERIAL_UART == 1
    mount.configureAZdriver(AZ_SERIAL_PORT_RX, AZ_SERIAL_PORT_TX, R_SENSE, AZ_DRIVER_ADDRESS, AZ_RMSCURRENT, AZ_STALL_VALUE);
        #endif
    #endif
#endif
#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    LOG(DEBUG_ANY, "[STEPPERS]: Configure Alt stepper...");
    mount.configureALTStepper(ALTmotorPin1, ALTmotorPin2, ALT_STEPPER_SPEED, ALT_STEPPER_ACCELERATION);
    #if ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    LOG(DEBUG_ANY, "[STEPPERS]: Configure ALT driver...");
        #if SW_SERIAL_UART == 0
    mount.configureALTdriver(&ALT_SERIAL_PORT, R_SENSE, ALT_DRIVER_ADDRESS, ALT_RMSCURRENT, ALT_STALL_VALUE);
        #elif SW_SERIAL_UART == 1
    mount.configureALTdriver(ALT_SERIAL_PORT_RX, ALT_SERIAL_PORT_TX, R_SENSE, ALT_DRIVER_ADDRESS, ALT_RMSCURRENT, ALT_STALL_VALUE);
        #endif
    #endif
#endif

#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    LOG(DEBUG_ANY, "[STEPPERS]: setup(): Configure Focus stepper...");
    mount.configureFocusStepper(FOCUSmotorPin1, FOCUSmotorPin2, FOCUS_STEPPER_SPEED, FOCUS_STEPPER_ACCELERATION);
    #if FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    LOG(DEBUG_ANY, "[STEPPERS]: setup(): Configure Focus driver...");
    LOG(DEBUG_FOCUS, "[FOCUS]: setup(): RSense %f, RMS Current %fmA", R_SENSE, FOCUS_RMSCURRENT);
        #if SW_SERIAL_UART == 0
    mount.configureFocusDriver(&FOCUS_SERIAL_PORT, R_SENSE, FOCUS_DRIVER_ADDRESS, FOCUS_RMSCURRENT, FOCUS_STALL_VALUE);
        #elif SW_SERIAL_UART == 1
    mount.configureFocusDriver(
        FOCUS_SERIAL_PORT_RX, FOCUS_SERIAL_PORT_TX, R_SENSE, FOCUS_DRIVER_ADDRESS, FOCUS_RMSCURRENT, FOCUS_STALL_VALUE);
        #endif
    #endif
#endif

#if (INFO_DISPLAY_TYPE != INFO_DISPLAY_TYPE_NONE)
    mount.getInfoDisplay()->updateConsoleText(stepperLine, F("INIT STEPPERS... OK"));
#endif

#if (INFO_DISPLAY_TYPE != INFO_DISPLAY_TYPE_NONE)
    mount.getInfoDisplay()->addConsoleText(F("CONFIGURING..."));
#endif

    LOG(DEBUG_ANY, "[SYSTEM]: Read Configuration...");

    // The mount uses EEPROM storage locations 0-10 that it reads during construction
    // The LCD uses EEPROM storage location 11
    mount.readConfiguration();

    // Read other persisted values and set in mount
    DayTime haTime = EEPROMStore::getHATime();

    LOG(DEBUG_INFO, "[SYSTEM]: SpeedCal: %s", String(mount.getSpeedCalibration(), 5).c_str());
    LOG(DEBUG_INFO, "[SYSTEM]: TRKSpeed: %s", String(mount.getSpeed(TRACKING), 5).c_str());

    mount.setHA(haTime);

    // For LCD screen, it's better to initialize the target to where we are (RA)
    mount.targetRA() = mount.currentRA();

// Setup service to periodically service the steppers.
#if defined(ESP32)

    disableCore0WDT();
    xTaskCreatePinnedToCore(stepperControlTask,  // Function to run on this core
                            "StepperControl",    // Name of this task
                            32767,               // Stack space in bytes
                            &mount,              // payload
                            1,                   // Priority (2 is higher than 1)
                            &StepperTask,        // The location that receives the thread id
                            0);                  // The core to run this on

#else
    #ifndef NEW_STEPPER_LIB
    // 2 kHz updates (higher frequency interferes with serial communications and complete messes up OATControl communications)
    if (!InterruptCallback::setInterval(0.5f, stepperControlTimerCallback, &mount))
    {
        LOG(DEBUG_MOUNT, "[SYSTEM]: CANNOT setup interrupt timer!");
    }
    #endif
#endif

#if UART_CONNECTION_TEST_TX == 1
    #if (INFO_DISPLAY_TYPE != INFO_DISPLAY_TYPE_NONE)
    int testLine = mount.getInfoDisplay()->addConsoleText(F("TEST STEPPERS..."));
    #endif
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
    #if (INFO_DISPLAY_TYPE != INFO_DISPLAY_TYPE_NONE)
    mount.getInfoDisplay()->updateConsoleText(testLine, F("TEST STEPPERS... OK"));
    #endif
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
#if (INFO_DISPLAY_TYPE != INFO_DISPLAY_TYPE_NONE)
    mount.getInfoDisplay()->addConsoleText(F("BOOT COMPLETE!"));
    delay(250);
    mount.getInfoDisplay()->setConsoleMode(false);
#endif
#if TEST_VERIFY_MODE == 1
    TestMenu::getCurrentMenu()->display();
#endif
}
