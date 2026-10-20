#pragma once

#include <stdint.h>

namespace ports
{

/// One snapshot of the GPS receiver's state, flattened from the library's nested
/// objects so no library type crosses the port.
struct GpsFix {
    bool locationValid;
    /// Degrees, positive north.
    double latitude;
    /// Degrees, positive east.
    double longitude;
    /// Age of the last valid location update in milliseconds.
    uint32_t locationAgeMs;

    bool dateTimeValid;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;

    uint8_t satellites;
};

class IGps
{
  public:
    virtual ~IGps()
    {
    }

    virtual bool begin() = 0;

    /// Pumps the receiver's serial stream. Call once per main-loop iteration.
    virtual void poll() = 0;

    /// True once the receiver has reported a valid location at least once.
    virtual bool hasLocation() = 0;

    virtual GpsFix fix() = 0;
};

}  // namespace ports
