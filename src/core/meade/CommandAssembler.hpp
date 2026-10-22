#pragma once

#include <stdint.h>

namespace oat
{
namespace core
{
namespace meade
{

/// Turns a byte stream into Meade commands.
///
/// The LX200 framing this firmware speaks is trivial and was nevertheless written out twice
/// inline — once for serial ([`f_serial.hpp`](../../f_serial.hpp)) and once for Wi-Fi — with
/// its own buffer, its own overflow rule and its own ACK handling. That is the kind of code
/// where the two copies drift and nobody notices until a host driver behaves differently over
/// Wi-Fi than over the cable. It lives here instead, host-tested, and the transports only move
/// bytes.
///
/// Faithful to the original serial loop on purpose:
/// * `0x06` is the ACK probe. It **discards** whatever was being assembled — the old code
///   reset its index here too, so a command interrupted by an ACK was never completed.
/// * `#` terminates the command. The terminator is not part of it.
/// * Other control bytes (below `' '`) are ignored, so CR/LF around commands are harmless.
/// * A command longer than the buffer is discarded **up to its terminator**, not just up to the
///   overflow point. The original serial loop kept going after the overflow and dispatched
///   whatever tail arrived before the `#`, so an over-long command could execute as a shorter,
///   valid one. Discarding the rest of it is the one deliberate behaviour change here.
///
/// `Capacity` includes the terminating NUL, so `Capacity == 20` accepts 19-character commands,
/// which is what the serial loop accepted before.
template <uint8_t Capacity> class CommandAssembler
{
    static_assert(Capacity >= 2, "a command buffer needs room for a character and a NUL");

  public:
    static constexpr uint8_t kAckProbe = 0x06;
    static constexpr char kTerminator  = '#';

    /// What a byte did to the assembler.
    enum class Event : uint8_t
    {
        None,          ///< appended (or ignored, for control bytes)
        AckRequested,  ///< the peer asked for the ACK reply; the partial command was dropped
        CommandReady,  ///< `command()` now holds a complete command
        Overflow,      ///< the command was too long; it was dropped
    };

    Event push(uint8_t value)
    {
        if (value == kAckProbe)
        {
            _length = 0;
            return Event::AckRequested;
        }

        if (value == static_cast<uint8_t>(kTerminator))
        {
            if (_discarding)
            {
                _discarding = false;
                return Event::None;
            }
            _command[_length] = '\0';
            return Event::CommandReady;
        }

        if (value < static_cast<uint8_t>(' '))
        {
            return Event::None;
        }

        if (_discarding)
        {
            return Event::None;
        }

        if (_length >= Capacity - 1)
        {
            _length     = 0;
            _discarding = true;
            return Event::Overflow;
        }

        _command[_length++] = static_cast<char>(value);
        return Event::None;
    }

    /// The last completed command, NUL-terminated. Empty after a reset.
    const char *command() const
    {
        return _command;
    }

    uint8_t length() const
    {
        return _length;
    }

    /// Drops any partial command, e.g. after the caller has consumed the last one.
    void reset()
    {
        _length     = 0;
        _discarding = false;
        _command[0] = '\0';
    }

  private:
    char _command[Capacity] = {0};
    uint8_t _length         = 0;
    bool _discarding        = false;  ///< the current command overflowed; skip to its terminator
};

}  // namespace meade
}  // namespace core
}  // namespace oat
