#include "../Configuration.hpp"
#include "Utility.hpp"
#include "Gyro.hpp"

#if USE_GYRO_LEVEL == 1

PUSH_NO_WARNINGS
    #include <Wire.h>  // I2C communication library
POP_NO_WARNINGS

/**
 * Tilt, roll, and temperature measurementusing the MPU-6050 MEMS gyro.
 * See: https://invensense.tdk.com/products/motion-tracking/6-axis/mpu-6050/
 * Datasheet: https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Datasheet1.pdf
 * Register descriptions: https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Register-Map1.pdf
 * */

bool Gyro::isPresent(false);
float Gyro::_pitchEma(0);
float Gyro::_rollEma(0);
bool Gyro::_initialized(false);
unsigned long Gyro::_lastSampleTime(0);

void Gyro::startup()
/* Starts up the MPU-6050 device.
   Reads the WHO_AM_I register to verify if the device is present.
   Wakes device from power-down.
   Sets accelerometers to minimum bandwidth to reduce measurement noise.
*/
{
    // Initialize interface to the MPU6050
    LOG(DEBUG_INFO, "[GYRO]:: Starting");
    Wire.begin();

    // Execute 1 byte read from MPU6050_REG_WHO_AM_I
    // This is a read-only register which should have the value 0x68
    Wire.beginTransmission(MPU6050_I2C_ADDR);
    Wire.write(MPU6050_REG_WHO_AM_I);
    Wire.endTransmission(true);
    Wire.requestFrom(MPU6050_I2C_ADDR, 1, 1);
    byte id   = (Wire.read() >> 1) & 0x3F;
    isPresent = (id == 0x34);
    if (!isPresent)
    {
        LOG(DEBUG_INFO, "[GYRO]:: Not found!");
        return;
    }

    // Execute 1 byte write to MPU6050_REG_PWR_MGMT_1
    Wire.beginTransmission(MPU6050_I2C_ADDR);
    Wire.write(MPU6050_REG_PWR_MGMT_1);
    Wire.write(0);  // Disable sleep, 8 MHz clock
    Wire.endTransmission(true);

    // Execute 1 byte write to MPU6050_REG_CONFIG
    Wire.beginTransmission(MPU6050_I2C_ADDR);
    Wire.write(MPU6050_REG_CONFIG);
    Wire.write(6);  // 5Hz bandwidth (lowest) for smoothing
    Wire.endTransmission(true);

    _initialized     = false;
    _pitchEma        = 0;
    _rollEma         = 0;
    _lastSampleTime  = 0;

    LOG(DEBUG_INFO, "[GYRO]:: Started");
}

void Gyro::shutdown()
/* Shuts down the MPU-6050 device.
   Currently does nothing.
*/
{
    LOG(DEBUG_INFO, "[GYRO]: Shutdown");
    // Nothing to do
}

void Gyro::collectSample()
/* Reads one accelerometer sample and updates the EMA state.
*/
{
    // Execute 6 byte read from MPU6050_REG_ACCEL_XOUT_H
    Wire.beginTransmission(MPU6050_I2C_ADDR);
    Wire.write(MPU6050_REG_ACCEL_XOUT_H);
    Wire.endTransmission(false);
    Wire.requestFrom(MPU6050_I2C_ADDR, 6, 1);      // Read 6 registers total, each axis value is stored in 2 registers
    int16_t AcX = Wire.read() << 8 | Wire.read();  // X-axis value
    int16_t AcY = Wire.read() << 8 | Wire.read();  // Y-axis value
    int16_t AcZ = Wire.read() << 8 | Wire.read();  // Z-axis value

    // Calculating the Pitch angle (rotation around Y-axis)
    float pitch = atan2f(-1.0f * AcX, sqrtf((float) AcY * AcY + (float) AcZ * AcZ)) * 180.0f / static_cast<float>(PI);
    // Calculating the Roll angle (rotation around X-axis)
    float roll = atan2f(-1.0f * AcY, sqrtf((float) AcX * AcX + (float) AcZ * AcZ)) * 180.0f / static_cast<float>(PI);

    if (!_initialized)
    {
        _pitchEma   = pitch;
        _rollEma    = roll;
        _initialized = true;
    }
    else
    {
        _pitchEma = ((1.0f - EMA_ALPHA) * _pitchEma) + (EMA_ALPHA * pitch);
        _rollEma  = ((1.0f - EMA_ALPHA) * _rollEma) + (EMA_ALPHA * roll);
    }
}

angle_t Gyro::getCurrentAngles()
/* Returns roll & tilt angles from MPU-6050 device in angle_t object in degrees.
   Non-blocking: collects one sample per call if at least SAMPLE_INTERVAL ms
   have elapsed since the last sample. Returns the EMA-filtered angles.
*/
{
    angle_t result;

    if (!isPresent)
        return result;

    unsigned long now = millis();
    if (now - _lastSampleTime >= SAMPLE_INTERVAL)
    {
        _lastSampleTime = now;
        collectSample();
    }

    if (!_initialized)
        return result;

    result.pitchAngle = _pitchEma;
    result.rollAngle  = _rollEma;

    #if GYRO_AXIS_SWAP == 1
    float temp        = result.pitchAngle;
    result.pitchAngle = result.rollAngle;
    result.rollAngle  = temp;
    #endif

    return result;
}

float Gyro::getCurrentTemperature()
/* Returns MPU-6050 device temperature in degree C.
   If MPU-6050 is not found then returns 99 (C).
*/
{
    if (!isPresent)
        return 99.0f;  // Gyro is not available

    // Execute 2 byte read from MPU6050_REG_TEMP_OUT_H
    Wire.beginTransmission(MPU6050_I2C_ADDR);
    Wire.write(MPU6050_REG_TEMP_OUT_H);
    Wire.endTransmission(false);
    Wire.requestFrom(MPU6050_I2C_ADDR, 2, 1);            // Read 2 registers total, the temperature value is stored in 2 registers
    int16_t tempValue = Wire.read() << 8 | Wire.read();  // Raw Temperature value

    // Calculating the actual temperature value
    float result = static_cast<float>(tempValue) / 340.0f + 36.53f;
    return result;
}
#endif
