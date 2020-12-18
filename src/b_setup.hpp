// Create the LCD menu variable and initialize the LCD (16x2 characters)

#pragma once

#include "InterruptCallback.hpp"

#include "inc/Config.hpp"
#include "a_inits.hpp"
#include "LcdMenu.hpp"
#include "Utility.hpp"
#include "EPROMStore.hpp"

LcdMenu lcdMenu(16, 2, MAXMENUITEMS);
#if DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD
LcdButtons lcdButtons(LCD_KEY_SENSE_PIN, &lcdMenu);
#endif

#if DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23017 || DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23008 || DISPLAY_TYPE == DISPLAY_TYPE_LCD_JOY_I2C_SSD1306
LcdButtons lcdButtons(&lcdMenu);
#endif

#if defined(__AVR_ATmega2560__)
#if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
  SoftwareSerial RA_SERIAL_PORT(RA_SERIAL_PORT_RX, RA_SERIAL_PORT_TX);
#endif
#if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
  SoftwareSerial DEC_SERIAL_PORT(DEC_SERIAL_PORT_RX, DEC_SERIAL_PORT_TX);
#endif
#if AZIMUTH_ALTITUDE_MOTORS == 1
  #if AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    SoftwareSerial AZ_SERIAL_PORT(AZ_SERIAL_PORT_RX, AZ_SERIAL_PORT_TX);
  #endif
  #if ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    SoftwareSerial ALT_SERIAL_PORT(ALT_SERIAL_PORT_RX, ALT_SERIAL_PORT_TX);
  #endif
#endif
#endif

#ifdef ESP32
DRAM_ATTR Mount mount(RA_STEPS_PER_DEGREE, DEC_STEPS_PER_DEGREE, &lcdMenu);
#else
Mount mount(RA_STEPS_PER_DEGREE, DEC_STEPS_PER_DEGREE, &lcdMenu);
#endif

#include "g_bluetooth.hpp"

#ifdef WIFI_ENABLED
#include "WifiControl.hpp"
WifiControl wifiControl(&mount, &lcdMenu);
#endif

/////////////////////////////////
//   Interrupt handling
/////////////////////////////////
/* There are three possible configurations for periodically servicing the steper drives:
 * 1) If RUN_STEPPERS_IN_MAIN_LOOP != 0 then the stepper drivers are called from Mount::loop().
 *    Performance depends on how fast Mount::loop() can execute. With serial or UI activity it 
 *    is likely that steps will be missed and tracking may not be smooth. No interrupts or threads
 *    are used, so this is simple to get running.
 * 2) If ESP32 is #defined then a periodic task is assigned to Core 0 to service the steppers.
 *    stepperControlTask() is scheduled to run every 1 ms (1 kHz rate). On ESP32 the default Arduino 
 *    loop() function runs on Core 1, therefore serial and UI activity also runs on Core 1. 
 *    Note that Wifi and Bluetooth drivers will be sharing Core 0 with stepperControlTask().
 *    This configuration decouples stepper servicing from other OAT activities by using both cores.
 * 3) By default (e.g. for ATmega2560) a periodic timer is configured for a 500 us (2 kHz rate interval).
 *    This timr generates interrupts which are handled by stepperControlCallback(). The stepper 
 *    servicing therefore suspends loop() to generate motion, ensuring smooth tracking.
 */
#if (RUN_STEPPERS_IN_MAIN_LOOP != 0)
  // Nothing to do - Mount::loop() will manage steppers in-line

#elif defined(ESP32)

TaskHandle_t StepperTask;

// This is the task for simulating periodic interrupts on ESP32 platforms. 
// It should do very minimal work, only calling Mount::interruptLoop() to step the stepper motors as needed.
// This task function is run on Core 0 of the ESP32 and never returns
void IRAM_ATTR stepperControlTask(void* payload)
{
  Mount* mount = reinterpret_cast<Mount*>(payload);
  for (;;) {
    mount->interruptLoop();
    vTaskDelay(1);  // 1 ms 	// This will limit max stepping rate to 1 kHz
  }
}

#else
// This is the callback function for the timer interrupt on ATMega platforms. 
// It should do very minimal work, only calling Mount::interruptLoop() to step the stepper motors as needed.
// It is called every 500 us (2 kHz rate)
void stepperControlTimerCallback(void* payload) {
  Mount* mount = reinterpret_cast<Mount*>(payload);
  if (mount)
    mount->interruptLoop();
}

#endif

/////////////////////////////////
//
// Main program setup 
//
/////////////////////////////////
void setup() {

  #if USE_GPS == 1
  GPS_SERIAL_PORT.begin(GPS_BAUD_RATE);
  #endif

  //Turn on dew heater
  #if DEW_HEATER == 1
  digitalWrite(DEW_HEATER_PIN, HIGH);
  #endif

  /////////////////////////////////
  //   Microstepping/driver pins
  /////////////////////////////////
  #if RA_STEPPER_TYPE == STEPPER_TYPE_NEMA17  // RA driver startup (for A4988)
    #if RA_DRIVER_TYPE == DRIVER_TYPE_GENERIC
      // include A4988 microstep pins
      //#error "Define Microstep pins and delete this error."
      digitalWrite(RA_EN_PIN, HIGH);
      digitalWrite(RA_MS0_PIN, HIGH);  // MS0
      digitalWrite(RA_MS1_PIN, HIGH);  // MS1
      digitalWrite(RA_MS2_PIN, HIGH);  // MS2
      #endif
    #if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE
      // include TMC2209 Standalone pins
      pinMode(RA_EN_PIN, OUTPUT);
      digitalWrite(RA_EN_PIN, LOW);  // ENABLE, LOW to enable
      // Microstepping control via MS pins - TODO -----------------------
      //digitalWrite(RA_MS0_PIN, HIGH);  // MS0
      //digitalWrite(RA_MS1_PIN, HIGH);  // MS1
      //digitalWrite(RA_MS2_PIN, HIGH);  // MS2
      #endif
    #if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
      // include TMC2209 UART pins  
      pinMode(RA_EN_PIN, OUTPUT);
      pinMode(RA_DIAG_PIN, INPUT);
      digitalWrite(RA_EN_PIN, LOW);  // Logic LOW to enable driver
      RA_SERIAL_PORT.begin(57600);  // Start HardwareSerial comms with driver
    #endif
  #endif
  #if DEC_STEPPER_TYPE == STEPPER_TYPE_NEMA17  // DEC driver startup (for A4988)
    #if DEC_DRIVER_TYPE == DRIVER_TYPE_GENERIC  // DEC driver startup (for A4988)
      digitalWrite(DEC_EN_PIN, HIGH);
      digitalWrite(DEC_MS0_PIN, HIGH);  // MS1
      digitalWrite(DEC_MS1_PIN, HIGH);  // MS2
      digitalWrite(DEC_MS2_PIN, HIGH);  // MS3
    #endif
    #if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE
      // include TMC2209 Standalone pins
      pinMode(DEC_EN_PIN, OUTPUT);
      digitalWrite(DEC_EN_PIN, LOW);  // ENABLE, LOW to enable
      // Microstepping control via MS pins - TODO -----------------------
      //digitalWrite(DEC_MS0_PIN, HIGH);  // MS0
      //digitalWrite(DEC_MS1_PIN, HIGH);  // MS1
      //digitalWrite(DEC_MS2_PIN, HIGH);  // MS2
    #endif
    #if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
      // include TMC2209 UART pins  
      pinMode(DEC_EN_PIN, OUTPUT);
      pinMode(DEC_DIAG_PIN, INPUT);
      //pinMode(DEC_MS1_PIN, OUTPUT);
      digitalWrite(DEC_EN_PIN, LOW);  // Logic LOW to enable driver
      //digitalWrite(DEC_MS1_PIN, HIGH); // Logic HIGH to MS1 to get 0b01 address
      DEC_SERIAL_PORT.begin(57600);  // Start HardwareSerial comms with driver
    #endif
  #endif
  
  #if AZIMUTH_ALTITUDE_MOTORS == 1  
    #if AZ_DRIVER_TYPE == DRIVER_TYPE_GENERIC || AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE || AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART  
      pinMode(AZ_EN_PIN, OUTPUT);
      digitalWrite(AZ_EN_PIN, HIGH);  // Logic HIGH to disable the driver initally
    #endif
    #if AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
      // include TMC2209 UART pins
      pinMode(AZ_DIAG_PIN, INPUT);
      AZ_SERIAL_PORT.begin(57600);  // Start HardwareSerial comms with driver
    #endif
    #if ALT_DRIVER_TYPE == DRIVER_TYPE_GENERIC || ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE || ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART  
      pinMode(ALT_EN_PIN, OUTPUT);
      digitalWrite(ALT_EN_PIN, HIGH);  // Logic HIGH to disable the driver initally
    #endif
    #if ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
      // include TMC2209 UART pins
      pinMode(ALT_DIAG_PIN, INPUT);
      ALT_SERIAL_PORT.begin(57600);  // Start HardwareSerial comms with driver
    #endif
  #endif
// end microstepping -------------------

  Serial.begin(SERIAL_BAUDRATE);

  #ifdef BLUETOOTH_ENABLED 
  BLUETOOTH_SERIAL.begin("OpenAstroTracker");
  #endif

  LOGV1(DEBUG_ANY, F("."));
  LOGV2(DEBUG_ANY, F("Hello, universe, this is OAT %s!"), VERSION);

  EEPROMStore::initialize();

  // Calling the LCD startup here, I2C can't be found if called earlier
  #if DISPLAY_TYPE > 0
    lcdMenu.startup();

    LOGV1(DEBUG_ANY, F("Finishing boot..."));
    // Show a splash screen
    lcdMenu.setCursor(0, 0);
    lcdMenu.printMenu("OpenAstroTracker");
    lcdMenu.setCursor(5, 1);
    lcdMenu.printMenu(VERSION);
    delay(1000);  // Pause on splash screen

    // Check for EEPROM reset (Button down during boot)
    if (lcdButtons.currentState() == btnDOWN){
      LOGV1(DEBUG_INFO, F("Erasing configuration in EEPROM!"));
      mount.clearConfiguration();
      // Wait for button release
      lcdMenu.setCursor(13, 1);
      lcdMenu.printMenu("CLR");
      while (lcdButtons.currentState() != btnNONE) {
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

    #if SUPPORT_INFO_DISPLAY == 1
      lcdMenu.addItem("INFO", Status_Menu);
    #endif

  #endif // DISPLAY_TYPE > 0
  
  LOGV2(DEBUG_ANY, F("Hardware: %s"), mount.getMountHardwareInfo().c_str());

  // Create the command processor singleton
  LOGV1(DEBUG_ANY, F("Initialize LX200 handler..."));
  MeadeCommandProcessor::createProcessor(&mount, &lcdMenu);

  #ifdef WIFI_ENABLED
    LOGV1(DEBUG_ANY, F("Setup Wifi..."));
    wifiControl.setup();
  #endif

  // Configure the mount
  // Delay for a while to get UARTs booted...
  delay(1000);  

  // Set the stepper motor parameters
  #if RA_STEPPER_TYPE == STEPPER_TYPE_28BYJ48 
    LOGV1(DEBUG_ANY, "Configure RA stepper 28BYJ-48...");
    mount.configureRAStepper(FULLSTEP_MODE, RAmotorPin1, RAmotorPin2, RAmotorPin3, RAmotorPin4, RA_STEPPER_SPEED, RA_STEPPER_ACCELERATION);
  #elif RA_STEPPER_TYPE == STEPPER_TYPE_NEMA17
    LOGV1(DEBUG_ANY, F("Configure RA stepper NEMA..."));
    mount.configureRAStepper(DRIVER_MODE, RAmotorPin1, RAmotorPin2, RA_STEPPER_SPEED, RA_STEPPER_ACCELERATION);
  #else
    #error New stepper type? Configure it here.
  #endif

  #if DEC_STEPPER_TYPE == STEPPER_TYPE_28BYJ48
    LOGV1(DEBUG_ANY, F("Configure DEC stepper 28BYJ-48..."));
    mount.configureDECStepper(HALFSTEP_MODE, DECmotorPin1, DECmotorPin2, DECmotorPin3, DECmotorPin4, RA_STEPPER_SPEED, DEC_STEPPER_ACCELERATION);
  #elif DEC_STEPPER_TYPE == STEPPER_TYPE_NEMA17
    LOGV1(DEBUG_ANY, F("Configure DEC stepper NEMA..."));
    mount.configureDECStepper(DRIVER_MODE, DECmotorPin1, DECmotorPin2, DEC_STEPPER_SPEED, DEC_STEPPER_ACCELERATION);
  #else
    #error New stepper type? Configure it here.
  #endif

  #if RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    LOGV1(DEBUG_ANY, F("Configure RA driver TMC2209 UART..."));
    mount.configureRAdriver(&RA_SERIAL_PORT, R_SENSE, RA_DRIVER_ADDRESS, RA_RMSCURRENT, RA_STALL_VALUE);
  #endif
  #if DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    LOGV1(DEBUG_ANY, F("Configure DEC driver TMC2209 UART..."));
    mount.configureDECdriver(&DEC_SERIAL_PORT, R_SENSE, DEC_DRIVER_ADDRESS, DEC_RMSCURRENT, DEC_STALL_VALUE);
  #endif

  #if AZIMUTH_ALTITUDE_MOTORS == 1
    LOGV1(DEBUG_ANY, F("Configure AZ stepper..."));
    #if AZ_DRIVER_TYPE == DRIVER_TYPE_ULN2003 
      #if AZ_MICROSTEPPING == 1
         mount.configureAZStepper(FULLSTEP_MODE, AZmotorPin1, AZmotorPin2, AZmotorPin3, AZmotorPin4, AZ_STEPPER_SPEED, AZ_STEPPER_ACCELERATION);
      #elif AZ_MICROSTEPPING == 2
         mount.configureAZStepper(HALFSTEP_MODE, AZmotorPin1, AZmotorPin2, AZmotorPin3, AZmotorPin4, AZ_STEPPER_SPEED, AZ_STEPPER_ACCELERATION);
      #endif
    #elif AZ_DRIVER_TYPE == DRIVER_TYPE_GENERIC || AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE || AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
      mount.configureAZStepper(DRIVER_MODE, AZmotorPin1, AZmotorPin2, AZ_STEPPER_SPEED, AZ_STEPPER_ACCELERATION);
    #endif
    #if AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
      LOGV1(DEBUG_ANY, F("Configure AZ driver..."));
      mount.configureAZdriver(&AZ_SERIAL_PORT, R_SENSE, AZ_DRIVER_ADDRESS, AZ_RMSCURRENT, AZ_STALL_VALUE);
    #endif
    LOGV1(DEBUG_ANY, F("Configure Alt stepper..."));
    #if ALT_DRIVER_TYPE == DRIVER_TYPE_ULN2003 
      #if ALT_MICROSTEPPING == 1
        mount.configureALTStepper(FULLSTEP_MODE, ALTmotorPin1, ALTmotorPin2, ALTmotorPin3, ALTmotorPin4, ALT_STEPPER_SPEED, ALT_STEPPER_ACCELERATION);
      #elif ALT_MICROSTEPPING == 2
        mount.configureALTStepper(HALFSTEP_MODE, ALTmotorPin1, ALTmotorPin2, ALTmotorPin3, ALTmotorPin4, ALT_STEPPER_SPEED, ALT_STEPPER_ACCELERATION);
      #endif
    #elif ALT_DRIVER_TYPE == DRIVER_TYPE_GENERIC || ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE || ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
      mount.configureALTStepper(DRIVER_MODE, ALTmotorPin1, ALTmotorPin2, ALT_STEPPER_SPEED, ALT_STEPPER_ACCELERATION);
    #endif
    #if ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
      LOGV1(DEBUG_ANY, F("Configure ALT driver..."));
      mount.configureALTdriver(&ALT_SERIAL_PORT, R_SENSE, ALT_DRIVER_ADDRESS, ALT_RMSCURRENT, ALT_STALL_VALUE);
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
  #if (RUN_STEPPERS_IN_MAIN_LOOP != 0)
    // Nothing to do - Mount::loop() will manage steppers in-line

  #elif defined(ESP32)

    disableCore0WDT();
    xTaskCreatePinnedToCore(
      stepperControlTask,    // Function to run on this core
      "StepperControl",      // Name of this task
      32767,                 // Stack space in bytes
      &mount,                // payload
      2,                     // Priority (2 is higher than 1)
      &StepperTask,          // The location that receives the thread id
      0);                    // The core to run this on
      
  #else
    // 2 kHz updates (higher frequency interferes with serial communications and complete messes up OATControl communications)
    if (!InterruptCallback::setInterval(0.5f, stepperControlTimerCallback, &mount))
    {
      LOGV1(DEBUG_MOUNT, F("CANNOT setup interrupt timer!"));
    }
  #endif

  // Start the tracker.
  LOGV1(DEBUG_ANY, F("Start Tracking..."));
  mount.startSlewing(TRACKING);

  mount.bootComplete();
  LOGV1(DEBUG_ANY, F("Boot complete!"));
}
