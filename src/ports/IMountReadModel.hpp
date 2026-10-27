#pragma once

#include <stdint.h>

#include "types/DayTime.hpp"
#include "ports/PortVocabulary.hpp"
#include "types/Declination.hpp"

namespace ports
{
/// What the command layer reads back from the mount.
///
/// This is the second of the three surfaces Phase 5 step 3 identified (motion, read model,
/// settings). Everything here is a question, not an action: where the axes are, where the
/// target is, how far the mount can still track before hitting its RA limit, and the status
/// strings a host polls. Keeping it separate from `IMountMotion` is what lets a test drive the
/// command layer with a static read model and no axes at all.
///
/// The coordinates are `types::` values on purpose. The Arduino-aware `DayTime` / `Declination`
/// overlays in `src/` derive from them, so a `core::` return cannot be overridden by the overlay
/// by value (covariance only works through pointers and references). The adapter converts at the
/// call site, which is where the wire format is decided anyway.
class IMountReadModel
{
  public:
    virtual ~IMountReadModel()
    {
    }

    virtual types::DayTime currentRA() const      = 0;
    virtual types::Declination currentDEC() const = 0;

    /// Mutable references: `:Sr#` / `:Sd#` write the target in place, and the mount has always
    /// exposed it that way. A setter pair would need the same storage and one more entry point.
    virtual types::DayTime &targetRA()      = 0;
    virtual types::Declination &targetDEC() = 0;

    /// Hour angle and local sidereal time, both derived from the clock and the site.
    virtual types::DayTime calculateHa()  = 0;
    virtual types::DayTime calculateLst() = 0;

    /// Converts a celestial coordinate into stepper positions without moving anything.
    virtual void calculateStepperPositions(float raCoord, float decCoord, long &raPos, long &decPos) = 0;

    /// Minutes of tracking left before the RA axis reaches its limit.
    virtual float checkRALimit() = 0;

    /// Hour angle as the mount currently reports it (the value `:GH#` sends).
    virtual types::DayTime hourAngle() = 0;

    /// Current stepper position of one axis, in its own microsteps. The `int` overload takes a
    /// movement direction (`WEST`, `NORTH`, `TRACKING`) as the menu screens do.
    virtual long getCurrentStepperPosition(StepperAxis which) = 0;
    virtual long getCurrentStepperPosition(int direction)     = 0;

    /// Whether the step generator is driving this axis right now.
    virtual bool isAxisRunning(StepperAxis axis) = 0;

    /// True when neither the RA nor the DEC axis is moving, and nothing is parked or homing.
    virtual bool isSlewingIdle() const = 0;

    /// Slew progress for the two-axis progress bar: percentages, true while a slew is in flight.
    virtual bool getStepperProgress(int &raPercentage, int &decPercentage) = 0;

    /// How far the slew in flight was asked to travel, in steps.
    virtual float plannedRaSteps() const  = 0;
    virtual float plannedDecSteps() const = 0;

    /// True once `setup()` has finished bringing the mount up.
    virtual bool isBootComplete() = 0;

    virtual bool isGuiding() const = 0;

    /// The hemisphere the site is in. A read fact like the site itself, not a status bit: the POI
    /// screen converts its declinations by hand for a southern mount, and the direction inversion
    /// is decided from it. Northern until a site says otherwise, as it always has been.
    virtual bool isNorthernHemisphere() const = 0;

    /// Status readouts, as the strings the protocol sends.
    ///
    /// They return a pointer into storage the mount owns and keeps until the next call to the
    /// same method: the alternative — filling a caller buffer — would mean either duplicating
    /// four `String`-building methods or moving them into the adapter, and the wire format of
    /// `:D#` is parsed by every host driver on the planet.
    /// The coordinate readouts, in the shape the menu and the info panel draw. Same storage rule as
    /// the status strings below: valid until the next call.
    virtual const char *raText(uint8_t type, uint8_t active = 0)  = 0;
    virtual const char *decText(uint8_t type, uint8_t active = 0) = 0;

    virtual const char *getStatusString()           = 0;
    virtual const char *getStepperInfo()            = 0;
    virtual const char *getMountHardwareInfo()      = 0;
    virtual const char *getAutoHomingStates() const = 0;

    /// The command counter a status screen shows. The command layer is its only writer, which is
    /// why the increment lives beside the read rather than on a port of its own.
    virtual long getNumCommandsReceived() = 0;
    virtual void commandReceived()        = 0;
};

}  // namespace ports
