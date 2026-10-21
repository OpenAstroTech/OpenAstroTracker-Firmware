#pragma once

#include <stdint.h>

namespace core
{

/// What the mount is doing right now, as the admission rules see it.
///
/// A plain value, filled from `MountState` and the trackers at the call site. Keeping it
/// a value rather than a second stored copy is deliberate: `MountState` stays the single
/// source of truth, and the rules stay a pure function that can be tested exhaustively.
struct MountActivityFacts {
    bool parking;      ///< homing to park, or moving to the park position
    bool tracking;     ///< sidereal tracking is running
    bool guiding;      ///< a guide pulse is in flight
    bool slewing;      ///< a manual or go-to slew is in progress
    bool findingHome;  ///< the hall sensor is homing an axis
};

/// A request the outside world can make of the mount.
enum class MountRequest : uint8_t
{
    Track,       ///< start sidereal tracking
    GoTo,        ///< slew to the configured target
    Home,        ///< slew to the home position
    ManualSlew,  ///< direction-pad move
    Guide,       ///< a guide pulse
    Park,        ///< park the mount
};

/// The verdict, and the reason when the answer is no.
enum class Admission : uint8_t
{
    Allowed,
    RefusedWhileParking,
    RefusedWhileNotTracking,
};

/// The arbitration table: which requests are accepted in which state.
///
/// This is the machine-readable form of rules that used to live as scattered guards
/// (`if (!isParking())`, `if (!isSlewingTRK())`, `stopGuiding()` at the top of a slew).
/// Stating them once, with a test per cell, is what makes the additivity checkable:
///
/// * **Parking wins over everything.** A direction-pad move, tracking, a go-to or a home
///   slew is refused while the mount parks, because the park sequence owns both axes
///   until it finishes. A guide pulse never reaches this point while parking either —
///   it needs tracking, and tracking is stopped by the park.
/// * **Every slew ends guiding.** The caller stops the pulse before it starts; the table
///   does not forbid a slew during guiding, it requires the stop as a precondition.
/// * **A guide pulse needs tracking.** Without it there is no sidereal motion to bias,
///   and the legacy code ignored these pulses.
/// * **Park is always accepted.** Re-parking while parked just re-runs the sequence, as
///   it always has.
///
/// The one behaviour this table *adds* is the go-to-while-parking refusal, which PLAN.md
/// lists as the sanctioned fix: before it, `:MS#` could reprogram both axes underneath a
/// park sequence that was still homing.
class MountRequests
{
  public:
    static Admission admit(MountRequest request, const MountActivityFacts &facts)
    {
        switch (request)
        {
            case MountRequest::Park:
                return Admission::Allowed;

            case MountRequest::Track:
            case MountRequest::GoTo:
            case MountRequest::Home:
            case MountRequest::ManualSlew:
                return facts.parking ? Admission::RefusedWhileParking : Admission::Allowed;

            case MountRequest::Guide:
                if (facts.parking)
                {
                    return Admission::RefusedWhileParking;
                }
                return facts.tracking ? Admission::Allowed : Admission::RefusedWhileNotTracking;
        }

        return Admission::Allowed;
    }

    /// True when a slew must stop the guide pulse first. Every slew shape does, which is
    /// why the call sites all begin with `stopGuiding()`.
    static bool slewStopsGuiding(MountRequest request)
    {
        return (request == MountRequest::GoTo) || (request == MountRequest::Home) || (request == MountRequest::ManualSlew);
    }
};

}  // namespace core
