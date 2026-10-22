#include <gtest/gtest.h>

#include <string>

#include "../test_common/hal_fakes/FakeSerialPort.hpp"
#include "adapters/SerialTransport.hpp"

using adapters::SerialTransport;
using fakes::FakeSerialPort;

namespace
{
/// Reads one command into a caller-sized buffer, or returns "<none>" when there is none.
std::string nextCommand(ports::ITransport &transport, size_t capacity = 32)
{
    char buffer[32] = {0};
    if (!transport.hasCommand())
    {
        return "<none>";
    }
    const size_t length = transport.readCommand(buffer, capacity < sizeof(buffer) ? capacity : sizeof(buffer));
    return std::string(buffer, length);
}
}  // namespace

TEST(SerialTransportTest, ReadsAWholeCommandAndStripsTheTerminator)
{
    FakeSerialPort port;
    SerialTransport transport(port, true);
    port.feed(":GR#");

    EXPECT_TRUE(transport.hasCommand());
    EXPECT_EQ(":GR", nextCommand(transport));
    EXPECT_FALSE(transport.hasCommand());
}

// One burst, two commands: the second must not be lost or merged into the first.
TEST(SerialTransportTest, QueuesBackToBackCommands)
{
    FakeSerialPort port;
    SerialTransport transport(port, true);
    port.feed(":GR#:GD#");

    EXPECT_EQ(":GR", nextCommand(transport));
    EXPECT_EQ(":GD", nextCommand(transport));
    EXPECT_FALSE(transport.hasCommand());
}

// The ACK probe answers `P` immediately and is never delivered as a command.
TEST(SerialTransportTest, AckProbeAnswersAndIsNotACommand)
{
    FakeSerialPort port;
    SerialTransport transport(port, true);
    port.feed(":GR");
    const uint8_t probe = 0x06;
    port.feed(&probe, 1);

    EXPECT_FALSE(transport.hasCommand());
    EXPECT_EQ("P", port.output());
}

// With debug output sharing the port the reply is suppressed, which is what the `#if` in the
// old serial loop did. The probe must still reset the partial command.
TEST(SerialTransportTest, AckProbeCanBeSilent)
{
    FakeSerialPort port;
    SerialTransport transport(port, false);
    port.feed(":G");
    const uint8_t probe = 0x06;
    port.feed(&probe, 1);

    EXPECT_FALSE(transport.hasCommand());
    EXPECT_EQ("", port.output());

    port.feed("R#");
    EXPECT_EQ("R", nextCommand(transport));  // the partial `:G` was dropped by the probe
}

TEST(SerialTransportTest, WritesTheResponse)
{
    FakeSerialPort port;
    SerialTransport transport(port, true);

    transport.writeResponse("22:31:04#");

    EXPECT_EQ("22:31:04#", port.output());
}

// An empty response is a no-op, so callers need no guard of their own.
TEST(SerialTransportTest, EmptyResponseWritesNothing)
{
    FakeSerialPort port;
    SerialTransport transport(port, true);

    transport.writeResponse("");
    transport.writeResponse(nullptr);

    EXPECT_EQ("", port.output());
}

// An over-long command is dropped whole and reported once, so the caller can log it.
TEST(SerialTransportTest, OverlongCommandIsDroppedAndReported)
{
    FakeSerialPort port;
    SerialTransport transport(port, true);

    std::string longCommand = ":";
    longCommand.append(25, 'A');
    longCommand += "#";
    port.feed(longCommand.c_str());

    EXPECT_FALSE(transport.hasCommand());
    EXPECT_TRUE(transport.takeOverflow());
    EXPECT_FALSE(transport.takeOverflow());
}

// A caller buffer that is too small drops the command instead of truncating it: a truncated
// LX200 command could be a different, valid command.
TEST(SerialTransportTest, CommandTooBigForTheCallerBufferIsDropped)
{
    FakeSerialPort port;
    SerialTransport transport(port, true);
    port.feed(":XSDLUnnnnn#");

    char small[4] = {0};
    EXPECT_TRUE(transport.hasCommand());
    EXPECT_EQ(0u, transport.readCommand(small, sizeof(small)));
    EXPECT_FALSE(transport.hasCommand());
}

// CR/LF around a command must not end up inside it.
TEST(SerialTransportTest, IgnoresLineEndings)
{
    FakeSerialPort port;
    SerialTransport transport(port, true);
    port.feed("\r\n:GR#\r\n");

    EXPECT_EQ(":GR", nextCommand(transport));
    EXPECT_FALSE(transport.hasCommand());
}
