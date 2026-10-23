#include "Mpu6050Gyro.hpp"

#include <math.h>

namespace adapters
{

bool Mpu6050Gyro::begin()
{
    _bus.begin();

    uint8_t whoAmI = 0;
    if (!_bus.readRegisters(kAddress, kRegWhoAmI, &whoAmI, 1))
    {
        _present = false;
        return false;
    }

    // The register returns 0x68; the legacy check ignored the low bit.
    _present = (((whoAmI >> 1) & 0x3F) == 0x34);
    if (!_present)
    {
        return false;
    }

    // Wake the device (0 == no sleep, 8 MHz clock) and set the lowest accelerometer bandwidth.
    if (!_bus.writeRegister(kAddress, kRegPowerMgmt1, 0) || !_bus.writeRegister(kAddress, kRegConfig, 6))
    {
        _present = false;
        return false;
    }

    _initialized  = false;
    _pitchEma     = 0;
    _rollEma      = 0;
    _lastSampleMs = 0;
    return true;
}

void Mpu6050Gyro::shutdown()
{
    // Nothing to do: the sensor has no power-down path in the original firmware.
}

bool Mpu6050Gyro::isPresent()
{
    return _present;
}

void Mpu6050Gyro::collectSample()
{
    uint8_t raw[6] = {0};
    if (!_bus.readRegisters(kAddress, kRegAccelXOutHigh, raw, sizeof(raw)))
    {
        return;
    }

    const int16_t acX = static_cast<int16_t>((raw[0] << 8) | raw[1]);
    const int16_t acY = static_cast<int16_t>((raw[2] << 8) | raw[3]);
    const int16_t acZ = static_cast<int16_t>((raw[4] << 8) | raw[5]);

    // Pitch is rotation around Y, roll rotation around X.
    const float pitch = atan2f(-1.0f * acX, sqrtf((float) acY * acY + (float) acZ * acZ)) * 180.0f / static_cast<float>(M_PI);
    const float roll  = atan2f(-1.0f * acY, sqrtf((float) acX * acX + (float) acZ * acZ)) * 180.0f / static_cast<float>(M_PI);

    if (!_initialized)
    {
        _pitchEma    = pitch;
        _rollEma     = roll;
        _initialized = true;
    }
    else
    {
        _pitchEma = ((1.0f - kEmaAlpha) * _pitchEma) + (kEmaAlpha * pitch);
        _rollEma  = ((1.0f - kEmaAlpha) * _rollEma) + (kEmaAlpha * roll);
    }
}

ports::GyroAngles Mpu6050Gyro::angles()
{
    ports::GyroAngles result {0.0f, 0.0f};

    if (!_present)
    {
        return result;
    }

    const unsigned long now = _clock.now();
    if (now - _lastSampleMs >= kSampleIntervalMs)
    {
        _lastSampleMs = now;
        collectSample();
    }

    if (!_initialized)
    {
        return result;
    }

    result.pitch = _pitchEma;
    result.roll  = _rollEma;

#if GYRO_AXIS_SWAP == 1
    const float temp = result.pitch;
    result.pitch     = result.roll;
    result.roll      = temp;
#endif

    return result;
}

float Mpu6050Gyro::temperature()
{
    if (!_present)
    {
        return 99.0f;  // the legacy sentinel for "sensor not available"
    }

    uint8_t raw[2] = {0};
    if (!_bus.readRegisters(kAddress, kRegTempOutHigh, raw, sizeof(raw)))
    {
        return 99.0f;
    }

    const int16_t tempValue = static_cast<int16_t>((raw[0] << 8) | raw[1]);
    return static_cast<float>(tempValue) / 340.0f + 36.53f;
}

}  // namespace adapters
