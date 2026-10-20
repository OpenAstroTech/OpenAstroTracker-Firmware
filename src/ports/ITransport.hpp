#pragma once

#include <stddef.h>

namespace ports
{

/// Where Meade commands come from and where their replies go: one serial port, one
/// TCP client, or one UDP datagram.
///
/// The port is line-oriented because that is the Meade LX200 protocol's framing. The
/// transport owns buffering and framing; the parser owns interpretation.
class ITransport
{
  public:
    virtual ~ITransport()
    {
    }

    /// True when at least one complete command is buffered.
    virtual bool hasCommand() = 0;

    /// Copies the next command, without its terminator, into `buffer` and returns its
    /// length. Returns 0 when no complete command is buffered; the command is
    /// discarded when it does not fit.
    virtual size_t readCommand(char *buffer, size_t capacity) = 0;

    /// Sends the reply to the peer that issued the current command.
    virtual void writeResponse(const char *response) = 0;
};

}  // namespace ports
