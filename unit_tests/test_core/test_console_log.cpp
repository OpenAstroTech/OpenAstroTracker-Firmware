#include <gtest/gtest.h>

#include <cstdio>
#include <string>

#include "core/ConsoleLog.hpp"

using core::ConsoleLog;

TEST(ConsoleLogTest, StartsEmpty)
{
    ConsoleLog log;

    EXPECT_EQ(0, log.firstVisible());
    EXPECT_EQ(0, log.visibleCount());
    EXPECT_STREQ("", log.line(0));
}

TEST(ConsoleLogTest, AppendsLinesInOrderAndUppercasesThem)
{
    ConsoleLog log;

    EXPECT_EQ(0, log.add("Booting"));
    EXPECT_EQ(1, log.add("Init GPS..."));

    EXPECT_STREQ("BOOTING", log.line(0));
    EXPECT_STREQ("INIT GPS...", log.line(1));
}

TEST(ConsoleLogTest, TruncatesAnOverlongLine)
{
    ConsoleLog log;

    log.add("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");

    EXPECT_EQ(ConsoleLog::kLineLength, static_cast<int>(std::string(log.line(0)).size()));
}

TEST(ConsoleLogTest, TheNewestLinesAreTheVisibleWindow)
{
    ConsoleLog log;
    for (int i = 0; i < 9; i++)
    {
        log.add("line");
    }

    EXPECT_EQ(9 - ConsoleLog::kVisible, log.firstVisible());
    EXPECT_EQ(ConsoleLog::kVisible, log.visibleCount());
}

TEST(ConsoleLogTest, AFewLinesShowFromTheStart)
{
    ConsoleLog log;
    log.add("one");
    log.add("two");

    EXPECT_EQ(0, log.firstVisible());
    EXPECT_EQ(2, log.visibleCount());
}

TEST(ConsoleLogTest, AFullLogKeepsTheNewestSixteenLines)
{
    ConsoleLog log;
    char text[16];
    for (int i = 0; i < 20; i++)
    {
        snprintf(text, sizeof(text), "line %d", i);
        log.add(text);
    }

    // Twenty lines in, sixteen kept: the oldest four are gone and the last one is at the end.
    EXPECT_STREQ("LINE 19", log.line(ConsoleLog::kCapacity - 1));
    EXPECT_STREQ("LINE 4", log.line(0));
}

TEST(ConsoleLogTest, UpdateReplacesALineInPlace)
{
    ConsoleLog log;
    const int line = log.add("Init GPS...");

    log.update(line, "Init GPS... OK");

    EXPECT_STREQ("INIT GPS... OK", log.line(line));
}

TEST(ConsoleLogTest, UpdateIgnoresAnIndexPastTheEnd)
{
    ConsoleLog log;
    log.add("one");

    log.update(5, "ignored");
    log.update(-1, "ignored");

    EXPECT_STREQ("ONE", log.line(0));
    EXPECT_STREQ("", log.line(5));
}
