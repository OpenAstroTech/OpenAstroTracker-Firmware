#pragma once

#include <stdint.h>

#include "types/MountEvent.hpp"
#include "ports/ITransport.hpp"

namespace adapters
{
namespace meade
{
/// What a deferred command owes its host when the work behind it finishes.
///
/// The table stores the *shape* of the answer, not the answer: formatting the bytes is the
/// adapter's job (it owns the wire), and keeping that out of the table is what lets the
/// table be a pure, exhaustive-testable piece of core.
enum class ReplyKind : uint8_t
{
    /// The command answers nothing either way (`:Q#`). The slot still exists so the peer's
    /// transport is released and the deadline is observed.
    Silent,
    /// `1` when the work finished, `0` when it failed or the deadline passed. This is the
    /// shape the LX200 protocol gives a set/action command whose effect takes time.
    SuccessFlag,
};

/// One reply that is still owed.
struct PendingReply {
    /// Identity of this deferral, monotonically increasing and never 0. Handlers and tests use
    /// it to say *which* deferral they mean; the wire never sees it.
    uint16_t correlation;
    /// Where the answer goes. Stored per entry, so a reply started on Wi-Fi can never be
    /// written to the serial port that issued the next command.
    ports::ITransport *out;
    /// Which mount event completes the work.
    ::types::MountEventType completesOn;
    /// What to say when it does.
    ReplyKind kind;
    /// `IClock` milliseconds after which the reply is answered as failed.
    uint32_t deadlineMs;
};

/// The deferred-reply bookkeeping: a fixed table of replies owed to a host.
///
/// Why a table at all: a command whose effect takes time (a GPS fix, a move that ends when an
/// axis stops) cannot answer inside the handler without blocking the main loop, and answering
/// with an empty string would look to the host like a command that has no reply at all. The
/// distinction has to be held somewhere between two ticks, so it is held here.
///
/// Fixed capacity and no allocation, like everything else in `core/`: the ATmega2560 has 8 KB
/// of SRAM and no allocator worth the risk. Four slots is enough for the commands that can be
/// in flight at once — the protocol is request/response per peer, and the events that
/// complete these replies are owner-exclusive (one slew, one focus move, one home).
///
/// Matching is by event type, not by correlation: `MountEvent` deliberately carries no
/// correlation id (it is two bytes so a queue of them fits in SRAM), so a deferral says which
/// event finishes it. Two entries waiting on the same event are completed oldest-first,
/// which is the only order that matches the wire.
template <uint8_t Capacity = 4> class PendingReplies
{
    static_assert(Capacity != 0, "a pending-reply table needs room for at least one reply");

  public:
    /// Reserves a slot. Returns the correlation id, or 0 when the table is full — the caller
    /// must then answer the command immediately rather than leave the host waiting forever.
    uint16_t add(ports::ITransport *out, ReplyKind kind, ::types::MountEventType completesOn, uint32_t deadlineMs)
    {
        for (uint8_t i = 0; i < Capacity; i++)
        {
            if (_entries[i].out != nullptr)
            {
                continue;
            }

            _nextCorrelation    = (_nextCorrelation == 0xFFFFu) ? 1u : (uint16_t) (_nextCorrelation + 1u);
            PendingReply &entry = _entries[i];
            entry.correlation   = _nextCorrelation;
            entry.out           = out;
            entry.completesOn   = completesOn;
            entry.kind          = kind;
            entry.deadlineMs    = deadlineMs;
            return entry.correlation;
        }

        return 0;
    }

    /// Takes the oldest entry that `event` completes, if any.
    bool takeCompleted(::types::MountEventType event, PendingReply &out)
    {
        for (uint8_t i = 0; i < Capacity; i++)
        {
            if ((_entries[i].out != nullptr) && (_entries[i].completesOn == event))
            {
                out = _entries[i];
                release(i);
                return true;
            }
        }

        return false;
    }

    /// Takes the oldest entry whose deadline has passed. `nowMs` comes from the caller's clock;
    /// the table never reads time itself, which is what makes the expiry testable.
    ///
    /// A deadline is inclusive: at `deadlineMs` the host has waited exactly as long as it was
    /// promised, so the failure reply is due.
    bool takeExpired(uint32_t nowMs, PendingReply &out)
    {
        for (uint8_t i = 0; i < Capacity; i++)
        {
            if ((_entries[i].out != nullptr) && (static_cast<int32_t>(nowMs - _entries[i].deadlineMs) >= 0))
            {
                out = _entries[i];
                release(i);
                return true;
            }
        }

        return false;
    }

    /// Removes one entry by correlation id. Used when the peer goes away, and by tests.
    bool take(uint16_t correlation, PendingReply &out)
    {
        for (uint8_t i = 0; i < Capacity; i++)
        {
            if ((_entries[i].out != nullptr) && (_entries[i].correlation == correlation))
            {
                out = _entries[i];
                release(i);
                return true;
            }
        }

        return false;
    }

    /// True when a reply with this correlation is still owed.
    bool contains(uint16_t correlation) const
    {
        for (uint8_t i = 0; i < Capacity; i++)
        {
            if ((_entries[i].out != nullptr) && (_entries[i].correlation == correlation))
            {
                return true;
            }
        }
        return false;
    }

    uint8_t size() const
    {
        uint8_t used = 0;
        for (uint8_t i = 0; i < Capacity; i++)
        {
            if (_entries[i].out != nullptr)
            {
                used++;
            }
        }
        return used;
    }

    bool empty() const
    {
        return size() == 0;
    }

    /// Forgets every pending reply without answering it. Used when the owning transport is
    /// gone, where writing a reply has nowhere to land.
    void clear()
    {
        for (uint8_t i = 0; i < Capacity; i++)
        {
            release(i);
        }
    }

    /// Forgets the replies owed to one transport. Called when a peer disconnects: the answers
    /// are not wrong, they are undeliverable.
    void clearFor(const ports::ITransport *out)
    {
        for (uint8_t i = 0; i < Capacity; i++)
        {
            if (_entries[i].out == out)
            {
                release(i);
            }
        }
    }

    static constexpr uint8_t capacity()
    {
        return Capacity;
    }

  private:
    void release(uint8_t index)
    {
        _entries[index].out = nullptr;
    }

    PendingReply _entries[Capacity] = {};
    uint16_t _nextCorrelation       = 0;
};

}  // namespace meade
}  // namespace adapters
