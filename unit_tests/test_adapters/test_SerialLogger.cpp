#include <gtest/gtest.h>

#include "../test_common/hal_fakes/FakeSerialPort.hpp"
#include "../test_common/hal_fakes/FakeSystemClock.hpp"
#include "adapters/SerialLogger.hpp"

namespace
{
// Mirrors the firmware's DEBUG_* bit layout: category 0x0004 is enabled below.
const uint16_t ENABLED_CATEGORY  = 0x0004;
const uint16_t DISABLED_CATEGORY = 0x0008;
}  // namespace

TEST(SerialLoggerTest, EmitsTimestampedLineForEnabledCategory)
{
    fakes::FakeSerialPort port;
    fakes::FakeSystemClock clock;
    adapters::SerialLogger logger(port, clock, ENABLED_CATEGORY);
    clock.set(1500);

    logger.log(ENABLED_CATEGORY, OAT_LOG_FORMAT("hello %d"), 42);

    EXPECT_EQ("[1500]{1500}ms: hello 42\n", port.output());
    EXPECT_EQ(1u, port.flushCount());
}

TEST(SerialLoggerTest, ReportsDeltaSinceThePreviousLine)
{
    fakes::FakeSerialPort port;
    fakes::FakeSystemClock clock;
    adapters::SerialLogger logger(port, clock, ENABLED_CATEGORY);

    clock.set(100);
    logger.log(ENABLED_CATEGORY, OAT_LOG_FORMAT("first"));
    clock.set(175);
    logger.log(ENABLED_CATEGORY, OAT_LOG_FORMAT("second"));

    EXPECT_EQ("[100]{100}ms: first\n[175]{75}ms: second\n", port.output());
}

TEST(SerialLoggerTest, SilentlyDropsDisabledCategories)
{
    fakes::FakeSerialPort port;
    fakes::FakeSystemClock clock;
    adapters::SerialLogger logger(port, clock, ENABLED_CATEGORY);

    EXPECT_FALSE(logger.isEnabled(DISABLED_CATEGORY));
    EXPECT_TRUE(logger.isEnabled(ENABLED_CATEGORY | DISABLED_CATEGORY));

    logger.log(DISABLED_CATEGORY, OAT_LOG_FORMAT("hidden"));

    EXPECT_EQ("", port.output());
}

TEST(SerialLoggerTest, TruncatesInsteadOfOverrunningTheBuffer)
{
    fakes::FakeSerialPort port;
    fakes::FakeSystemClock clock;
    adapters::SerialLogger logger(port, clock, ENABLED_CATEGORY);

    // 200 characters into a 192-byte line buffer: the line must end cleanly.
    logger.log(ENABLED_CATEGORY,
               OAT_LOG_FORMAT("%s"),
               "0123456789012345678901234567890123456789"
               "0123456789012345678901234567890123456789"
               "0123456789012345678901234567890123456789"
               "0123456789012345678901234567890123456789"
               "0123456789012345678901234567890123456789");

    const std::string output = port.output();
    ASSERT_EQ('\n', output.back());
    EXPECT_LE(output.size(), adapters::SerialLogger::LINE_BUFFER_SIZE);
}
