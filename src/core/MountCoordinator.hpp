#pragma once

#include <stdint.h>

#include "core/AzAltController.hpp"
#include "core/EventBus.hpp"
#include "core/FocusController.hpp"
#include "core/GuidingController.hpp"
#include "core/MountRequests.hpp"
#include "core/MountState.hpp"
#include "core/ParkingController.hpp"
#include "core/SlewController.hpp"
#include "core/TrackingController.hpp"
#include "ports/IClock.hpp"
#include "ports/IStepperAxis.hpp"

namespace core
{

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
    MountState _state;
    SlewController _slew;
    ParkingController _parking;
    EventBus<kEventQueueDepth> _events;
};

}  // namespace core
