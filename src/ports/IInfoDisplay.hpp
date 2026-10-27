#pragma once

#include <stdint.h>

#include "types/DayTime.hpp"

namespace ports
{

/// Everything the optional 128x64 status panel shows.
///
/// A plain POD with fixed-capacity text: the producer fills it once per frame, the renderer reads
/// only these fields, and neither side hands the other a live domain object. When a frame is
/// produced is the panel driver's decision; this struct only describes one frame.
struct InfoDisplayState {
    /// Current stepper positions, in slew microsteps (what the position markers map to).
    long raSteps;
    long decSteps;
    /// Tracking stepper position, raw. Divided by the microstep ratio by the renderer.
    long trkSteps;
    /// Microsteps per degree to pair with `raSteps` / `decSteps`, for the marker arithmetic.
    float raStepsPerDegree;
    float decStepsPerDegree;

    /// Which axes are moving right now: the six status rectangles.
    bool raRunning;
    bool decRunning;
    bool altRunning;
    bool azRunning;
    bool trkRunning;
    /// A guide pulse is in flight (the GDE rectangle).
    bool guiding;

    /// High-level status, for the header line and the activity mark.
    bool tracking;
    bool slewing;
    bool parked;

    /// Command counter: the panel animates its activity mark when this changes.
    long commandsReceived;

    /// Slew progress: present only while a slew is in flight.
    bool progressActive;
    /// Percentage of the RA/DEC slew already travelled, 0-100.
    int raProgressPercent;
    int decProgressPercent;

    /// Site and time readouts the panel cycles through. Latitude and longitude are carried as
    /// hours (`getTotalHours()`, 15 degrees per hour) because that is the unit the overlay types
    /// and the panel's DMS formatter work in.
    float latitudeHours;
    float longitudeHours;
    /// Hours of tracking left before the RA limit, as a decimal hour count.
    float hoursToLimit;
    /// Local sidereal time.
    types::DayTime lst;

    /// Pre-formatted coordinate strings, in the display's own wire shape.
    char raText[24];
    char decText[24];

    /// Short status line, or nullptr when there is nothing to say. Must stay valid for as long as
    /// the state does: the panel only copies the characters it draws.
    const char *statusText;
};

/// Which axes the step generator is driving this frame. Split from the full snapshot so the
/// display's refresh policy can ask cheaply, without the fill's side effects.
struct InfoDisplayActivity {
    /// The RA axis is generating steps.
    bool raRunning;
    /// The DEC axis is generating steps.
    bool decRunning;
};

/// Where the panel's frames come from.
///
/// `app::MountReadout` implements it. The display only ever sees this interface, so it never pulls
/// from a live domain object.
class IInfoDisplaySource
{
  public:
    virtual ~IInfoDisplaySource()
    {
    }

    /// Cheap check for the refresh policy: which axes are moving right now. Must not fill a frame.
    virtual InfoDisplayActivity activity() = 0;

    /// Fills every field of `state` for one frame, in one pass, so the frame is self-consistent.
    virtual void fillInfoDisplayState(InfoDisplayState &state) = 0;
};

/// The optional status panel next to the menu display.
///
/// A frame is pushed, never pulled: the panel receives a finished `InfoDisplayState` and owns its
/// layout, clearing and glyphs. The caller decides what to show and how often; this port only says
/// "draw this frame".
class IInfoDisplay
{
  public:
    virtual ~IInfoDisplay()
    {
    }

    /// Brings the panel up. False when it is absent or does not answer on its bus.
    virtual bool begin() = 0;

    /// Draws one complete frame. The panel clears its own buffer; the caller need not.
    virtual void render(const InfoDisplayState &state) = 0;
};

}  // namespace ports
