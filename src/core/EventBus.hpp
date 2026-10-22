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
///
/// `Sinks` is a fixed slot count rather than one pointer: there are two consumers now, the
/// command layer (waiting for a slew, homing or focus move to finish so it can answer a
/// deferred Meade reply) and, since Phase 6, the status display. A single-sink bus would let
/// the second `subscribe()` silently displace the first, and the loss would be invisible:
/// the displaced subscriber simply never hears anything again.
template <uint8_t Capacity, uint8_t Sinks = 2> class EventBus
{
    static_assert(Capacity != 0, "an event bus needs room for at least one event");
    static_assert((Capacity & (Capacity - 1)) == 0, "capacity must be a power of two");
    static_assert(Sinks != 0, "an event bus needs room for at least one subscriber");

  public:
    /// Adds a subscriber. The same sink twice is a no-op rather than a duplicate delivery, and
    /// a full bus drops the newcomer: a subscriber that cannot be registered is a
    /// configuration mistake the caller sees through `hasSubscriber()`, not a runtime fault.
    void subscribe(IEventSink *sink)
    {
        if ((sink == nullptr) || hasSubscriber(sink))
        {
            return;
        }

        for (uint8_t i = 0; i < Sinks; i++)
        {
            if (_sinks[i] == nullptr)
            {
                _sinks[i] = sink;
                return;
            }
        }
    }

    /// Removes one subscriber, leaving the others in place.
    void unsubscribe(IEventSink *sink)
    {
        for (uint8_t i = 0; i < Sinks; i++)
        {
            if (_sinks[i] == sink)
            {
                _sinks[i] = nullptr;
            }
        }
    }

    /// Removes every subscriber. Used by the composition root when the whole graph goes away.
    void unsubscribe()
    {
        for (uint8_t i = 0; i < Sinks; i++)
        {
            _sinks[i] = nullptr;
        }
    }

    bool hasSubscriber() const
    {
        for (uint8_t i = 0; i < Sinks; i++)
        {
            if (_sinks[i] != nullptr)
            {
                return true;
            }
        }
        return false;
    }

    bool hasSubscriber(const IEventSink *sink) const
    {
        for (uint8_t i = 0; i < Sinks; i++)
        {
            if (_sinks[i] == sink)
            {
                return true;
            }
        }
        return false;
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
        if (!hasSubscriber() || _dispatching)
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
            for (uint8_t i = 0; i < Sinks; i++)
            {
                if (_sinks[i] != nullptr)
                {
                    _sinks[i]->onMountEvent(event);
                }
            }
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
    IEventSink *_sinks[Sinks]   = {};
    bool _dispatching           = false;
};

}  // namespace core
