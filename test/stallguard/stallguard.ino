#define USE_TIMER_1     true
#define USE_TIMER_2     true
#define USE_TIMER_3     false
#define USE_TIMER_4     false
#define USE_TIMER_5     false

#include "TimerInterrupt2.h"

#include <TMCStepper.h>
#include <AccelStepper.h>

#define MAX_SPEED      2000
#define MIN_SPEED      100

int STALL_VALUE    = 10; // [0..255]

//////////////////////////////
// RA - MEGA
//#define DIAG_PIN         28 // Diag
//#define EN_PIN           26 // Enable
//#define DIR_PIN          24 // Direction
//#define STEP_PIN         22 // Step
//#define DRIVER_ADDRESS 0b00 // TMC2209 Driver address according to MS1 and MS2
//
//////////////////////////////
// RA - MKS LV21
#define DIAG_PIN         3 // Diag
#define EN_PIN           38 // Enable
#define DIR_PIN          55 // Direction
#define STEP_PIN         54 // Step
#define DRIVER_ADDRESS 0b00 // TMC2209 Driver address according to MS1 and MS2

//////////////////////////////
// DEC
//#define DIAG_PIN         29 // Diag
//#define EN_PIN           27 // Enable
//#define DIR_PIN          25 // Direction
//#define STEP_PIN         23 // Step
//#define DRIVER_ADDRESS 0b01 // TMC2209 Driver address according to MS1 and MS2

// MKS Software Serial
#define RA_SERIAL_PORT_TX 40 // SoftwareSerial TX port
#define RA_SERIAL_PORT_RX 63 // SoftwareSerial RX port

// #define SERIAL_PORT Serial2 // TMC2208/TMC2224 HardwareSerial port

#define R_SENSE 0.11f // Match to your driver
// SilentStepStick series use 0.11
// UltiMachine Einsy and Archim2 boards use 0.2
// Panucatt BSD2660 uses 0.1
// Watterott TMC5160 uses 0.075

// Select your stepper driver type
//TMC2209Stepper driver(&SERIAL_PORT, R_SENSE, DRIVER_ADDRESS);
TMC2209Stepper driver(RA_SERIAL_PORT_RX, RA_SERIAL_PORT_TX, R_SENSE, DRIVER_ADDRESS);
AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

long volatile calls=0;
float rms = 350;
int microstepping = 2;

int diagPinLevel  = 0;
bool diagTripped=false;
int maxSGResult = 0;

void stepperControlTimerCallback(void* payload) 
{
  stepper.runSpeed();
  
  diagPinLevel = digitalRead(DIAG_PIN);
  if (diagPinLevel == HIGH){
    diagTripped=true;
  }
  
  calls++;
}

void initDriver() { 
  
  driver.begin();

  Serial.println(driver.test_connection() == 0? "UART Connected" : "UART NOT Connected");

  driver.toff(0);
  driver.rms_current(rms, 1.0); // mA
  driver.en_spreadCycle(0); // Must use StealthChop mode for stallguard
  driver.toff(3);
  driver.blank_time(24);
  driver.fclktrim(4); 
  driver.TCOOLTHRS(0xFFFFF); // 
  driver.semin(0); // No CoolStep
  driver.SGTHRS(STALL_VALUE);



  driver.mstep_reg_select(1); // Microsteps programmable
  driver.pdn_disable(1);

//  driver.I_scale_analog(0);
//  driver.sedn(0b01);
 
  driver.microsteps(microstepping); // FULL STEPS
}

void setup() {
  Serial.begin(57600);         // Init serial port and set baudrate
  while (!Serial)
    ;               // Wait for serial port to connect
  Serial.println("\nStart...");

  //SERIAL_PORT.begin(19200);
  driver.beginSerial(19200);

  pinMode(DIAG_PIN, INPUT);
  pinMode(EN_PIN, OUTPUT);
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  digitalWrite(EN_PIN, LOW);
  digitalWrite(DIAG_PIN, LOW);

  initDriver();

  stepper.setMaxSpeed(MAX_SPEED);

  ITimer2.init();
  
  // This timer supports the callback with payload
  if (!ITimer2.attachInterruptInterval<void*>(0.5f, stepperControlTimerCallback, NULL, 0UL)) {
        Serial.println("Interrupt handler...FAILED");
  }
  else
  {
        Serial.println("Interrupt handler...OK");
  }

  ITimer2.restartTimer();  
  
  Serial.println("Keys: ");
  Serial.println("  !  : Re-init driver");
  Serial.println("  0  : Stop");
  Serial.println("  1  : Start");
  Serial.println(" ] [ : Increase/Decrease microstepping");
  Serial.println(" g h : Increase/Decrease RMS current by 10mA");
  Serial.println(" G H : Increase/Decrease RMS current by 100mA");
  Serial.println(" + - : Increase/Decrease Speed by 200");
  Serial.println(" < > : Turn RA ring Clockwise/Counterclockwise");
  Serial.println(" a z : Increase/Decrease Stall Value by 2");
}



int dir = -1;
int mode = 0;
int speed=500;
uint32_t restart=millis();

void loop() {
  static uint32_t last_time = 0;
  uint32_t ms = millis();

  while (Serial.available() > 0) {
    int8_t read_byte = Serial.read();

    switch (read_byte)
    {
      case '!': 
      {
        initDriver();
      }
      break;

      case '0': 
      {
        stepper.stop();
      }
      break;
    
      case '1':
      {
        stepper.setSpeed(dir*speed);
        restart=millis();
      }
      break;

      case ']': 
      {
        microstepping++;
        if (microstepping>8) microstepping=8;
        
        driver.microsteps(1<<microstepping);
      }
      break;

      case '[': 
      {
        microstepping--;
        if (microstepping<0) microstepping=0;
        driver.microsteps(1<<microstepping);
      }
      break;

      case 'g': 
      {
        rms-=10;
        driver.rms_current(rms, 1.0); // mA
        Serial.println("rms: "+ String(driver.rms_current())+"mA ");
      }
      break;
      
      case 'h': 
      {
        rms+=10;
        driver.rms_current(rms, 1.0); // mA
        Serial.println("rms: "+ String(driver.rms_current())+"mA ");
      }
      break;

      case 'G': 
      {
        rms-=100;
        driver.rms_current(rms, 1.0); // mA
        Serial.println("rms: "+ String(driver.rms_current())+"mA ");
      }
      break;
      
      case 'H': 
      {
        rms+=100;
        driver.rms_current(rms, 1.0); // mA
        Serial.println("rms: "+ String(driver.rms_current())+"mA ");
      }
      break;

      case '+':
      {
        speed+=200;
        stepper.setSpeed(dir*speed);
      }
      break;
      case '-':
      {
        speed-=200;
        stepper.setSpeed(dir*speed);
      }
      break;
      case '<':
      {
        dir=-1;
        stepper.setSpeed(dir*speed);
        restart=millis();
      }
      break;
      case '>':
      {
        dir=1;
        stepper.setSpeed(dir*speed);
        restart=millis();
      }
      break;
      case 'a':
      {
        STALL_VALUE+=2;    
        if (STALL_VALUE>255){
          STALL_VALUE=255;
        }
        driver.SGTHRS(STALL_VALUE);         
      }
      break;
      case 'z':
      {
        STALL_VALUE-=2; 
        if (STALL_VALUE<1){
          STALL_VALUE=1;
        }
        driver.SGTHRS(STALL_VALUE);         
      }
      break;
    }
  }

  // Keep track of the highest SG result between printouts
  int sg = driver.SG_RESULT();
  if (sg>maxSGResult) maxSGResult=sg;
  
  if ((ms - last_time) > 100) { //run every 0.1s
  
    if (millis() > restart+1000){ // Check whether DIAG tripped one second after restarting the motor
      if (diagTripped){
        Serial.print("Tripped");
        diagTripped=false;
        stepper.stop();
        stepper.setSpeed(0);
      }
    }
  
      last_time = ms;
    Serial.print("Conn:");
    Serial.print(driver.test_connection());
    Serial.print(" Speed:");
    Serial.print(speed*dir, DEC);
    Serial.print("  MS:");
    Serial.print(1 << microstepping);
    Serial.print("  rms:");
    Serial.print(rms);
    Serial.print("  Stall:");
    Serial.print(STALL_VALUE, DEC);
    Serial.print("  SG:");
    Serial.print(maxSGResult);
    maxSGResult=0;
    Serial.print("  DiagPin:");
    Serial.print(diagPinLevel, DEC);
    Serial.print("  DiagTripped: ");
    Serial.print(diagTripped, DEC);
    //Serial.print(" ");
    //Serial.print(driver.cs2rms(driver.cs_actual()), DEC);
    //Serial.print(calls);
	Serial.println();
  }
}
