#define SW_I2C 



#ifdef SW_I2C
#include <AsyncDelay.h>   // I2C communication library
#include <SlowSoftWire.h>   // I2C communication library

//#define GYRO_SOFTWARE_SCL_PIN 21
//#define GYRO_SOFTWARE_SDA_PIN 20

#define GYRO_SOFTWARE_SCL_PIN 11
#define GYRO_SOFTWARE_SDA_PIN 21

SlowSoftWire Wire(GYRO_SOFTWARE_SDA_PIN, GYRO_SOFTWARE_SCL_PIN);

char wireTxBuffer[16];
char wireRxBuffer[16];
#else

#include <Wire.h> // I2C communication library

#endif

bool isPresent = false;

enum {
  MPU6050_I2C_ADDR = 0x68,  // I2C address of the MPU6050 accelerometer

  // Register addresses
  MPU6050_REG_CONFIG = 0x1A,
  MPU6050_REG_ACCEL_CONFIG = 0x1C,
  MPU6050_REG_ACCEL_XOUT_H = 0x3B,
  MPU6050_REG_TEMP_OUT_H = 0x41,
  MPU6050_REG_PWR_MGMT_1 = 0x6B,
  MPU6050_REG_WHO_AM_I = 0x75
};

char achBuffer[100];


void setup() {
  // put your setup code here, to run once:
  Serial.begin(57600);
  while (!Serial)
    ;
  Serial.println("\nStart...");

    #ifdef SW_I2C
//    Wire.setTxBuffer(wireTxBuffer, sizeof(wireTxBuffer));
//    Wire.setRxBuffer(wireRxBuffer, sizeof(wireRxBuffer));
//    Wire.setDelay_us(5);
//    Wire.setTimeout(1000);
  #endif
    Wire.begin();

  // Execute 1 byte read from MPU6050_REG_WHO_AM_I
  // This is a read-only register which should have the value 0x68
  Wire.beginTransmission(MPU6050_I2C_ADDR);
  Wire.write(MPU6050_REG_WHO_AM_I);
  Wire.endTransmission(true);
  Wire.requestFrom(MPU6050_I2C_ADDR, 1, 1);
  byte id = Wire.read();
  Serial.println("GYRO:: Read byte from MP6050: "+String(id, HEX));
  id = (id >> 1) & 0x3F;
  isPresent = (id == 0x34);
  if (!isPresent)
  {
      Serial.println("GYRO:: Not found! Expected 0x34 but got "+String(id, HEX));
      return;
  }

  // Execute 1 byte write to MPU6050_REG_PWR_MGMT_1
  Wire.beginTransmission(MPU6050_I2C_ADDR);
  Wire.write(MPU6050_REG_PWR_MGMT_1);
  Wire.write(0); // Disable sleep, 8 MHz clock
  Wire.endTransmission(true);

  // Execute 1 byte write to MPU6050_REG_ACCEL_CONFIG to set 4g sensititvity
  Wire.beginTransmission(MPU6050_I2C_ADDR);
  Wire.write(MPU6050_REG_ACCEL_CONFIG);
  Wire.endTransmission();
  Wire.requestFrom(MPU6050_I2C_ADDR, 1);
  byte x = Wire.read(); //the value of Register-28 is in x
  x = (x & 0b11100111) | 0b00000000;     //appending values of Bit4 and Bit3
  
  Wire.beginTransmission(MPU6050_I2C_ADDR);
  Wire.write(MPU6050_REG_ACCEL_CONFIG);
  Wire.write(x);
  Wire.endTransmission();

  // Execute 1 byte write to MPU6050_REG_PWR_MGMT_1
  Wire.beginTransmission(MPU6050_I2C_ADDR);
  Wire.write(MPU6050_REG_CONFIG);
  Wire.write(6); // 5Hz bandwidth (lowest) for smoothing
  Wire.endTransmission(true);

  Serial.println("GYRO:: Started");
}

String padString(float f, int places) {
  String res = String(f, places);
  while (res.length()<8){
    res=String(' ')+res;
  }
  return res;
}

String padString(int16_t i) {
  String res = String(i);
  while (res.length()<8){
    res=String(' ')+res;
  }
  return res;
}


const int window = 1;
void loop() {
  if (!isPresent)
    return; // Gyro is not available

  float ax=0,ay=0,az=0;
  float rollAngle=0;
  float pitchAngle=0;
  float deltaPitch=0;
  float maxDeltaPitch=0;
  float lastPitch=0, temp;
  memset(achBuffer, 32, 99);
  achBuffer[99]=0;
    
  for (int i=0; i<window; i++)
  {
    // Execute 6 byte read from MPU6050_REG_WHO_AM_I
    Wire.beginTransmission(MPU6050_I2C_ADDR);
    Wire.write(MPU6050_REG_ACCEL_XOUT_H);
    Wire.endTransmission(false);
    
    Wire.requestFrom(MPU6050_I2C_ADDR, 6, 1);     // Read 6 registers total, each axis value is stored in 2 registers
    const uint8_t accelXMSByte = Wire.read();
    const uint8_t accelXLSByte = Wire.read();
    const uint8_t accelYMSByte = Wire.read();
    const uint8_t accelYLSByte = Wire.read();
    const uint8_t accelZMSByte = Wire.read();
    const uint8_t accelZLSByte = Wire.read();

    const int16_t accelInX = accelXMSByte << 8 | accelXLSByte; // X-axis value
    const int16_t accelInY = accelYMSByte << 8 | accelYLSByte; // Y-axis value
    const int16_t accelInZ = accelZMSByte << 8 | accelZLSByte; // Z-axis value

    // Calculating the Pitch angle (rotation around Y-axis)
    temp = ((atanf(-1 * accelInX / sqrtf(powf(accelInY, 2) + powf(accelInZ, 2))) * 180.0f / static_cast<float>(PI)) * 2.0f) / 2.0f;
    if (i>0){
      
      deltaPitch=temp-lastPitch;
      if (fabs(deltaPitch)>maxDeltaPitch)
      {
        maxDeltaPitch=deltaPitch;
      }
      lastPitch=temp;
    }
    pitchAngle += temp;
    // Calculating the Roll angle (rotation around X-axis)
    rollAngle += ((atanf(-1 * accelInY / sqrtf(powf(accelInX, 2) + powf(accelInZ, 2))) * 180.0f / static_cast<float>(PI)) * 2.0f) / 2.0f;  
    //Serial.println("Accel : "+padString(pitchAngle,2)+ "  Roll:"+padString(rollAngle,2)+ "  Roll:"+padString(pitchAngle,3));

  }
  
  pitchAngle /= window;
  rollAngle /=window;
  
  achBuffer[45]='|';
  pitchAngle=45+pitchAngle*20;
  if (pitchAngle<0) pitchAngle=0;
  if (pitchAngle>98) pitchAngle=98;
  achBuffer[(int)(pitchAngle)]='*';
  Serial.print(achBuffer);
  Serial.println(String(maxDeltaPitch*1000,1));
  
  Wire.beginTransmission(MPU6050_I2C_ADDR);
  Wire.write(MPU6050_REG_TEMP_OUT_H);
  Wire.endTransmission(false);
  
  Wire.requestFrom(MPU6050_I2C_ADDR, 2, 1);
  int8_t th =  Wire.read();
  int8_t tl =  Wire.read();
  int16_t t = 256 * th;
  t = t + tl;
  temp = ((float)t/340.0) + 36.53;
  //Serial.println("Accel : "+padString(ax,1)+" "+padString(ay,1)+ " Roll:"+padString(rollAngle,3) +"    Temp: "+padString(temp,1)+"C");
  
  //Serial.println("Accel Bytes: "+String(b0,HEX)+" "+String(b1,HEX)+" "+String(b2,HEX)+" "+String(b3,HEX)+" "+String(b4,HEX)+" "+String(b5,HEX));
  //Serial.println("Temp: "+String(temp,1)+"C Bytes: "+String(th,HEX)+" "+String(tl,HEX));
 
}
