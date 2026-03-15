#pragma once

#if USE_GYRO_LEVEL == 1
struct angle_t {
    float pitchAngle = 0;
    float rollAngle  = 0;
};

class Gyro
{
  public:
    static void startup();
    static void shutdown();
    static angle_t getCurrentAngles();
    static float getCurrentTemperature();

  private:
    static void collectSample();

    // MPU6050 constants
    enum
    {
        MPU6050_I2C_ADDR = 0x68,  // I2C address of the MPU6050 accelerometer

        // Register addresses
        MPU6050_REG_CONFIG       = 0x1A,
        MPU6050_REG_ACCEL_XOUT_H = 0x3B,
        MPU6050_REG_TEMP_OUT_H   = 0x41,
        MPU6050_REG_PWR_MGMT_1   = 0x6B,
        MPU6050_REG_WHO_AM_I     = 0x75
    };

    // Use about 72 bytes of RAM for the moving average buffer
    static const int WINDOW_SIZE              = 8;  // Number of samples to keep for moving average
    static const unsigned long SAMPLE_INTERVAL = 5;  // ms between samples

    static bool isPresent;
    static float _pitchSamples[WINDOW_SIZE];
    static float _rollSamples[WINDOW_SIZE];
    static int _ringIndex;
    static int _samplesCollected;  // Counts up to WINDOW_SIZE, then stays there
    static unsigned long _lastSampleTime;
};
#endif
