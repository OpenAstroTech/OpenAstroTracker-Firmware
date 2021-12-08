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

#include "InterruptCallback.hpp"

#include "Utility.hpp"
#include "EPROMStore.hpp"
#include "a_inits.hpp"
#include "LcdMenu.hpp"
#include "LcdButtons.hpp"

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
        vTaskDelay(1);  // 1 ms 	// This will limit max stepping rate to 1 kHz
    }
}

#else
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
#endif

#if USE_GPS == 1
    GPS_SERIAL_PORT.begin(GPS_BAUD_RATE);
#endif

//Turn on dew heater
#if DEW_HEATER == 1
    digitalWrite(DEW_HEATER_1_PIN, HIGH);
    digitalWrite(DEW_HEATER_2_PIN, HIGH);
#endif

/////////////////////////////////
//   Microstepping/driver pins
/////////////////////////////////
#if RA_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC
    // include A4988 microstep pins
    //#error "Define Microstep pins and delete this error."
    digitalWrite(RA_EN_PIN, HIGH);
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
#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE
    // include TMC2209 Standalone pins
    pinMode(RA_EN_PIN, OUTPUT);
    digitalWrite(RA_EN_PIN, LOW);  // ENABLE, LOW to enable
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
    pinMode(RA_EN_PIN, OUTPUT);
    digitalWrite(RA_EN_PIN, LOW);
    #ifdef RA_SERIAL_PORT
    RA_SERIAL_PORT.begin(57600);  // Start HardwareSerial comms with driver
    #endif
#endif
#if DEC_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC  // DEC driver startup (for A4988)
    digitalWrite(DEC_EN_PIN, HIGH);
    #if defined(RA_MS0_PIN)
    digitalWrite(DEC_MS0_PIN, HIGH);  // MS1
    #endif
    #if defined(RA_MS0_PIN)
    digitalWrite(DEC_MS1_PIN, HIGH);  // MS2
    #endif
    #if defined(RA_MS0_PIN)
    digitalWrite(DEC_MS2_PIN, HIGH);  // MS3
    #endif
#endif
#if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE
    // include TMC2209 Standalone pins
    pinMode(DEC_EN_PIN, OUTPUT);
    digitalWrite(DEC_EN_PIN, LOW);  // ENABLE, LOW to enable
    #if defined(RA_MS0_PIN)
    digitalWrite(DEC_MS0_PIN, HIGH);  // MS1
    #endif
    #if defined(RA_MS0_PIN)
    digitalWrite(DEC_MS1_PIN, HIGH);  // MS2
    #endif
    #if defined(RA_MS0_PIN)
    digitalWrite(DEC_MS2_PIN, HIGH);  // MS3
    #endif
#endif
#if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    // include TMC2209 UART pins
    pinMode(DEC_DIAG_PIN, INPUT);
    pinMode(DEC_EN_PIN, OUTPUT);
    digitalWrite(DEC_EN_PIN, LOW);
    //pinMode(DEC_MS1_PIN, OUTPUT);
    //digitalWrite(DEC_MS1_PIN, HIGH); // Logic HIGH to MS1 to get 0b01 address
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
    LOGV1(DEBUG_FOCUS, F("setup(): focus disabling enable pin"));
    pinMode(FOCUS_EN_PIN, OUTPUT);
    digitalWrite(FOCUS_EN_PIN, HIGH);  // Logic HIGH to disable the driver initally
    #if FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
        // include TMC2209 UART pins
        #ifdef FOCUS_SERIAL_PORT
    LOGV1(DEBUG_FOCUS, F("setup(): focus TMC2209U starting comms"));
    FOCUS_SERIAL_PORT.begin(57600);  // Start HardwareSerial comms with driver
        #endif
    #endif
#endif
    // end microstepping -------------------

#if USE_HALL_SENSOR_RA_AUTOHOME == 1
    pinMode(RA_HOMING_SENSOR_PIN, INPUT);
#endif

#if !defined(OAT_DEBUG_BUILD)
    Serial.begin(SERIAL_BAUDRATE);
#endif

    LOGV1(DEBUG_ANY, F("."));
    LOGV2(DEBUG_ANY, F("Hello, universe, this is OAT %s!"), VERSION);

    EEPROMStore::initialize();

// Calling the LCD startup here, I2C can't be found if called earlier
#if DISPLAY_TYPE != DISPLAY_TYPE_NONE
    lcdMenu.startup();

    LOGV1(DEBUG_ANY, F("Finishing boot..."));
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
    if (lcdButtons.currentState() == btnDOWN)
    {
        LOGV1(DEBUG_INFO, F("Erasing configuration in EEPROM!"));
        mount.clearConfiguration();
        // Wait for button release
        lcdMenu.setCursor(13, 1);
        lcdMenu.printMenu("CLR");
        LOGV1(DEBUG_INFO, F("Waiting for button release!"));
        while (lcdButtons.currentState() != btnNONE)
        {
            delay(10);
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

    LOGV2(DEBUG_ANY, F("Hardware: %s"), mount.getMountHardwareInfo().c_str());

    // Create the command processor singleton
    LOGV1(DEBUG_ANY, F("Initialize LX200 handler..."));
    MeadeCommandProcessor::createProcessor(&mount, &lcdMenu);

#if (WIFI_ENABLED == 1)
    LOGV1(DEBUG_ANY, F("Setup Wifi..."));
    wifiControl.setup();
#endif

    // Configure the mount
    // Delay for a while to get UARTs booted...
    delay(1000);

// Set the stepper motor parameters
#if (RA_STEPPER_TYPE != STEPPER_TYPE_NONE)
    LOGV1(DEBUG_ANY, F("Configure RA stepper NEMA..."));
    mount.configureRAStepper(RAmotorPin1, RAmotorPin2, RA_STEPPER_SPEED, RA_STEPPER_ACCELERATION);
#else
    #error New stepper type? Configure it here.
#endif

#if (DEC_STEPPER_TYPE != STEPPER_TYPE_NONE)
    LOGV1(DEBUG_ANY, F("Configure DEC stepper NEMA..."));
    mount.configureDECStepper(DECmotorPin1, DECmotorPin2, DEC_STEPPER_SPEED, DEC_STEPPER_ACCELERATION);
#else
    #error New stepper type? Configure it here.
#endif

#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    LOGV1(DEBUG_ANY, F("Configure RA driver TMC2209 UART..."));
    #if SW_SERIAL_UART == 0
    mount.configureRAdriver(&RA_SERIAL_PORT, R_SENSE, RA_DRIVER_ADDRESS, RA_RMSCURRENT, RA_STALL_VALUE);
    #elif SW_SERIAL_UART == 1
    mount.configureRAdriver(RA_SERIAL_PORT_RX, RA_SERIAL_PORT_TX, R_SENSE, RA_DRIVER_ADDRESS, RA_RMSCURRENT, RA_STALL_VALUE);
    #endif
#endif
#if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    LOGV1(DEBUG_ANY, F("Configure DEC driver TMC2209 UART..."));
    #if SW_SERIAL_UART == 0
    mount.configureDECdriver(&DEC_SERIAL_PORT, R_SENSE, DEC_DRIVER_ADDRESS, DEC_RMSCURRENT, DEC_STALL_VALUE);
    #elif SW_SERIAL_UART == 1
    mount.configureDECdriver(DEC_SERIAL_PORT_RX, DEC_SERIAL_PORT_TX, R_SENSE, DEC_DRIVER_ADDRESS, DEC_RMSCURRENT, DEC_STALL_VALUE);
    #endif
#endif

#if (AZ_STEPPER_TYPE != STEPPER_TYPE_NONE)
    LOGV1(DEBUG_ANY, F("Configure AZ stepper..."));
    mount.configureAZStepper(AZmotorPin1, AZmotorPin2, AZ_STEPPER_SPEED, AZ_STEPPER_ACCELERATION);
    #if AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    LOGV1(DEBUG_ANY, F("Configure AZ driver..."));
        #if SW_SERIAL_UART == 0
    mount.configureAZdriver(&AZ_SERIAL_PORT, R_SENSE, AZ_DRIVER_ADDRESS, AZ_RMSCURRENT, AZ_STALL_VALUE);
        #elif SW_SERIAL_UART == 1
    mount.configureAZdriver(AZ_SERIAL_PORT_RX, AZ_SERIAL_PORT_TX, R_SENSE, AZ_DRIVER_ADDRESS, AZ_RMSCURRENT, AZ_STALL_VALUE);
        #endif
    #endif
#endif
#if (ALT_STEPPER_TYPE != STEPPER_TYPE_NONE)
    LOGV1(DEBUG_ANY, F("Configure Alt stepper..."));
    mount.configureALTStepper(ALTmotorPin1, ALTmotorPin2, ALT_STEPPER_SPEED, ALT_STEPPER_ACCELERATION);
    #if ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    LOGV1(DEBUG_ANY, F("Configure ALT driver..."));
        #if SW_SERIAL_UART == 0
    mount.configureALTdriver(&ALT_SERIAL_PORT, R_SENSE, ALT_DRIVER_ADDRESS, ALT_RMSCURRENT, ALT_STALL_VALUE);
        #elif SW_SERIAL_UART == 1
    mount.configureALTdriver(ALT_SERIAL_PORT_RX, ALT_SERIAL_PORT_TX, R_SENSE, ALT_DRIVER_ADDRESS, ALT_RMSCURRENT, ALT_STALL_VALUE);
        #endif
    #endif
#endif

#if (FOCUS_STEPPER_TYPE != STEPPER_TYPE_NONE)
    LOGV1(DEBUG_ANY, F("setup(): Configure Focus stepper..."));
    mount.configureFocusStepper(FOCUSmotorPin1, FOCUSmotorPin2, FOCUS_STEPPER_SPEED, FOCUS_STEPPER_ACCELERATION);
    #if FOCUS_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    LOGV1(DEBUG_ANY, F("setup(): Configure Focus driver..."));
    LOGV3(DEBUG_FOCUS, F("setup(): RSense %f, RMS Current %fmA"), R_SENSE, FOCUS_RMSCURRENT);
        #if SW_SERIAL_UART == 0
    mount.configureFocusDriver(&FOCUS_SERIAL_PORT, R_SENSE, FOCUS_DRIVER_ADDRESS, FOCUS_RMSCURRENT, FOCUS_STALL_VALUE);
        #elif SW_SERIAL_UART == 1
    mount.configureFocusDriver(
        FOCUS_SERIAL_PORT_RX, FOCUS_SERIAL_PORT_TX, R_SENSE, FOCUS_DRIVER_ADDRESS, FOCUS_RMSCURRENT, FOCUS_STALL_VALUE);
        #endif
    #endif
#endif

    // The mount uses EEPROM storage locations 0-10 that it reads during construction
    // The LCD uses EEPROM storage location 11
    mount.readConfiguration();

    // Read other persisted values and set in mount
    DayTime haTime = EEPROMStore::getHATime();

    LOGV2(DEBUG_INFO, "SpeedCal: %s", String(mount.getSpeedCalibration(), 5).c_str());
    LOGV2(DEBUG_INFO, "TRKSpeed: %s", String(mount.getSpeed(TRACKING), 5).c_str());

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
                            2,                   // Priority (2 is higher than 1)
                            &StepperTask,        // The location that receives the thread id
                            0);                  // The core to run this on

#else
    // 2 kHz updates (higher frequency interferes with serial communications and complete messes up OATControl communications)
    if (!InterruptCallback::setInterval(0.5f, stepperControlTimerCallback, &mount))
    {
        LOGV1(DEBUG_MOUNT, F("CANNOT setup interrupt timer!"));
    }
#endif

#if UART_CONNECTION_TEST_TX == 1
    #if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    LOGV1(DEBUG_STEPPERS, "Moving RA axis using UART commands...");
    mount.testRA_UART_TX();
    LOGV1(DEBUG_STEPPERS, "Finished moving RA axis using UART commands.");
    #endif

    #if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    LOGV1(DEBUG_STEPPERS, "Moving DEC axis using UART commands...");
    mount.testDEC_UART_TX();
    LOGV1(DEBUG_STEPPERS, "Finished moving DEC axis using UART commands.");
    #endif
#endif

    // Start the tracker.
    LOGV1(DEBUG_ANY, F("Start Tracking..."));
    mount.startSlewing(TRACKING);

    mount.bootComplete();
    LOGV1(DEBUG_ANY, F("Boot complete!"));
}
