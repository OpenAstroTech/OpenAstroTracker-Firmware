#include <gtest/gtest.h>

#include <string>

#include "adapters/meade/PendingReplies.hpp"

namespace meade = adapters::meade;
using meade::PendingReplies;
using meade::PendingReply;
using meade::ReplyKind;

namespace
{
class RecordingTransport final : public ports::ITransport
{
  public:
    bool hasCommand() override
    {
        return false;
    }

    size_t readCommand(char *, size_t) override
    {
        return 0;
    }

    void writeResponse(const char *response) override
    {
        writes += response;
    }

    std::string writes;
};

PendingReply entry()
{
    PendingReply e;
    e.correlation = 0;
    e.out         = nullptr;
    e.completesOn = types::MountEventType::SlewArrived;
    e.kind        = ReplyKind::Silent;
    e.deadlineMs  = 0;
    return e;
}
}  // namespace

TEST(PendingRepliesTest, AnEmptyTableOwesNothing)
{
    PendingReplies<> table;

    EXPECT_TRUE(table.empty());
    EXPECT_EQ(0, static_cast<int>(table.size()));
    EXPECT_FALSE(table.contains(1));
}

TEST(PendingRepliesTest, AddReturnsANonZeroCorrelationAndKeepsTheEntry)
{
    PendingReplies<> table;
    RecordingTransport out;

    const uint16_t correlation = table.add(&out, ReplyKind::SuccessFlag, types::MountEventType::SlewArrived, 1000);

    ASSERT_NE(0, correlation);
    EXPECT_TRUE(table.contains(correlation));
    EXPECT_FALSE(table.empty());
    EXPECT_EQ(1, static_cast<int>(table.size()));
}

TEST(PendingRepliesTest, CorrelationsAreDistinctAndNeverZero)
{
    PendingReplies<> table;
    RecordingTransport out;

    const uint16_t first  = table.add(&out, ReplyKind::Silent, types::MountEventType::Parked, 1000);
    const uint16_t second = table.add(&out, ReplyKind::Silent, types::MountEventType::Parked, 1000);

    EXPECT_NE(0, first);
    EXPECT_NE(0, second);
    EXPECT_NE(first, second);
}

TEST(PendingRepliesTest, CorrelationWrapsWithoutBecomingZero)
{
    PendingReplies<1> table;
    RecordingTransport out;

    uint16_t last = 0;
    for (int i = 0; i < 70000; i++)
    {
        const uint16_t correlation = table.add(&out, ReplyKind::Silent, types::MountEventType::Parked, 1000);
        ASSERT_NE(0, correlation);
        last = correlation;

        PendingReply taken;
        ASSERT_TRUE(table.take(correlation, taken));
    }

    // 70000 additions passes 0xFFFF several times; the id is still a usable, non-zero key.
    EXPECT_NE(0, last);
    EXPECT_TRUE(table.empty());
}

TEST(PendingRepliesTest, AFullTableRefusesRatherThanOverwrites)
{
    PendingReplies<2> table;
    RecordingTransport first;
    RecordingTransport second;
    RecordingTransport third;

    const uint16_t a = table.add(&first, ReplyKind::Silent, types::MountEventType::Parked, 1000);
    const uint16_t b = table.add(&second, ReplyKind::Silent, types::MountEventType::Parked, 1000);
    const uint16_t c = table.add(&third, ReplyKind::Silent, types::MountEventType::Parked, 1000);

    EXPECT_NE(0, a);
    EXPECT_NE(0, b);
    EXPECT_EQ(0, c);  // the caller must answer inline instead of leaving the host waiting
    EXPECT_TRUE(table.contains(a));
    EXPECT_TRUE(table.contains(b));
    EXPECT_EQ(2, static_cast<int>(table.size()));
}

TEST(PendingRepliesTest, TakesTheEntryTheEventCompletes)
{
    PendingReplies<> table;
    RecordingTransport slewOut;
    RecordingTransport focusOut;

    const uint16_t slew  = table.add(&slewOut, ReplyKind::SuccessFlag, types::MountEventType::SlewArrived, 1000);
    const uint16_t focus = table.add(&focusOut, ReplyKind::SuccessFlag, types::MountEventType::FocusStopped, 1000);

    PendingReply taken = entry();
    ASSERT_TRUE(table.takeCompleted(types::MountEventType::FocusStopped, taken));
    EXPECT_EQ(focus, taken.correlation);
    EXPECT_EQ(&focusOut, taken.out);
    EXPECT_EQ(ReplyKind::SuccessFlag, taken.kind);

    EXPECT_TRUE(table.contains(slew));
    EXPECT_FALSE(table.contains(focus));
}

TEST(PendingRepliesTest, AnEventNoEntryWaitsForCompletesNothing)
{
    PendingReplies<> table;
    RecordingTransport out;
    table.add(&out, ReplyKind::Silent, types::MountEventType::SlewArrived, 1000);

    PendingReply taken = entry();
    EXPECT_FALSE(table.takeCompleted(types::MountEventType::Parked, taken));
    EXPECT_EQ(1, static_cast<int>(table.size()));
}

TEST(PendingRepliesTest, EntriesWaitingOnTheSameEventCompleteOldestFirst)
{
    PendingReplies<> table;
    RecordingTransport out;

    const uint16_t first  = table.add(&out, ReplyKind::SuccessFlag, types::MountEventType::Parked, 1000);
    const uint16_t second = table.add(&out, ReplyKind::SuccessFlag, types::MountEventType::Parked, 2000);

    PendingReply taken = entry();
    ASSERT_TRUE(table.takeCompleted(types::MountEventType::Parked, taken));
    EXPECT_EQ(first, taken.correlation);

    ASSERT_TRUE(table.takeCompleted(types::MountEventType::Parked, taken));
    EXPECT_EQ(second, taken.correlation);

    EXPECT_TRUE(table.empty());
}

TEST(PendingRepliesTest, ADeadlineExpiresTheEntryAtItsMoment)
{
    PendingReplies<> table;
    RecordingTransport out;
    const uint16_t correlation = table.add(&out, ReplyKind::SuccessFlag, types::MountEventType::SlewArrived, 500);

    PendingReply taken = entry();
    EXPECT_FALSE(table.takeExpired(499, taken));
    EXPECT_TRUE(table.contains(correlation));

    EXPECT_TRUE(table.takeExpired(500, taken));
    EXPECT_EQ(correlation, taken.correlation);
    EXPECT_FALSE(table.contains(correlation));
}

TEST(PendingRepliesTest, ADeadlineSurvivesTheMillisecondCounterWrapping)
{
    // `millis()` wraps every 49.7 days. A deadline taken just before the wrap must still expire
    // just after it, which is why the comparison is a signed difference and not `now >= deadline`.
    PendingReplies<> table;
    RecordingTransport out;
    const uint32_t deadline    = 0xFFFFFFFFu - 10u;
    const uint16_t correlation = table.add(&out, ReplyKind::SuccessFlag, types::MountEventType::SlewArrived, deadline);

    PendingReply taken = entry();
    EXPECT_FALSE(table.takeExpired(deadline - 1u, taken));
    EXPECT_TRUE(table.takeExpired(0u, taken));  // wrapped: 11 ms past the deadline
    EXPECT_EQ(correlation, taken.correlation);
}

TEST(PendingRepliesTest, TheTableHandsBackWhatItStored)
{
    PendingReplies<> table;
    RecordingTransport out;

    const uint16_t correlation = table.add(&out, ReplyKind::SuccessFlag, types::MountEventType::HomingFinished, 1234);

    PendingReply taken = entry();
    ASSERT_TRUE(table.take(correlation, taken));
    EXPECT_EQ(&out, taken.out);
    EXPECT_EQ(types::MountEventType::HomingFinished, taken.completesOn);
    EXPECT_EQ(ReplyKind::SuccessFlag, taken.kind);
    EXPECT_EQ(1234u, taken.deadlineMs);
}

TEST(PendingRepliesTest, ClearForgetsEveryReply)
{
    PendingReplies<> table;
    RecordingTransport out;
    table.add(&out, ReplyKind::Silent, types::MountEventType::Parked, 1000);
    table.add(&out, ReplyKind::Silent, types::MountEventType::Parked, 1000);

    table.clear();

    EXPECT_TRUE(table.empty());
}

TEST(PendingRepliesTest, ClearForDropsOnlyOnePeersReplies)
{
    PendingReplies<> table;
    RecordingTransport serial;
    RecordingTransport wifi;

    const uint16_t keep = table.add(&serial, ReplyKind::Silent, types::MountEventType::Parked, 1000);
    table.add(&wifi, ReplyKind::Silent, types::MountEventType::Parked, 1000);

    table.clearFor(&wifi);

    EXPECT_TRUE(table.contains(keep));
    EXPECT_EQ(1, static_cast<int>(table.size()));
}

TEST(PendingRepliesTest, AReleasedSlotIsReusable)
{
    PendingReplies<1> table;
    RecordingTransport first;
    RecordingTransport second;

    const uint16_t a = table.add(&first, ReplyKind::Silent, types::MountEventType::Parked, 1000);
    PendingReply taken;
    ASSERT_TRUE(table.take(a, taken));

    const uint16_t b = table.add(&second, ReplyKind::Silent, types::MountEventType::Parked, 2000);
    EXPECT_NE(0, b);
    EXPECT_TRUE(table.contains(b));
}
