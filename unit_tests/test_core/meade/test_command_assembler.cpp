#include <gtest/gtest.h>

#include "core/meade/CommandAssembler.hpp"

using oat::core::meade::CommandAssembler;

namespace
{
using Assembler = CommandAssembler<20>;  // the serial loop's buffer: 19 characters + NUL
using Event     = CommandAssembler<20>::Event;

/// Feeds every byte of `text` and returns the last event it produced.
Event feed(Assembler &assembler, const char *text)
{
    Event last = Event::None;
    for (const char *c = text; *c != '\0'; c++)
    {
        last = assembler.push(static_cast<uint8_t>(*c));
    }
    return last;
}
}  // namespace

TEST(CommandAssemblerTest, CollectsUntilTheTerminator)
{
    Assembler assembler;

    EXPECT_EQ(Event::None, feed(assembler, ":GR"));
    EXPECT_EQ(Event::CommandReady, assembler.push('#'));
    EXPECT_STREQ(":GR", assembler.command());
    EXPECT_EQ(3, static_cast<int>(assembler.length()));
}

// Two commands in one burst: the caller reads one, then the next, with no terminator left over
// in either.
TEST(CommandAssemblerTest, BackToBackCommands)
{
    Assembler assembler;

    EXPECT_EQ(Event::CommandReady, feed(assembler, ":GR#"));
    EXPECT_STREQ(":GR", assembler.command());

    assembler.reset();
    EXPECT_EQ(Event::CommandReady, feed(assembler, ":GD#"));
    EXPECT_STREQ(":GD", assembler.command());
}

// The ACK probe is not part of a command and drops whatever was being assembled — the original
// serial loop reset its index here, so `:G` + ACK + `R#` never produced a command.
TEST(CommandAssemblerTest, AckProbeDropsThePartialCommand)
{
    Assembler assembler;
    feed(assembler, ":G");

    EXPECT_EQ(Event::AckRequested, assembler.push(0x06));
    EXPECT_EQ(0, static_cast<int>(assembler.length()));

    EXPECT_EQ(Event::CommandReady, feed(assembler, "R#"));
    EXPECT_STREQ("R", assembler.command());
}

// CR/LF and other control bytes around a command are ignored rather than becoming part of it.
TEST(CommandAssemblerTest, ControlBytesAreIgnored)
{
    Assembler assembler;

    feed(assembler, "\r\n:GR");
    EXPECT_EQ(Event::CommandReady, assembler.push('#'));
    EXPECT_STREQ(":GR", assembler.command());

    // A trailing CR/LF must not become the start of the next command: the completed one is
    // left untouched.
    feed(assembler, "\r\n");
    EXPECT_STREQ(":GR", assembler.command());
    EXPECT_EQ(3, static_cast<int>(assembler.length()));
}

// An over-long command is dropped whole and the buffer restarts, so a truncated command is
// never executed as if the peer had meant it.
TEST(CommandAssemblerTest, OverlongCommandIsDiscarded)
{
    Assembler assembler;

    // 19 characters is the limit (Capacity - 1) and still fits.
    Event last = Event::None;
    for (int i = 0; i < 19; i++)
    {
        last = assembler.push('A');
    }
    EXPECT_EQ(Event::None, last);
    EXPECT_EQ(Event::CommandReady, assembler.push('#'));
    EXPECT_EQ(19, static_cast<int>(assembler.length()));

    // The 20th character does not fit. The rest of that command is discarded up to its
    // terminator, so no truncated tail is ever dispatched as if it were a command.
    assembler.reset();
    for (int i = 0; i < 19; i++)
    {
        assembler.push('A');
    }
    EXPECT_EQ(Event::Overflow, assembler.push('A'));
    EXPECT_EQ(0, static_cast<int>(assembler.length()));
    EXPECT_EQ(Event::None, feed(assembler, "AAAAA"));  // still inside the discarded command
    EXPECT_EQ(Event::None, assembler.push('#'));       // its terminator ends the discard

    // The next command is assembled normally.
    EXPECT_EQ(Event::CommandReady, feed(assembler, ":GR#"));
    EXPECT_STREQ(":GR", assembler.command());
}

// An empty command (just `#`) is reported as complete and empty; the caller decides what to do
// with it, which is what the old loop did (it dispatched an empty string).
TEST(CommandAssemblerTest, EmptyCommandIsComplete)
{
    Assembler assembler;

    EXPECT_EQ(Event::CommandReady, assembler.push('#'));
    EXPECT_STREQ("", assembler.command());
    EXPECT_EQ(0, static_cast<int>(assembler.length()));
}

TEST(CommandAssemblerTest, ResetClearsTheCommand)
{
    Assembler assembler;
    feed(assembler, ":GR#");

    assembler.reset();

    EXPECT_STREQ("", assembler.command());
    EXPECT_EQ(0, static_cast<int>(assembler.length()));
}
