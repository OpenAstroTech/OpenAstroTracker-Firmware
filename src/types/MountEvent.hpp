#pragma once

#include <stdint.h>

namespace types
{

/// Facts the mount publishes about what it just did.
///
/// These are observations, never requests: a subscriber may react (repaint a display,
/// log a line) but must never ask the publisher to change course. A name like
/// `StartSlew` would be a coordinator request in disguise and does not belong here.
///
/// The set is deliberately small: it covers the transitions a display or a log needs,
/// not every state the mount can be in. Anything else a subscriber wants it reads from
/// the mount's read model, which is always current.
enum class MountEventType : uint8_t
{
    TrackingStarted,
    TrackingStopped,
    SlewStarted,
    SlewArrived,
    ParkingStarted,
    Parked,
    HomingStarted,
    HomingFinished,
    GuidingStarted,
    GuidingStopped,
    FocusMoving,
    FocusStopped,
};

/// One event. Two bytes, no heap: the bus copies these into a fixed-size queue.
///
/// It carries no state snapshot on purpose. A queued snapshot is stale by the time it is
/// delivered, and on AVR every byte of the queue is SRAM; a subscriber that needs more
/// reads the mount's read model, which is always current.
struct MountEvent {
    MountEventType type;
    bool succeeded;  ///< homing outcome; always false for the other events
};

}  // namespace types
