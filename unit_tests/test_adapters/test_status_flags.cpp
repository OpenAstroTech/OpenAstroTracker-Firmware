#include <gtest/gtest.h>

#include "adapters/StatusFlags.hpp"
#include "core/MountState.hpp"

namespace
{
constexpr uint16_t kTracking = 0x0008;
constexpr uint16_t kSlewing  = 0x0002;
}  // namespace

TEST(StatusFlagsTest, SetAndClearReachTheBitfield)
{
    core::MountState state;
    adapters::StatusFlags flags(state);

    flags.set(kTracking);
    EXPECT_TRUE(state.test(kTracking));

    flags.clear(kTracking);
    EXPECT_FALSE(state.test(kTracking));
}

TEST(StatusFlagsTest, ClearingOneBitLeavesTheOthersAlone)
{
    core::MountState state;
    adapters::StatusFlags flags(state);

    flags.set(kTracking | kSlewing);
    flags.clear(kSlewing);

    EXPECT_TRUE(state.test(kTracking));
    EXPECT_FALSE(state.test(kSlewing));
}

TEST(StatusFlagsTest, WritingDoesNotDisturbUnrelatedBits)
{
    core::MountState state;
    state.set(0x1000);  // Parked position, set by someone else
    adapters::StatusFlags flags(state);

    flags.set(kTracking);

    EXPECT_TRUE(state.test(0x1000));
    EXPECT_TRUE(state.test(kTracking));
}
