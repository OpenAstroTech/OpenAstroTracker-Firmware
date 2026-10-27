#pragma once

namespace ports
{

/// Pitch/roll of the mount base as reported by the level sensor, in degrees.
struct GyroAngles {
    /// Rotation around the Y axis, in degrees.
    float pitch;
    /// Rotation around the X axis, in degrees.
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

    /// Releases the sensor. A no-op where the hardware has no power-down path.
    virtual void shutdown() = 0;

    /// True once `begin()` has found the sensor. A consumer must check this before trusting
    /// `angles()`.
    virtual bool isPresent() = 0;

    /// The latest filtered pitch/roll. Zero when the sensor is absent.
    virtual GyroAngles angles() = 0;

    /// Die temperature in degrees Celsius.
    virtual float temperature() = 0;
};

}  // namespace ports
