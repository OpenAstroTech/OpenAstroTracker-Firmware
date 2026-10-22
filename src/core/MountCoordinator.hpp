#pragma once

#include <stdint.h>

#include "core/AzAltController.hpp"
#include "core/DriftAlignmentController.hpp"
#include "core/EventBus.hpp"
#include "core/FocusController.hpp"
#include "core/GuidingController.hpp"
#include "core/MountRequests.hpp"
#include "core/MountState.hpp"
#include "core/ParkingController.hpp"
#include "core/SlewCompletion.hpp"
#include "core/SlewController.hpp"
#include "core/TrackingController.hpp"
#include "ports/IClock.hpp"
#include "ports/IStepperAxis.hpp"

namespace core
{

/// What the controller half of a tick decided, so the hardware layer knows what is left.
///
/// The tick used to be a fixed order of six calls buried in a 300-line `Mount::loop()`,
/// interleaved with pins and EEPROM writes, so "in what order do these run" was only
/// answerable by reading the whole function. The order is now stated once, in
/// `MountCoordinator::tick()`, and reported here as facts the caller acts on.
struct TickOutcome {
    bool azAltJustStopped  = false;  ///< the AZ/ALT axes stopped this tick: power the drivers down
    bool focusStopped      = false;  ///< the focuser stopped this tick: disable and publish
    bool guiding           = false;  ///< a guide pulse owns the tick; the rest of it is skipped
    bool guidingRaExpired  = false;  ///< this pulse ends on RA
    bool guidingDecExpired = false;  ///< this pulse ends on DEC
};

/// The objects that make up "what the mount is doing", in one place.
///
/// Before this, `Mount` held nine controllers as loose members and each of them was
/// reachable from anywhere in a 4,600-line class. Nothing said which of them may be
/// touched during a tick or in what order, and admission had to be asked of `Mount`
/// itself, which answered by reading four of those members. The coordinator is the one
/// object that owns them, so there is a single place to put the tick order and the
/// ownership rules.
///
/// It holds no hardware: every controller here is pure logic, and the axes, pins,
/// displays and EEPROM writes stay in `Mount`. `FindingHome` is the one admission fact
/// the coordinator cannot derive, because it needs the hall sensor ports, so the caller
/// passes it in.
///
/// `TrackingAxis` is a template parameter for the reason `TrackingController` is: the AVR
/// build keeps its compile-time interrupt-stepper engine rather than an interface.
/// `FocusAxis` is fixed to `ports::IStepperAxis` on every board since the optional axes
/// became runtime-present.
template <typename TrackingAxis = ports::IStepperAxis, typename FocusAxis = ports::IStepperAxis> class MountCoordinator
{
  public:
    /// Four is enough for one tick's worth of facts and keeps the ring in SRAM: the AVR
    /// has no divide instruction, so the capacity has to stay a power of two.
    static constexpr uint8_t kEventQueueDepth = 4;

    MountCoordinator(ports::IClock *clock, float siderealSecondsPerDay, unsigned trackingMicrostepRatio)
        : _tracking(siderealSecondsPerDay, trackingMicrostepRatio), _guiding(clock)
    {
    }

    MountCoordinator(const MountCoordinator &)            = delete;
    MountCoordinator &operator=(const MountCoordinator &) = delete;

    MountState &state()
    {
        return _state;
    }

    const MountState &state() const
    {
        return _state;
    }

    SlewController &slew()
    {
        return _slew;
    }

    const SlewController &slew() const
    {
        return _slew;
    }

    ParkingController &parking()
    {
        return _parking;
    }

    const ParkingController &parking() const
    {
        return _parking;
    }

    EventBus<kEventQueueDepth> &events()
    {
        return _events;
    }

    TrackingController<TrackingAxis> &tracking()
    {
        return _tracking;
    }

    const TrackingController<TrackingAxis> &tracking() const
    {
        return _tracking;
    }

    GuidingController &guiding()
    {
        return _guiding;
    }

    const GuidingController &guiding() const
    {
        return _guiding;
    }

    FocusController<FocusAxis> &focus()
    {
        return _focus;
    }

    const FocusController<FocusAxis> &focus() const
    {
        return _focus;
    }

    AzAltController &azAlt()
    {
        return _azAlt;
    }

    DriftAlignmentController &drift()
    {
        return _drift;
    }

    const AzAltController &azAlt() const
    {
        return _azAlt;
    }

    /// What the mount is doing, derived from the controllers it owns.
    ///
    /// Every field is the answer `Mount` used to assemble by hand in `activityFacts()`,
    /// including the one that is easy to get wrong: tracking counts as a slew only
    /// outside parking and guiding, because `slewStatus()` reports no slew at all in
    /// those two states. Taking the raw `StatusBit::Tracking` bit here would admit a
    /// guide pulse on a parked mount, which the firmware has never done.
    MountActivityFacts activityFacts() const
    {
        MountActivityFacts facts;
        facts.parking     = _parking.isParking();
        facts.guiding     = _state.test(bitMask(StatusBit::GuidePulse));
        facts.tracking    = !facts.parking && !facts.guiding && _state.test(bitMask(StatusBit::Tracking));
        facts.slewing     = _state.test(bitMask(StatusBit::Slewing));
        facts.findingHome = _state.test(bitMask(StatusBit::FindingHome));
        return facts;
    }

    /// The arbitration table (see `MountRequests`), applied to this mount's own state.
    Admission admit(MountRequest request) const
    {
        return MountRequests::admit(request, activityFacts());
    }

    /// Runs the controllers this tick, in the one order that is correct.
    ///
    /// Published facts are delivered first, so a subscriber is never called from inside the
    /// code that published. Then the two independent axes poll for "stopped", then the guide
    /// pulse — which, when one is in flight, is the whole tick: the rest of `loop()` is
    /// skipped so a pulse cannot be interrupted by slew bookkeeping.
    ///
    /// Every hardware consequence is reported, not performed: the caller owns the pins.
    TickOutcome tick(bool azAltRunning)
    {
        TickOutcome outcome;

        _events.dispatch();

        outcome.azAltJustStopped = (_azAlt.update(azAltRunning) == AzAltUpdate::JustStopped);
        outcome.focusStopped     = (_focus.update() == FocusUpdate::Stopped);

        outcome.guiding = _state.test(bitMask(StatusBit::GuidePulse));
        if (outcome.guiding)
        {
            outcome.guidingRaExpired  = _guiding.raExpired();
            outcome.guidingDecExpired = _guiding.decExpired();
        }

        return outcome;
    }

    /// Records what the arrival logic saw at the start of this tick. Called at the end of it,
    /// because the arrival path reads the previous tick's answer to tell "still moving" from
    /// "just stopped".
    void finishTick(bool raRunning, bool decRunning)
    {
        _slew.noteRunning(raRunning || decRunning);
    }

    /// Everything `SlewCompletion` needs, taken from the controllers that own it.
    ///
    /// `Mount` used to fill these six fields by hand in the middle of the arrival block.
    ArrivalFacts arrivalFacts() const
    {
        ArrivalFacts facts;
        facts.intent                = _slew.intent();
        facts.parking               = _parking.isParking();
        facts.headingToParkPosition = _parking.headingToPosition();
        facts.findingHome           = _state.test(bitMask(StatusBit::FindingHome));
        facts.compensationPending   = _tracking.compensationPending();
        facts.backlashOwed          = _slew.backlashOwed();
        facts.parkTargetReached     = false;  // computed from post-move distances, if ever needed
        return facts;
    }

    /// The plan for finishing the move that just arrived.
    CompletionPlan completionPlan() const
    {
        return SlewCompletion::plan(arrivalFacts());
    }

  private:
    /// `bit` would be a macro on Arduino, and this header is included from the sketch's
    /// translation units, so the helper is named around it.
    static constexpr uint16_t bitMask(StatusBit which)
    {
        return static_cast<uint16_t>(which);
    }

    TrackingController<TrackingAxis> _tracking;
    GuidingController _guiding;
    FocusController<FocusAxis> _focus;
    AzAltController _azAlt;
    DriftAlignmentController _drift;
    MountState _state;
    SlewController _slew;
    ParkingController _parking;
    EventBus<kEventQueueDepth> _events;
};

}  // namespace core
