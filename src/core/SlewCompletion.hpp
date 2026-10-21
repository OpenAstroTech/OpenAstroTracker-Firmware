#pragma once

#include <stdint.h>

#include "SlewController.hpp"

namespace core
{

/// What the mount observed at the moment a slew finished.
struct ArrivalFacts {
    SlewIntent intent;           ///< what the move was for
    bool parking;                ///< STATUS_PARKING or STATUS_PARKING_POS is set
    bool headingToParkPosition;  ///< already on the second leg of a park
    bool findingHome;            ///< a hall sensor is still homing an axis
    bool compensationPending;    ///< the tracker was stopped for this slew
    bool backlashOwed;           ///< the RA backlash correction is still due
    bool parkTargetReached;      ///< the mount is already at the park position
};

/// The ordered list of things to do when a slew finishes.
///
/// A value, not a sequence of calls: the arrival path used to be 150 lines of nested
/// `if`s interleaved with axis, driver and display writes, which made the order the only
/// place the rules lived. Stating the decisions here lets them be tested, and leaves
/// `Mount` as the executor — it still owns every side effect, including the ones a
/// controller must not reach (`setHome()`, driver registers, display refresh).
///
/// The flags are applied in declaration order, which is the order the original code ran
/// them in. Several are deliberately *not* mutually exclusive, because the original ran
/// both: arriving home outside a park starts tracking twice (once from the generic
/// "not parking" block, once from the home branch), and the trailing DEC microstep reset
/// runs even after a park leg has just set DEC back to slew microstepping.
struct CompletionPlan {
    bool stopTracking;              ///< a park always stops tracking before homing
    bool setHome;                   ///< first park leg: the home position is now known
    bool raTrackingMicrosteps;      ///< unless a hall sensor is still homing RA
    bool compensateTracker;         ///< outside a park, catch up the tracker
    bool startTracking;             ///< outside a park, resume sidereal tracking
    bool decGuideMicrosteps;        ///< DEC returns to guide microstepping
    bool correctBacklash;           ///< RA backlash correction is due
    bool retargetRa;                ///< a home move re-anchors the RA target
    bool startParkPositionLeg;      ///< park: start the second leg to the park position
    bool parkPositionReached;       ///< park: the second leg had nothing to do
    bool restartTrackingAfterHome;  ///< home outside a park resumes tracking
    bool decGuideAfterHome;         ///< the trailing DEC microstep reset
    bool parkComplete;              ///< second park leg finished: the mount is parked
    bool endSlew;                   ///< the slew is no longer in flight
};

/// Decides what happens when the axes have stopped. Pure; no hardware, no time.
class SlewCompletion
{
  public:
    static CompletionPlan plan(const ArrivalFacts &facts)
    {
        CompletionPlan out = {};

        out.stopTracking         = facts.parking;
        out.setHome              = facts.parking && !facts.headingToParkPosition;
        out.raTrackingMicrosteps = !facts.findingHome;
        out.compensateTracker    = !facts.parking && facts.compensationPending;
        out.startTracking        = !facts.parking && !facts.findingHome;
        out.decGuideMicrosteps   = true;
        out.correctBacklash      = facts.backlashOwed;
        out.retargetRa           = (facts.intent == SlewIntent::Home);

        // A home move that was requested to park starts a second leg to the park
        // position. Whether that leg has anything to do is only known after the targets
        // are programmed, so that is `parkLegIsNoOp()` below.
        out.startParkPositionLeg = (facts.intent == SlewIntent::Home) && facts.parking;

        out.restartTrackingAfterHome = (facts.intent == SlewIntent::Home) && !facts.parking;
        out.decGuideAfterHome        = (facts.intent == SlewIntent::Home);

        out.parkComplete = (facts.intent != SlewIntent::Home) && facts.headingToParkPosition;

        out.endSlew = true;
        return out;
    }

    /// True when the park position is where the mount already is, so the second leg is
    /// finished the moment it is programmed.
    static bool parkLegIsNoOp(long raDistanceToGo, long decDistanceToGo)
    {
        return (raDistanceToGo == 0) && (decDistanceToGo == 0);
    }
};

}  // namespace core
