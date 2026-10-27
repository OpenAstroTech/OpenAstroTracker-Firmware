#include <gtest/gtest.h>

#include <vector>

#include "core/EventBus.hpp"

namespace
{
class RecordingSink final : public ports::IEventSink
{
  public:
    void onMountEvent(const types::MountEvent &event) override
    {
        _events.push_back(event);
    }

    const std::vector<types::MountEvent> &events() const
    {
        return _events;
    }

    void clear()
    {
        _events.clear();
    }

  private:
    std::vector<types::MountEvent> _events;
};

types::MountEvent event(types::MountEventType type)
{
    types::MountEvent e;
    e.type      = type;
    e.succeeded = false;
    return e;
}
}  // namespace

TEST(EventBusTest, PublishesNothingUntilDispatched)
{
    core::EventBus<4> bus;
    RecordingSink sink;
    bus.subscribe(&sink);

    bus.publish(event(types::MountEventType::SlewStarted));

    EXPECT_TRUE(sink.events().empty());
    EXPECT_EQ(1, bus.pending());

    bus.dispatch();

    ASSERT_EQ(1u, sink.events().size());
    EXPECT_EQ(types::MountEventType::SlewStarted, sink.events()[0].type);
    EXPECT_EQ(0, bus.pending());
}

TEST(EventBusTest, DeliversInPublishOrder)
{
    core::EventBus<4> bus;
    RecordingSink sink;
    bus.subscribe(&sink);

    bus.publish(event(types::MountEventType::SlewStarted));
    bus.publish(event(types::MountEventType::SlewArrived));
    bus.publish(event(types::MountEventType::Parked));
    bus.dispatch();

    ASSERT_EQ(3u, sink.events().size());
    EXPECT_EQ(types::MountEventType::SlewStarted, sink.events()[0].type);
    EXPECT_EQ(types::MountEventType::SlewArrived, sink.events()[1].type);
    EXPECT_EQ(types::MountEventType::Parked, sink.events()[2].type);
}

TEST(EventBusTest, CarriesTheOutcome)
{
    core::EventBus<4> bus;
    RecordingSink sink;
    bus.subscribe(&sink);

    types::MountEvent homed = event(types::MountEventType::HomingFinished);
    homed.succeeded        = true;
    bus.publish(homed);
    bus.dispatch();

    ASSERT_EQ(1u, sink.events().size());
    EXPECT_TRUE(sink.events()[0].succeeded);
}

TEST(EventBusTest, OverflowDropsTheOldestEvent)
{
    core::EventBus<4> bus;
    RecordingSink sink;
    bus.subscribe(&sink);

    bus.publish(event(types::MountEventType::TrackingStarted));
    bus.publish(event(types::MountEventType::TrackingStopped));
    bus.publish(event(types::MountEventType::SlewStarted));
    bus.publish(event(types::MountEventType::SlewArrived));
    bus.publish(event(types::MountEventType::Parked));  // oldest is pushed out

    EXPECT_EQ(4, bus.pending());
    bus.dispatch();

    ASSERT_EQ(4u, sink.events().size());
    EXPECT_EQ(types::MountEventType::TrackingStopped, sink.events()[0].type);
    EXPECT_EQ(types::MountEventType::Parked, sink.events()[3].type);
}

TEST(EventBusTest, TheQueueKeepsWorkingAfterAnOverflowWrapped)
{
    core::EventBus<4> bus;
    RecordingSink sink;
    bus.subscribe(&sink);

    for (int i = 0; i < 6; i++)
    {
        bus.publish(event(types::MountEventType::FocusMoving));
    }
    bus.dispatch();
    ASSERT_EQ(4u, sink.events().size());
    EXPECT_EQ(types::MountEventType::FocusMoving, sink.events()[0].type);

    sink.clear();
    bus.publish(event(types::MountEventType::FocusStopped));
    bus.dispatch();

    ASSERT_EQ(1u, sink.events().size());
    EXPECT_EQ(types::MountEventType::FocusStopped, sink.events()[0].type);
}

TEST(EventBusTest, PublishingWithoutASubscriberIsHarmless)
{
    core::EventBus<4> bus;

    bus.publish(event(types::MountEventType::Parked));
    bus.dispatch();

    EXPECT_FALSE(bus.hasSubscriber());
}

TEST(EventBusTest, UnsubscribeStopsDelivery)
{
    core::EventBus<4> bus;
    RecordingSink sink;
    bus.subscribe(&sink);
    bus.publish(event(types::MountEventType::Parked));

    bus.unsubscribe();
    bus.dispatch();

    EXPECT_TRUE(sink.events().empty());
}

TEST(EventBusTest, AnEventPublishedDuringDispatchWaitsForTheNextTick)
{
    // A subscriber that publishes must not re-enter the publisher: the new event is
    // queued and delivered on the next dispatch, never inside this one.
    class EchoSink final : public ports::IEventSink
    {
      public:
        explicit EchoSink(core::EventBus<4> &bus) : _bus(bus)
        {
        }

        void onMountEvent(const types::MountEvent &received) override
        {
            if (!_echoed && (received.type == types::MountEventType::SlewArrived))
            {
                _echoed = true;
                _bus.publish(types::MountEvent {types::MountEventType::Parked, false});
            }
            _delivered++;
        }

        int delivered() const
        {
            return _delivered;
        }

      private:
        core::EventBus<4> &_bus;
        bool _echoed   = false;
        int _delivered = 0;
    };

    core::EventBus<4> bus;
    EchoSink sink(bus);
    bus.subscribe(&sink);
    bus.publish(event(types::MountEventType::SlewArrived));

    bus.dispatch();
    EXPECT_EQ(1, sink.delivered());
    EXPECT_EQ(1, bus.pending());

    bus.dispatch();
    EXPECT_EQ(2, sink.delivered());
    EXPECT_EQ(0, bus.pending());
}

TEST(EventBusTest, DispatchWithoutAPendingEventDoesNothing)
{
    core::EventBus<4> bus;
    RecordingSink sink;
    bus.subscribe(&sink);

    bus.dispatch();

    EXPECT_TRUE(sink.events().empty());
}

TEST(EventBusTest, DeliversToEverySubscriber)
{
    // Two consumers exist: the command layer waiting on a deferred reply and the status
    // display. A single-sink bus would let the second subscribe silently displace the first.
    core::EventBus<4> bus;
    RecordingSink first;
    RecordingSink second;
    bus.subscribe(&first);
    bus.subscribe(&second);

    bus.publish(event(types::MountEventType::SlewArrived));
    bus.dispatch();

    ASSERT_EQ(1u, first.events().size());
    ASSERT_EQ(1u, second.events().size());
    EXPECT_EQ(types::MountEventType::SlewArrived, second.events()[0].type);
}

TEST(EventBusTest, SubscribingTheSameSinkTwiceDeliversOnce)
{
    core::EventBus<4> bus;
    RecordingSink sink;
    bus.subscribe(&sink);
    bus.subscribe(&sink);

    bus.publish(event(types::MountEventType::Parked));
    bus.dispatch();

    EXPECT_EQ(1u, sink.events().size());
}

TEST(EventBusTest, UnsubscribingOneSinkLeavesTheOther)
{
    core::EventBus<4> bus;
    RecordingSink leaver;
    RecordingSink stayer;
    bus.subscribe(&leaver);
    bus.subscribe(&stayer);

    bus.unsubscribe(&leaver);
    bus.publish(event(types::MountEventType::Parked));
    bus.dispatch();

    EXPECT_TRUE(leaver.events().empty());
    EXPECT_EQ(1u, stayer.events().size());
    EXPECT_TRUE(bus.hasSubscriber());
}

TEST(EventBusTest, TheSlotsAreBoundedAndTheFirstSubscribersWin)
{
    core::EventBus<4, 2> bus;
    RecordingSink first;
    RecordingSink second;
    RecordingSink third;
    bus.subscribe(&first);
    bus.subscribe(&second);
    bus.subscribe(&third);  // no room

    EXPECT_FALSE(bus.hasSubscriber(&third));

    bus.publish(event(types::MountEventType::Parked));
    bus.dispatch();

    EXPECT_EQ(1u, first.events().size());
    EXPECT_EQ(1u, second.events().size());
    EXPECT_TRUE(third.events().empty());
}
