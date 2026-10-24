#pragma once

#include "Types.hpp"

namespace core
{

/// What a "move one axis by N steps" request has to do before the axis is programmed.
///
/// The four optional axes and the two main ones behave differently, and the difference is not
/// guessable from the call: RA has to take the tracker out of the way first, DEC does not, and
/// AZ/ALT/focus never enter the slew state machine at all.
struct RelativeMovePlan {
    /// The request names one axis. `RA_AND_DEC_STEPS` is rejected: the relative move exists for
    /// single-axis nudges and the firmware has always refused the combined form.
    bool accepted = false;

    /// Stop the tracking axis first. RA only, and only when the move is non-zero and the tracker
    /// is actually running — stopping it when it is not would publish a `TrackingStopped` event
    /// for a move that never needed one.
    bool stopTracking = false;

    /// Switch the axis to slew microsteps and mark the slew state. True for RA and DEC.
    ///
    /// Preserved quirk: this is set for RA even when `steps` is zero, so a zero-step RA request
    /// enters the slew state machine and switches microsteps without moving. Nothing in the tree
    /// calls it that way, and the alternative is a behaviour change, so it is modelled rather
    /// than corrected.
    bool entersSlewMode = false;
};

RelativeMovePlan planRelativeMove(StepperAxis axis, long steps, bool tracking);

}  // namespace core
