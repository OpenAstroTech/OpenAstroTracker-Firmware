#pragma once

namespace ports
{

/// A hall sensor used to home an axis.
///
/// Polarity is normalised by the adapter: `isTriggered()` is true when the sensor
/// sees the magnet, whichever way the pin is wired.
class IHomingSensor
{
  public:
    virtual ~IHomingSensor()
    {
    }

    virtual bool isTriggered() = 0;
};

}  // namespace ports
