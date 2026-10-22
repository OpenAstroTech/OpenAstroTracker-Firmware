#include <gtest/gtest.h>

#include "../test_common/hal_fakes/FakeSystemClock.hpp"
#include "core/MountCoordinator.hpp"

using core::Admission;
using core::MountCoordinator;
using core::MountEvent;
using core::MountEventType;
using core::MountRequest;
using core::ParkingPhase;
using core::SlewIntent;
using core::StatusBit;

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
    MountCoordinator<> coordinator {&clock, kSiderealSecondsPerDay, kMicrostepRatio};

    void track()
    {
        coordinator.state().set(mask(StatusBit::Tracking));
    }
};

/// Collects what the coordinator's event bus delivers.
class RecordingSink final : public core::IEventSink
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
    const MountCoordinator<> &view = f.coordinator;
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

// The capacity has to stay a power of two: on AVR the ring index is a mask, not a divide.
TEST(MountCoordinatorTest, EventQueueIsFourDeepAndPowerOfTwo)
{
    const uint8_t depth = MountCoordinator<>::kEventQueueDepth;

    EXPECT_EQ(4, static_cast<int>(depth));
    EXPECT_EQ(0, static_cast<int>(depth & (depth - 1)));
}
