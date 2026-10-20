#include <gtest/gtest.h>

#include "../test_common/hal_fakes/FakeSystemClock.hpp"
#include "adapters/ArduinoClock.hpp"

TEST(ArduinoClockTest, NowFollowsTheSystemClock)
{
    fakes::FakeSystemClock systemClock;
    adapters::ArduinoClock clock(systemClock);

    systemClock.set(1234);

    EXPECT_EQ(1234u, clock.now());
}

TEST(ArduinoClockTest, DelayBlocksTheUnderlyingClock)
{
    fakes::FakeSystemClock systemClock;
    adapters::ArduinoClock clock(systemClock);

    clock.delay(250);

    EXPECT_EQ(250u, clock.now());
    EXPECT_EQ(1u, systemClock.delaysRequested());
}

TEST(ArduinoClockTest, NowWrapsAsAnUnsignedCounter)
{
    fakes::FakeSystemClock systemClock;
    adapters::ArduinoClock clock(systemClock);
    systemClock.set(0xFFFFFF00u);

    systemClock.advance(0x200u);

    // Deadlines must be compared with unsigned subtraction for exactly this reason:
    // the counter wrapped, but the elapsed time is still the real 0x200 ms.
    EXPECT_EQ(0x100u, clock.now());
    EXPECT_EQ(0x200u, clock.now() - 0xFFFFFF00u);
}
