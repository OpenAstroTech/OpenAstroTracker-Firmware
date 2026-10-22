// The command pipeline as the firmware runs it, minus the firmware.
//
// Every other Meade test starts at `dispatchMeadeCommand` with a string. The bugs that survive
// that are the ones in between — a command split across reads, the terminator left in the
// buffer, an over-long command executing as a shorter one, a reply sent to the wrong peer, a
// deferred reply that never arrives. This file drives the real `SerialTransport` (framing and
// ACK) into the real parser, and takes the answer back out of the same transport, so what is
// asserted is the bytes a host would actually see.
//
// The deferred half is exercised with the real `PendingReplies` table and a handler that hands
// the reply over instead of answering inline: one case completes on a mount event, the other on
// the deadline, because those are the two ways a deferred reply can end.

#include <gtest/gtest.h>

#include <string>

#include "adapters/SerialTransport.hpp"
#include "core/meade/MeadeParser.hpp"
#include "core/meade/PendingReplies.hpp"
#include "../test_common/hal_fakes/FakeSerialPort.hpp"
#include "../test_common/meade_fakes/StubMeadeHandlers.hpp"

namespace meade = oat::core::meade;

namespace
{
using Transport = adapters::SerialTransport<20>;

/// The pipeline under test: one transport, one parser, one handler set, and the reply table the
/// adapter keeps. It mirrors `MeadeCommandAdapter::processCommand()` line for line where it
/// matters: dispatch into a response object, publish the bytes unless the handler deferred.
class Pipeline
{
  public:
    Pipeline() : _transport(_port, true)
    {
    }

    /// Feeds bytes as a host would and returns everything the firmware sent back.
    std::string exchange(const char *bytes)
    {
        _port.feed(bytes);
        drain();
        return _port.output();
    }

    /// Delivers queued input without asserting anything about the reply.
    void drain()
    {
        while (_transport.hasCommand())
        {
            char buffer[Transport::kCommandCapacity] = {0};
            const size_t length                      = _transport.readCommand(buffer, sizeof(buffer));
            if (length == 0)
            {
                continue;
            }

            _response.clear();
            _response.clearDeferral();
            _handlers.deferred = false;
            meade::dispatchMeadeCommand(_response, buffer, _handlers);

            // The adapter tracks the deferral itself (a handler cannot reach the response
            // object), so both have to be consulted, exactly as `processCommand()` does.
            if (!_handlers.deferred && !_response.deferred())
            {
                _transport.writeResponse(_response.c_str());
            }
        }
    }

    void deliverEvent(::core::MountEventType type, bool succeeded = false)
    {
        ::core::MountEvent event;
        event.type      = type;
        event.succeeded = succeeded;

        meade::PendingReply reply;
        while (_pending.takeCompleted(event.type, reply))
        {
            writeReply(reply, succeeded);
        }
    }

    void advanceTo(uint32_t nowMs)
    {
        meade::PendingReply reply;
        while (_pending.takeExpired(nowMs, reply))
        {
            writeReply(reply, false);
        }
    }

    void writeReply(const meade::PendingReply &reply, bool succeeded)
    {
        switch (reply.kind)
        {
            case meade::ReplyKind::SuccessFlag:
                reply.out->writeResponse(succeeded ? "1" : "0");
                break;
            case meade::ReplyKind::Silent:
                break;
        }
    }

    fakes::FakeSerialPort &port()
    {
        return _port;
    }

    fakes::StubMeadeHandlers &handlers()
    {
        return _handlers;
    }

    meade::PendingReplies<> &pending()
    {
        return _pending;
    }

    Transport &transport()
    {
        return _transport;
    }

  private:
    fakes::FakeSerialPort _port;
    Transport _transport;
    fakes::StubMeadeHandlers _handlers;
    meade::PendingReplies<> _pending;
    meade::MeadeResponse _response;
};

/// Answers a command later instead of inline, the way a handler with real work to do does.
class DeferredHandlers : public fakes::StubMeadeHandlers
{
  public:
    DeferredHandlers(meade::PendingReplies<> &pending,
                     ports::ITransport &out,
                     meade::ReplyKind kind,
                     ::core::MountEventType completesOn,
                     uint32_t deadlineMs)
        : _pending(pending), _out(out), _kind(kind), _completesOn(completesOn), _deadlineMs(deadlineMs)
    {
    }

    void onPark() override
    {
        lastCall     = "onPark";
        deferred     = true;
        _correlation = _pending.add(&_out, _kind, _completesOn, _deadlineMs);
    }

    uint16_t correlation() const
    {
        return _correlation;
    }

  private:
    meade::PendingReplies<> &_pending;
    ports::ITransport &_out;
    meade::ReplyKind _kind;
    ::core::MountEventType _completesOn;
    uint32_t _deadlineMs;
    uint16_t _correlation = 0;
};
}  // namespace

TEST(MeadePipelineTest, AWholeCommandAnswersThroughTheTransportItArrivedOn)
{
    Pipeline pipeline;

    EXPECT_EQ("V1.2.3#", pipeline.exchange(":GVN#"));
}

TEST(MeadePipelineTest, ACommandSplitAcrossReadsIsAnsweredOnceComplete)
{
    Pipeline pipeline;

    EXPECT_EQ("", pipeline.exchange(":GV"));
    EXPECT_EQ("", pipeline.exchange("N"));
    EXPECT_EQ("V1.2.3#", pipeline.exchange("#"));
}

TEST(MeadePipelineTest, BackToBackCommandsInOneBurstAreBothAnswered)
{
    Pipeline pipeline;

    // `:Q#` answers nothing at all, so the only bytes are the second command's reply.
    EXPECT_EQ("V1.2.3#", pipeline.exchange(":Q#:GVN#"));
}

TEST(MeadePipelineTest, TheTerminatorIsNotPartOfThePayload)
{
    Pipeline pipeline;

    pipeline.exchange(":GVN#");

    EXPECT_EQ("onFirmwareVersion", std::string(pipeline.handlers().lastCall));
}

TEST(MeadePipelineTest, BytesOutsideAnyCommandAreIgnored)
{
    // A host that drips newlines between commands must not shift the framing.
    Pipeline pipeline;

    EXPECT_EQ("V1.2.3#", pipeline.exchange("\r\n:GVN#\r\n"));
}

TEST(MeadePipelineTest, AnOverLongCommandIsDiscardedUpToItsTerminator)
{
    // The legacy loop restarted its index at the overflow and then executed whatever tail
    // arrived before the `#`, so a truncated `:GVN#` could run as a shorter, valid command.
    // The assembler drops the whole thing instead.
    Pipeline pipeline;

    std::string overLong = ":";
    for (int i = 0; i < 40; i++)
    {
        overLong += 'N';
    }
    overLong += "#";

    EXPECT_EQ("", pipeline.exchange(overLong.c_str()));
    EXPECT_TRUE(pipeline.transport().takeOverflow());

    // The next command is framed normally, not stitched onto the discarded one.
    EXPECT_EQ("V1.2.3#", pipeline.exchange(":GVN#"));
}

TEST(MeadePipelineTest, TheHandshakeIsAnsweredByTheTransportNotTheParser)
{
    // 0x06 is a link probe: the transport answers `P` and the parser never sees it.
    Pipeline pipeline;

    EXPECT_EQ("P", pipeline.exchange("\x06"));
    EXPECT_EQ(nullptr, pipeline.handlers().lastCall);
}

TEST(MeadePipelineTest, ADeferredHandlerWritesNothingInlineAndAnswersOnTheEvent)
{
    fakes::FakeSerialPort port;
    Transport transport(port, false);
    meade::PendingReplies<> pending;
    DeferredHandlers handlers(pending, transport, meade::ReplyKind::SuccessFlag, ::core::MountEventType::Parked, 60000);

    port.feed(":hP#");
    ASSERT_TRUE(transport.hasCommand());

    char buffer[Transport::kCommandCapacity] = {0};
    ASSERT_LT(0u, transport.readCommand(buffer, sizeof(buffer)));

    meade::MeadeResponse response;
    meade::dispatchMeadeCommand(response, buffer, handlers);
    if (!handlers.deferred && !response.deferred())
    {
        transport.writeResponse(response.c_str());
    }

    // The park is in flight: the host has been told nothing, and must not be told "1#" either.
    ASSERT_TRUE(handlers.deferred);
    ASSERT_TRUE(handlers.correlation() != 0);
    EXPECT_EQ("", port.output());
    EXPECT_EQ(1, static_cast<int>(pending.size()));

    // The mount finishes parking; the reply goes out on the transport that asked.
    meade::PendingReply reply;
    ASSERT_TRUE(pending.takeCompleted(::core::MountEventType::Parked, reply));
    reply.out->writeResponse("1");

    EXPECT_EQ("1", port.output());
    EXPECT_TRUE(pending.empty());
}

TEST(MeadePipelineTest, ADeferredHandlerAnswersFailureWhenTheDeadlinePasses)
{
    fakes::FakeSerialPort port;
    Transport transport(port, false);
    meade::PendingReplies<> pending;
    DeferredHandlers handlers(pending, transport, meade::ReplyKind::SuccessFlag, ::core::MountEventType::Parked, 2000);

    port.feed(":hP#");
    ASSERT_TRUE(transport.hasCommand());
    char buffer[Transport::kCommandCapacity] = {0};
    ASSERT_LT(0u, transport.readCommand(buffer, sizeof(buffer)));
    meade::MeadeResponse response;
    meade::dispatchMeadeCommand(response, buffer, handlers);
    ASSERT_TRUE(handlers.deferred);  // park answers nothing inline anyway; the flag is the contract
    EXPECT_EQ(1, static_cast<int>(pending.size()));

    meade::PendingReply reply;
    EXPECT_FALSE(pending.takeExpired(1999, reply));
    EXPECT_TRUE(pending.takeExpired(2000, reply));
    reply.out->writeResponse("0");

    EXPECT_EQ("0", port.output());
}

TEST(MeadePipelineTest, ADeferredReplyForOnePeerNeverReachesAnother)
{
    // Two peers, one deferral each, on the same table. The answers have to land on the
    // transport that asked, which is what makes the serial and Wi-Fi paths independent.
    fakes::FakeSerialPort serialPort;
    fakes::FakeSerialPort wifiPort;
    Transport serialTransport(serialPort, false);
    Transport wifiTransport(wifiPort, false);
    meade::PendingReplies<> pending;

    const uint16_t serialCorrelation
        = pending.add(&serialTransport, meade::ReplyKind::SuccessFlag, ::core::MountEventType::SlewArrived, 5000);
    const uint16_t wifiCorrelation = pending.add(&wifiTransport, meade::ReplyKind::SuccessFlag, ::core::MountEventType::SlewArrived, 5000);
    ASSERT_NE(serialCorrelation, wifiCorrelation);

    meade::PendingReply reply;
    ASSERT_TRUE(pending.take(serialCorrelation, reply));
    reply.out->writeResponse("1");

    EXPECT_EQ("1", serialPort.output());
    EXPECT_EQ("", wifiPort.output());
    EXPECT_TRUE(pending.contains(wifiCorrelation));
}

TEST(MeadePipelineTest, APeerThatDisconnectsOwesNothingAfterwards)
{
    fakes::FakeSerialPort serialPort;
    fakes::FakeSerialPort wifiPort;
    Transport serialTransport(serialPort, false);
    Transport wifiTransport(wifiPort, false);
    meade::PendingReplies<> pending;

    pending.add(&serialTransport, meade::ReplyKind::SuccessFlag, ::core::MountEventType::Parked, 5000);
    const uint16_t wifi = pending.add(&wifiTransport, meade::ReplyKind::SuccessFlag, ::core::MountEventType::Parked, 5000);

    pending.clearFor(&wifiTransport);

    EXPECT_EQ(1, static_cast<int>(pending.size()));
    EXPECT_FALSE(pending.contains(wifi));
}
