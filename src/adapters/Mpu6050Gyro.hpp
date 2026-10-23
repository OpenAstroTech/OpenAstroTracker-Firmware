#pragma once

#include "hal/II2cBus.hpp"
#include "ports/IClock.hpp"
#include "ports/IGyro.hpp"

namespace adapters
{

/// `ports::IGyro` over an MPU-6050 on an I2C bus.
///
/// See https://invensense.tdk.com/products/motion-tracking/6-axis/mpu-6050/ and the register
/// map: https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Register-Map1.pdf
///
/// The sampling policy and the exponential moving average are the ones the firmware has always
/// used: at most one accelerometer sample per `kSampleIntervalMs`, with `kEmaAlpha` smoothing.
/// Time comes from the clock port, so this adapter is host-testable over a fake bus.
class Mpu6050Gyro final : public ports::IGyro
{
  public:
    static constexpr float kEmaAlpha                 = 0.1f;
    static constexpr unsigned long kSampleIntervalMs = 5;

    Mpu6050Gyro(hal::II2cBus &bus, ports::IClock &clock) : _bus(bus), _clock(clock)
    {
    }

    bool begin() override;
    void shutdown() override;
    bool isPresent() override;
    ports::GyroAngles angles() override;
    float temperature() override;

  private:
    enum
    {
        kAddress          = 0x68,
        kRegConfig        = 0x1A,
        kRegAccelXOutHigh = 0x3B,
        kRegTempOutHigh   = 0x41,
        kRegPowerMgmt1    = 0x6B,
        kRegWhoAmI        = 0x75,
    };

    void collectSample();

    hal::II2cBus &_bus;
    ports::IClock &_clock;
    bool _present               = false;
    bool _initialized           = false;
    float _pitchEma             = 0;
    float _rollEma              = 0;
    unsigned long _lastSampleMs = 0;
};

}  // namespace adapters
