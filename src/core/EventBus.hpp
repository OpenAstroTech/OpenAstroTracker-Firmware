#pragma once

#include <stdint.h>

#include "MountEvent.hpp"

namespace core
{

/// A fixed-capacity, heap-free queue of mount events, delivered at the tick boundary.
///
/// Why a queue at all: a subscriber must never be able to re-enter the code that just
/// published. `publish()` only records; `dispatch()` delivers, and a subscriber that
/// publishes while being called simply queues its event for the next dispatch. That is
/// what makes it safe for the publisher to be in the middle of changing its own state.
///
/// Capacity is a template parameter and must be a power of two: the index arithmetic is
/// a mask rather than a modulo, because the ATmega2560 has no divide instruction.
/// Overflow drops the **oldest** event: for a status display the newest fact is the one
/// worth showing.
///
/// Nothing here allocates, locks or blocks, so the same object works on every platform.
template <uint8_t Capacity> class EventBus
{
    static_assert(Capacity != 0, "an event bus needs room for at least one event");
    static_assert((Capacity & (Capacity - 1)) == 0, "capacity must be a power of two");

  public:
    /// One subscriber. Mount has exactly one consumer today (the status display), so the
    /// bus keeps a single pointer instead of a list.
    void subscribe(IEventSink *sink)
    {
        _sink = sink;
    }

    void unsubscribe()
    {
        _sink = nullptr;
    }

    bool hasSubscriber() const
    {
        return _sink != nullptr;
    }

    void publish(const MountEvent &event)
    {
        if (_count == Capacity)
        {
            _head = next(_head);
            _count--;
        }
        _queue[_tail] = event;
        _tail         = next(_tail);
        _count++;
    }

    /// Delivers everything queued. Called once per main-loop tick, never from an ISR.
    void dispatch()
    {
        if ((_sink == nullptr) || _dispatching)
        {
            return;
        }

        // The batch size is snapshotted first: an event published by a subscriber while
        // it is being called waits for the next tick instead of extending this one.
        // Without that, a subscriber that publishes on every event would spin here
        // forever, and a publisher would be re-entered before it finished updating.
        _dispatching      = true;
        uint8_t toDeliver = _count;
        while (toDeliver-- != 0)
        {
            const MountEvent event = _queue[_head];
            _head                  = next(_head);
            _count--;
            _sink->onMountEvent(event);
        }
        _dispatching = false;
    }

    uint8_t pending() const
    {
        return _count;
    }

  private:
    static uint8_t next(uint8_t index)
    {
        return (uint8_t) ((index + 1u) & (Capacity - 1u));
    }

    MountEvent _queue[Capacity] = {};
    uint8_t _head               = 0;
    uint8_t _tail               = 0;
    uint8_t _count              = 0;
    IEventSink *_sink           = nullptr;
    bool _dispatching           = false;
};

}  // namespace core
