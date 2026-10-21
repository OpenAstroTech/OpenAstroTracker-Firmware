#include <gtest/gtest.h>

#include <vector>

#include "core/EventBus.hpp"

namespace
{
class RecordingSink final : public core::IEventSink
{
  public:
    void onMountEvent(const core::MountEvent &event) override
    {
        _events.push_back(event);
    }

    const std::vector<core::MountEvent> &events() const
    {
        return _events;
    }

    void clear()
    {
        _events.clear();
    }

  private:
    std::vector<core::MountEvent> _events;
};

core::MountEvent event(core::MountEventType type)
{
    core::MountEvent e;
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

    bus.publish(event(core::MountEventType::SlewStarted));

    EXPECT_TRUE(sink.events().empty());
    EXPECT_EQ(1, bus.pending());

    bus.dispatch();

    ASSERT_EQ(1u, sink.events().size());
    EXPECT_EQ(core::MountEventType::SlewStarted, sink.events()[0].type);
    EXPECT_EQ(0, bus.pending());
}

TEST(EventBusTest, DeliversInPublishOrder)
{
    core::EventBus<4> bus;
    RecordingSink sink;
    bus.subscribe(&sink);

    bus.publish(event(core::MountEventType::SlewStarted));
    bus.publish(event(core::MountEventType::SlewArrived));
    bus.publish(event(core::MountEventType::Parked));
    bus.dispatch();

    ASSERT_EQ(3u, sink.events().size());
    EXPECT_EQ(core::MountEventType::SlewStarted, sink.events()[0].type);
    EXPECT_EQ(core::MountEventType::SlewArrived, sink.events()[1].type);
    EXPECT_EQ(core::MountEventType::Parked, sink.events()[2].type);
}

TEST(EventBusTest, CarriesTheOutcome)
{
    core::EventBus<4> bus;
    RecordingSink sink;
    bus.subscribe(&sink);

    core::MountEvent homed = event(core::MountEventType::HomingFinished);
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

    bus.publish(event(core::MountEventType::TrackingStarted));
    bus.publish(event(core::MountEventType::TrackingStopped));
    bus.publish(event(core::MountEventType::SlewStarted));
    bus.publish(event(core::MountEventType::SlewArrived));
    bus.publish(event(core::MountEventType::Parked));  // oldest is pushed out

    EXPECT_EQ(4, bus.pending());
    bus.dispatch();

    ASSERT_EQ(4u, sink.events().size());
    EXPECT_EQ(core::MountEventType::TrackingStopped, sink.events()[0].type);
    EXPECT_EQ(core::MountEventType::Parked, sink.events()[3].type);
}

TEST(EventBusTest, TheQueueKeepsWorkingAfterAnOverflowWrapped)
{
    core::EventBus<4> bus;
    RecordingSink sink;
    bus.subscribe(&sink);

    for (int i = 0; i < 6; i++)
    {
        bus.publish(event(core::MountEventType::FocusMoving));
    }
    bus.dispatch();
    ASSERT_EQ(4u, sink.events().size());
    EXPECT_EQ(core::MountEventType::FocusMoving, sink.events()[0].type);

    sink.clear();
    bus.publish(event(core::MountEventType::FocusStopped));
    bus.dispatch();

    ASSERT_EQ(1u, sink.events().size());
    EXPECT_EQ(core::MountEventType::FocusStopped, sink.events()[0].type);
}

TEST(EventBusTest, PublishingWithoutASubscriberIsHarmless)
{
    core::EventBus<4> bus;

    bus.publish(event(core::MountEventType::Parked));
    bus.dispatch();

    EXPECT_FALSE(bus.hasSubscriber());
}

TEST(EventBusTest, UnsubscribeStopsDelivery)
{
    core::EventBus<4> bus;
    RecordingSink sink;
    bus.subscribe(&sink);
    bus.publish(event(core::MountEventType::Parked));

    bus.unsubscribe();
    bus.dispatch();

    EXPECT_TRUE(sink.events().empty());
}

TEST(EventBusTest, AnEventPublishedDuringDispatchWaitsForTheNextTick)
{
    // A subscriber that publishes must not re-enter the publisher: the new event is
    // queued and delivered on the next dispatch, never inside this one.
    class EchoSink final : public core::IEventSink
    {
      public:
        explicit EchoSink(core::EventBus<4> &bus) : _bus(bus)
        {
        }

        void onMountEvent(const core::MountEvent &received) override
        {
            if (!_echoed && (received.type == core::MountEventType::SlewArrived))
            {
                _echoed = true;
                _bus.publish(core::MountEvent {core::MountEventType::Parked, false});
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
    bus.publish(event(core::MountEventType::SlewArrived));

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
