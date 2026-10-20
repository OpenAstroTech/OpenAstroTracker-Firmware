#pragma once

namespace ports
{

/// Pitch/roll of the mount base as reported by the level sensor.
struct GyroAngles {
    float pitch;
    float roll;
};

/// The optional levelling sensor.
///
/// The adapter owns sampling and smoothing; `angles()` returns the latest filtered
/// reading. When the sensor is absent the composition root injects nothing and the
/// domain treats levelling as unavailable.
class IGyro
{
  public:
    virtual ~IGyro()
    {
    }

    /// Powers the sensor and verifies it answers. Returns false when absent.
    virtual bool begin() = 0;

    virtual void shutdown() = 0;

    virtual bool isPresent() = 0;

    virtual GyroAngles angles() = 0;

    /// Die temperature in degrees Celsius.
    virtual float temperature() = 0;
};

}  // namespace ports
