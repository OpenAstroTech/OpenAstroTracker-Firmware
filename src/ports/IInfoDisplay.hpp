#pragma once

#include <stdint.h>

#include "types/DayTime.hpp"

namespace ports
{

/// Everything the optional 128x64 status panel shows.
///
/// A plain POD with fixed-capacity text: the producer fills it once per frame, the renderer reads
/// only these fields, and neither side hands the other a live domain object.
struct InfoDisplayState {
    /// Current stepper positions, in slew microsteps (what the position markers map to).
    long raSteps;
    long decSteps;
    /// Tracking stepper position, raw. Divided by the microstep ratio by the renderer.
    long trkSteps;
    /// Slewing microsteps per degree per axis, for the marker arithmetic.
    float raStepsPerDegree;
    float decStepsPerDegree;

    /// Which axes are moving right now: the six status rectangles.
    bool raRunning;
    bool decRunning;
    bool altRunning;
    bool azRunning;
    bool trkRunning;
    bool guiding;

    /// High-level status, for the header line and the activity mark.
    bool tracking;
    bool slewing;
    bool parked;

    /// Command counter: the panel animates its activity mark when this changes.
    long commandsReceived;

    /// Slew progress: present only while a slew is in flight.
    bool progressActive;
    int raProgressPercent;
    int decProgressPercent;

    /// Site and time readouts the panel cycles through.
    float latitudeHours;
    float longitudeHours;
    float hoursToLimit;
    types::DayTime lst;

    /// Pre-formatted coordinate strings, in the display's own wire shape.
    char raText[24];
    char decText[24];

    /// Short status line, or nullptr when there is nothing to say.
    const char *statusText;
};

/// Which axes the step generator is driving this frame. Split from the full snapshot so the
/// display's refresh policy can ask cheaply, without the fill's side effects.
struct InfoDisplayActivity {
    bool raRunning;
    bool decRunning;
};

/// Where the panel's frames come from.
///
/// `Mount` implements it today (it still owns the axes and the read model); it moves to the
/// coordinator when the last `Mount` consumer does. The display only ever sees this interface, so
/// it never pulls from a live domain object.
class IInfoDisplaySource
{
  public:
    virtual ~IInfoDisplaySource()
    {
    }

    virtual InfoDisplayActivity activity() = 0;

    virtual void fillInfoDisplayState(InfoDisplayState &state) = 0;
};

/// The optional status panel next to the menu display.
class IInfoDisplay
{
  public:
    virtual ~IInfoDisplay()
    {
    }

    virtual bool begin() = 0;

    virtual void render(const InfoDisplayState &state) = 0;
};

}  // namespace ports
