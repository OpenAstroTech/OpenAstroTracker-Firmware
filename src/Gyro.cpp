#include "../Configuration.hpp"
#include "Utility.hpp"
#include "Gyro.hpp"

#if USE_GYRO_LEVEL == 1

PUSH_NO_WARNINGS
#if USE_GYRO_WITH_SOFTWAREI2C == 1
#include "SlowSoftWire.h" // I2C communication library
SlowSoftWire Wire(GYRO_SOFTWARE_SDA_PIN, GYRO_SOFTWARE_SCL_PIN);
#else
#include <Wire.h> // I2C communication library
#endif
POP_NO_WARNINGS

// Tilt, roll, and temperature measurementusing the MPU-6050 MEMS gyro.
// See: https://invensense.tdk.com/products/motion-tracking/6-axis/mpu-6050/
// Datasheet: https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Datasheet1.pdf
// Register descriptions: https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Register-Map1.pdf

// Static var to keep track of whether the MPU6050 was found
bool Gyro::isPresent = false;

// Starts up the MPU-6050 device.
// Reads the WHO_AM_I register to verify if the device is present.
// Wakes device from power-down.
// Sets accelerometers to minimum bandwidth to reduce measurement noise.
void Gyro::startup()
{
// Initialize interface to the MPU6050
#if USE_GYRO_WITH_SOFTWAREI2C == 1
    LOGV1(DEBUG_INFO, F("GYRO:: Starting software I2C for MPU6050 comms."));
#else
    LOGV1(DEBUG_INFO, F("GYRO:: Starting hardware I2C for MPU6050 comms."));
#endif

    Wire.begin();
    Wire.setClock(100000); // Set lowest clock speed (100kHz) to make the communication more reliable

#if USE_GYRO_WITH_SOFTWAREI2C == 0
    Wire.setWireTimeout(3000, true); // Set I2C timeout if the Wire.h calls are hanging due to a HW issue
#endif
    // Execute 1 byte read from MPU6050_REG_WHO_AM_I
    // This is a read-only register which should have the value 0x68
    Wire.beginTransmission(MPU6050_I2C_ADDR);
    Wire.write(MPU6050_REG_WHO_AM_I);
    Wire.endTransmission(true);
    Wire.requestFrom(MPU6050_I2C_ADDR, 1, 1);
    const byte id = (Wire.read() >> 1) & 0x3F;
    isPresent = (id == 0x34);
    if (!isPresent)
    {
        LOGV2(DEBUG_INFO, F("GYRO:: Not found! Expected 0x34 but got %x"), id);
        return;
    }

    // Execute 1 byte write to MPU6050_REG_PWR_MGMT_1 to disable sleep and set 8MHz clock
    Wire.beginTransmission(MPU6050_I2C_ADDR);
    Wire.write(MPU6050_REG_PWR_MGMT_1);
    Wire.write(0); // Disable sleep, 8 MHz clock
    Wire.endTransmission(true);

    // Execute 1 byte read/write to MPU6050_REG_ACCEL_CONFIG to set 2g sensitivity
    Wire.beginTransmission(MPU6050_I2C_ADDR);
    Wire.write(MPU6050_REG_ACCEL_CONFIG);
    Wire.endTransmission();
    Wire.requestFrom(MPU6050_I2C_ADDR, 1);
    byte accelConfigReg = Wire.read();              //the value of Register-28 is in x
    accelConfigReg = (accelConfigReg & 0b11100111); //clear values of Bit4 and Bit3 fo 2g sensitivity
    Wire.beginTransmission(MPU6050_I2C_ADDR);
    Wire.write(MPU6050_REG_ACCEL_CONFIG);
    Wire.write(accelConfigReg);
    Wire.endTransmission();

    // Execute 1 byte write to MPU6050_REG_CONFIG to set slowest refresh rate
    Wire.beginTransmission(MPU6050_I2C_ADDR);
    Wire.write(MPU6050_REG_CONFIG);
    Wire.write(6); // 5Hz bandwidth (lowest) for smoothest readings
    Wire.endTransmission(true);

    LOGV1(DEBUG_INFO, F("GYRO:: Started"));
}

// Shuts down the MPU-6050 device.
// Currently does nothing.
void Gyro::shutdown()
{
    LOGV1(DEBUG_INFO, F("GYRO: Shutdown"));
    // Nothing to do
}

// Returns roll & tilt angles from MPU-6050 device in angle_t object in degrees.
// If MPU-6050 is not found then returns {0,0}.
angle_t Gyro::getCurrentAngles()
{
    // Read the accelerometer data
    struct angle_t result;
    result.pitchAngle = 0;
    result.rollAngle = 0;
    if (!isPresent)
    {
        return result; // Gyro is not available
    }

    // Execute 6 byte read from MPU6050_REG_WHO_AM_I
    Wire.beginTransmission(MPU6050_I2C_ADDR);
    Wire.write(MPU6050_REG_ACCEL_XOUT_H);
    Wire.endTransmission(false);
    Wire.requestFrom(MPU6050_I2C_ADDR, 6, 1); // Read 6 registers total, each axis value is stored in 2 registers
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
    result.pitchAngle += ((atanf(-1 * accelInX / sqrtf(powf(accelInY, 2) + powf(accelInZ, 2))) * 180.0f / static_cast<float>(PI)) * 2.0f) / 2.0f;

    // Calculating the Roll angle (rotation around X-axis)
    result.rollAngle += ((atanf(-1 * accelInY / sqrtf(powf(accelInX, 2) + powf(accelInZ, 2))) * 180.0f / static_cast<float>(PI)) * 2.0f) / 2.0f;

#if USE_GYRO_WITH_SOFTWAREI2C == 0
    if (Wire.getWireTimeoutFlag())
    {
        LOGV1(DEBUG_INFO, F("GYRO:: WARN: I2C Timeout."));
        Wire.clearWireTimeoutFlag();
    }
#endif

#if GYRO_AXIS_SWAP == 1
    float temp = result.pitchAngle;
    result.pitchAngle = result.rollAngle;
    result.rollAngle = temp;
#endif
    return result;
}

// Returns MPU-6050 device temperature in degree C.
// If MPU-6050 is not found then returns 99 (C).
float Gyro::getCurrentTemperature()
{
    if (!isPresent)
        return 99.0f; // Gyro is not available

    // Execute 2 byte read from MPU6050_REG_TEMP_OUT_H
    Wire.beginTransmission(MPU6050_I2C_ADDR);
    Wire.write(MPU6050_REG_TEMP_OUT_H);
    Wire.endTransmission(false);
    Wire.requestFrom(MPU6050_I2C_ADDR, 2, 1);           // Read 2 registers total, the temperature value is stored in 2 registers
    int16_t tempValue = Wire.read() << 8 | Wire.read(); // Raw Temperature value

    // Calculating the actual temperature value
    float result = static_cast<float>(tempValue) / 340.0f + 36.53f;
    return result;
}

#endif
