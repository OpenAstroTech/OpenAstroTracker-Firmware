#include <gtest/gtest.h>

#include "../test_common/hal_fakes/FakeStepperAxis.hpp"
#include "../test_common/hal_fakes/FakeSystemClock.hpp"
#include "core/MountCoordinator.hpp"

using core::Admission;
using core::MountCoordinator;
using core::MountRequest;
using core::ParkingPhase;
using core::SlewIntent;
using core::StatusBit;
using types::MountEvent;
using types::MountEventType;

namespace
{
constexpr float kSiderealSecondsPerDay = 86164.1f;
constexpr unsigned kMicrostepRatio     = 32;  // e.g. 256x tracking / 8x slew

uint16_t mask(StatusBit which)
{
    return static_cast<uint16_t>(which);
}

/// The coordinator over `ports::IStepperAxis`, which is what every board but AVR uses,
/// plus the fake clock its controllers want.
struct Fixture {
    fakes::FakeClock clock;
    MountCoordinator coordinator {&clock, kSiderealSecondsPerDay, kMicrostepRatio};

    void track()
    {
        coordinator.state().set(mask(StatusBit::Tracking));
    }
};

/// Collects what the coordinator's event bus delivers.
class RecordingSink final : public ports::IEventSink
{
  public:
    void onMountEvent(const MountEvent &event) override
    {
        last = event.type;
        count++;
    }

    MountEventType last = MountEventType::TrackingStarted;
    int count           = 0;
};
}  // namespace

// The coordinator is the object that owns the controllers, so a controller reached through
// an accessor has to be the same object the previous statement touched. If these accessors
// ever returned copies, every caller would silently edit a temporary.
TEST(MountCoordinatorTest, AccessorsReturnTheOwnedControllers)
{
    Fixture f;

    f.coordinator.state().set(mask(StatusBit::Tracking));
    EXPECT_TRUE(f.coordinator.state().test(mask(StatusBit::Tracking)));

    f.coordinator.slew().begin(SlewIntent::Target);
    EXPECT_EQ(SlewIntent::Target, f.coordinator.slew().intent());

    f.coordinator.parking().beginHoming();
    EXPECT_TRUE(f.coordinator.parking().isParking());

    // A const view has to see the same state: admission runs through it.
    const MountCoordinator &view = f.coordinator;
    EXPECT_TRUE(view.state().test(mask(StatusBit::Tracking)));
    EXPECT_TRUE(view.parking().isParking());
    EXPECT_EQ(SlewIntent::Target, view.slew().intent());
}

TEST(MountCoordinatorTest, StartsIdle)
{
    Fixture f;

    core::MountActivityFacts facts = f.coordinator.activityFacts();
    EXPECT_FALSE(facts.parking);
    EXPECT_FALSE(facts.tracking);
    EXPECT_FALSE(facts.guiding);
    EXPECT_FALSE(facts.slewing);
    EXPECT_FALSE(facts.findingHome);
    EXPECT_EQ(ParkingPhase::NotParking, f.coordinator.parking().phase());
    EXPECT_FALSE(f.coordinator.slew().isActive());
}

// `activityFacts()` replaces the facts `Mount` used to assemble by hand, so it has to report
// the same things the four old accessors did.
TEST(MountCoordinatorTest, ActivityFactsMirrorTheMountsOwnAccessors)
{
    Fixture f;
    f.track();
    f.coordinator.state().set(mask(StatusBit::Slewing));
    f.coordinator.state().set(mask(StatusBit::FindingHome));

    core::MountActivityFacts facts = f.coordinator.activityFacts();
    EXPECT_TRUE(facts.tracking);
    EXPECT_TRUE(facts.slewing);
    EXPECT_TRUE(facts.findingHome);
    EXPECT_FALSE(facts.parking);
    EXPECT_FALSE(facts.guiding);
}

// `slewStatus()` reports no slew at all while the mount is parked or guiding, so the old
// fact build saw no tracking there either. Reading the raw bit instead would admit a guide
// pulse on a parked mount, which the firmware has never done.
TEST(MountCoordinatorTest, TrackingFactIsSuppressedWhileParkingOrGuiding)
{
    Fixture f;
    f.track();

    f.coordinator.parking().beginHoming();
    EXPECT_FALSE(f.coordinator.activityFacts().tracking);
    EXPECT_EQ(Admission::RefusedWhileParking, f.coordinator.admit(MountRequest::Guide));

    f.coordinator.parking().clear();
    EXPECT_TRUE(f.coordinator.activityFacts().tracking);

    f.coordinator.state().set(mask(StatusBit::GuidePulse));
    EXPECT_TRUE(f.coordinator.activityFacts().guiding);
    EXPECT_FALSE(f.coordinator.activityFacts().tracking);
    EXPECT_EQ(Admission::RefusedWhileNotTracking, f.coordinator.admit(MountRequest::Guide));
}

// The verdict is the table applied to this mount's own state, not to state a caller passes
// in, so the parking phase alone has to refuse every move.
TEST(MountCoordinatorTest, ParkingRefusesEveryMove)
{
    Fixture f;
    f.coordinator.parking().beginHoming();

    EXPECT_EQ(Admission::RefusedWhileParking, f.coordinator.admit(MountRequest::Track));
    EXPECT_EQ(Admission::RefusedWhileParking, f.coordinator.admit(MountRequest::GoTo));
    EXPECT_EQ(Admission::RefusedWhileParking, f.coordinator.admit(MountRequest::Home));
    EXPECT_EQ(Admission::RefusedWhileParking, f.coordinator.admit(MountRequest::ManualSlew));
    EXPECT_EQ(Admission::Allowed, f.coordinator.admit(MountRequest::Park));

    // A finished park releases the axes: that is how unparking works.
    f.coordinator.parking().markParked();
    EXPECT_FALSE(f.coordinator.activityFacts().parking);
    EXPECT_EQ(Admission::Allowed, f.coordinator.admit(MountRequest::GoTo));
    EXPECT_EQ(Admission::Allowed, f.coordinator.admit(MountRequest::ManualSlew));
}

TEST(MountCoordinatorTest, TrackingIsWhatAdmitsAGuidePulse)
{
    Fixture f;

    EXPECT_EQ(Admission::RefusedWhileNotTracking, f.coordinator.admit(MountRequest::Guide));
    EXPECT_EQ(Admission::Allowed, f.coordinator.admit(MountRequest::GoTo));

    f.track();
    EXPECT_EQ(Admission::Allowed, f.coordinator.admit(MountRequest::Guide));
}

// A find is the hall-sensor state machine owning the axes; it reaches admission through the
// status bit, not the parking phase.
TEST(MountCoordinatorTest, FindingHomeIsReadFromTheStatusBit)
{
    Fixture f;

    f.coordinator.state().set(mask(StatusBit::FindingHome));
    EXPECT_TRUE(f.coordinator.activityFacts().findingHome);
    EXPECT_FALSE(f.coordinator.activityFacts().parking);
}

// The event bus moved inside the coordinator, so publishing and draining has to work with
// no `Mount` involved.
TEST(MountCoordinatorTest, EventsAreQueuedAndDispatchedThroughTheOwnedBus)
{
    Fixture f;
    RecordingSink sink;
    f.coordinator.events().subscribe(&sink);

    f.coordinator.events().publish(MountEvent {MountEventType::Parked, false});
    f.coordinator.events().publish(MountEvent {MountEventType::SlewArrived, false});
    EXPECT_EQ(0, sink.count);  // nothing is delivered until the tick boundary

    f.coordinator.events().dispatch();
    EXPECT_EQ(2, sink.count);
    EXPECT_EQ(MountEventType::SlewArrived, sink.last);

    f.coordinator.events().dispatch();
    EXPECT_EQ(2, sink.count);  // the queue is empty, so the second tick delivers nothing
}

// The tick's order is the point of the class: published facts are delivered before any
// controller is polled, so a subscriber never runs from inside the code that published.
TEST(MountCoordinatorTest, TickDeliversQueuedEventsBeforePolling)
{
    Fixture f;
    RecordingSink sink;
    f.coordinator.events().subscribe(&sink);
    f.coordinator.events().publish(MountEvent {MountEventType::TrackingStarted, false});

    const core::TickOutcome outcome = f.coordinator.tick(false);

    EXPECT_EQ(1, sink.count);
    EXPECT_FALSE(outcome.azAltJustStopped);
    EXPECT_FALSE(outcome.guiding);
}

// The AZ/ALT poll is edge-triggered: only the tick that observes the axes stopping reports
// it, because that is the moment the drivers are powered down.
TEST(MountCoordinatorTest, TickReportsTheAzAltStoppedEdgeOnce)
{
    Fixture f;

    EXPECT_FALSE(f.coordinator.tick(true).azAltJustStopped);
    EXPECT_TRUE(f.coordinator.tick(false).azAltJustStopped);
    EXPECT_FALSE(f.coordinator.tick(false).azAltJustStopped);
}

// Same edge rule for the focuser, and it has to survive a detached axis: a board without a
// focuser ticks through here on every pass.
TEST(MountCoordinatorTest, TickReportsTheFocusStoppedEdge)
{
    Fixture f;

    EXPECT_FALSE(f.coordinator.tick(false).focusStopped);  // detached axis: no transition

    fakes::FakeStepperAxis focusAxis;
    f.coordinator.focus().attach(&focusAxis);
    f.coordinator.focus().moveBy(100);
    EXPECT_TRUE(focusAxis.isRunning());

    EXPECT_FALSE(f.coordinator.tick(false).focusStopped);  // seen running
    focusAxis.runToPosition();
    EXPECT_TRUE(f.coordinator.tick(false).focusStopped);   // stopped this tick
    EXPECT_FALSE(f.coordinator.tick(false).focusStopped);  // and only this one
}

// A guide pulse owns the tick: the caller skips the rest of its work, and the tick reports
// which of the two deadlines fired so the pulse is stopped on the right axis.
TEST(MountCoordinatorTest, GuidingTickReportsTheExpiredAxes)
{
    Fixture f;
    fakes::FakeStepperAxis raAxis;
    f.coordinator.tracking().attach(&raAxis);

    EXPECT_FALSE(f.coordinator.tick(false).guiding);

    // A pulse with a deadline still in the future is not expiring yet.
    f.coordinator.state().set(mask(StatusBit::GuidePulse));
    f.coordinator.guiding().startRa(50);
    core::TickOutcome outcome = f.coordinator.tick(false);
    EXPECT_TRUE(outcome.guiding);
    EXPECT_FALSE(outcome.guidingRaExpired);
    EXPECT_FALSE(outcome.guidingDecExpired);  // no DEC pulse was started

    f.clock.advance(51);
    outcome = f.coordinator.tick(false);
    EXPECT_TRUE(outcome.guidingRaExpired);
    EXPECT_FALSE(outcome.guidingDecExpired);
}

// `wasRunning()` is the arrival path's "the axes just stopped" test, so it must only ever see
// what the previous tick observed. `finishTick()` is the one place that writes it.
TEST(MountCoordinatorTest, FinishTickFeedsTheArrivalWasRunningFlag)
{
    Fixture f;

    f.coordinator.finishTick(false, false);
    EXPECT_FALSE(f.coordinator.slew().wasRunning());

    f.coordinator.finishTick(false, true);  // DEC alone is enough
    EXPECT_TRUE(f.coordinator.slew().wasRunning());

    f.coordinator.finishTick(false, false);
    EXPECT_FALSE(f.coordinator.slew().wasRunning());
}

// The six facts the arrival plan is decided from belong to the controllers, so the
// coordinator fills them instead of `Mount` doing it by hand in the middle of the block.
TEST(MountCoordinatorTest, ArrivalFactsComeFromTheOwnedControllers)
{
    Fixture f;

    core::ArrivalFacts idle = f.coordinator.arrivalFacts();
    EXPECT_EQ(SlewIntent::None, idle.intent);
    EXPECT_FALSE(idle.parking);
    EXPECT_FALSE(idle.headingToParkPosition);
    EXPECT_FALSE(idle.findingHome);
    EXPECT_FALSE(idle.compensationPending);
    EXPECT_FALSE(idle.backlashOwed);
    EXPECT_FALSE(idle.parkTargetReached);

    f.coordinator.slew().begin(SlewIntent::Park);
    f.coordinator.slew().requestBacklashCorrection();
    f.coordinator.parking().beginHoming();
    f.coordinator.state().set(mask(StatusBit::FindingHome));

    core::ArrivalFacts facts = f.coordinator.arrivalFacts();
    EXPECT_EQ(SlewIntent::Park, facts.intent);
    EXPECT_TRUE(facts.parking);
    EXPECT_FALSE(facts.headingToParkPosition);  // still on the homing leg
    EXPECT_TRUE(facts.findingHome);
    EXPECT_TRUE(facts.backlashOwed);

    f.coordinator.parking().beginPositionLeg();
    EXPECT_TRUE(f.coordinator.arrivalFacts().headingToParkPosition);
}

// The plan is derived, not stored: the same facts have to give the same plan.
TEST(MountCoordinatorTest, CompletionPlanFollowsTheArrivalFacts)
{
    Fixture f;

    f.coordinator.slew().begin(SlewIntent::Target);
    const core::CompletionPlan plan = f.coordinator.completionPlan();
    EXPECT_TRUE(plan.startTracking);  // a finished go-to resumes tracking
    EXPECT_TRUE(plan.decGuideMicrosteps);

    f.coordinator.parking().beginHoming();
    EXPECT_FALSE(f.coordinator.completionPlan().startTracking);
}

// The capacity has to stay a power of two: on AVR the ring index is a mask, not a divide.
TEST(MountCoordinatorTest, EventQueueIsFourDeepAndPowerOfTwo)
{
    const uint8_t depth = MountCoordinator::kEventQueueDepth;

    EXPECT_EQ(4, static_cast<int>(depth));
    EXPECT_EQ(0, static_cast<int>(depth & (depth - 1)));
}
