#pragma once

#include <stdint.h>

#include "MountState.hpp"

namespace core
{

/// What the mount is doing, in the vocabulary the protocol and the displays have always used.
///
/// The name is a classification, not a state machine: the controllers own the transitions and
/// `MountState` owns the bits; this only answers "which word do we show" from one consistent read
/// of both, so the status line, the info panel and `:D#` cannot disagree.
enum class ActivityState : uint8_t
{
    Idle,
    Parked,
    Parking,
    Homing,
    Guiding,
    SlewToTarget,
    FreeSlew,
    ManualSlew,
    Tracking,

    /// A slew is in flight but no kind bit says which. The legacy status string printed nothing in
    /// this window, and the protocol text is frozen, so the caller maps this to an empty string.
    Unknown,
};

/// Classifies one instant.
///
/// `anySlew` is true when an RA or DEC axis is moving, `trackingAxis` when the tracking axis is
/// (the two halves of the legacy `slewStatus()` bitmask that this decision needs). Precedence is
/// the historical one: parked before parking, parking before homing, and a guiding pulse masks
/// everything but the two above it.
ActivityState classifyActivity(const MountState &state, bool findingHome, bool guiding, bool anySlew, bool trackingAxis);

}  // namespace core
