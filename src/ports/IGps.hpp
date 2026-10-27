#pragma once

#include <stdint.h>

namespace ports
{

/// One snapshot of the GPS receiver's state, flattened from the library's nested
/// objects so no library type crosses the port.
///
/// The two halves are independent: a receiver can have a location fix before it has decoded the
/// date and time, and the reverse. Check the matching `*Valid` flag before reading either group.
struct GpsFix {
    /// True when the `latitude`/`longitude` pair below is a real fix.
    bool locationValid;
    /// Degrees, positive north.
    double latitude;
    /// Degrees, positive east.
    double longitude;
    /// Age of the last valid location update in milliseconds.
    uint32_t locationAgeMs;

    /// True when the date and time below have been decoded since the receiver started.
    bool dateTimeValid;
    /// Full year, e.g. 2026.
    uint16_t year;
    /// 1-12.
    uint8_t month;
    /// 1-31.
    uint8_t day;
    /// 0-23, UTC as the satellites report it.
    uint8_t hour;
    /// 0-59.
    uint8_t minute;
    /// 0-59.
    uint8_t second;

    /// Satellites used in the current solution; 0 until the receiver has one.
    uint8_t satellites;
};

/// The optional GPS receiver, as the mount uses it: a fix to read, nothing else.
///
/// The adapter owns the stream and the library's decoding; the domain only polls and reads a
/// snapshot. A board without a receiver holds nothing at all, and the caller checks for a `nullptr`
/// before polling.
class IGps
{
  public:
    virtual ~IGps()
    {
    }

    /// Brings the receiver up. False when the stream cannot be opened; the caller may still poll,
    /// but no fix will ever arrive.
    virtual bool begin() = 0;

    /// Pumps the receiver's serial stream. Call once per main-loop iteration.
    virtual void poll() = 0;

    /// True once the receiver has reported a valid location at least once.
    virtual bool hasLocation() = 0;

    /// The current snapshot. All zero/false until the receiver has produced a fix.
    virtual GpsFix fix() = 0;
};

}  // namespace ports
