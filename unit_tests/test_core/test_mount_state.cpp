#include <gtest/gtest.h>

#include "core/MountState.hpp"

using core::MountState;
using core::StatusBit;

namespace
{
uint16_t mask(StatusBit bit)
{
    return static_cast<uint16_t>(bit);
}
}  // namespace

// The numbering is a wire format: the Meade status string and the displays decode it,
// so a reshuffle here would silently change what the mount reports.
TEST(MountStateTest, BitValuesMatchTheHistoricalLayout)
{
    EXPECT_EQ(0x0000u, mask(StatusBit::Parked));
    EXPECT_EQ(0x0002u, mask(StatusBit::Slewing));
    EXPECT_EQ(0x0004u, mask(StatusBit::SlewingToTarget));
    EXPECT_EQ(0x0002u, mask(StatusBit::SlewingFree));  // deliberately the same bit
    EXPECT_EQ(0x0100u, mask(StatusBit::SlewingManual));
    EXPECT_EQ(0x0008u, mask(StatusBit::Tracking));
    EXPECT_EQ(0x0010u, mask(StatusBit::Parking));
    EXPECT_EQ(0x1000u, mask(StatusBit::ParkingPos));
    EXPECT_EQ(0x0080u, mask(StatusBit::GuidePulse));
    EXPECT_EQ(0x0060u, mask(StatusBit::GuidePulseDir));
    EXPECT_EQ(0x0040u, mask(StatusBit::GuidePulseRa));
    EXPECT_EQ(0x0020u, mask(StatusBit::GuidePulseDec));
    EXPECT_EQ(0x00E0u, mask(StatusBit::GuidePulseMask));
    EXPECT_EQ(0x2000u, mask(StatusBit::FindingHome));
}

TEST(MountStateTest, StartsParked)
{
    MountState state;

    EXPECT_TRUE(state.isParked());
    EXPECT_EQ(0u, state.value());
    EXPECT_FALSE(state.test(mask(StatusBit::Tracking)));
}

TEST(MountStateTest, SetAndClearSingleBits)
{
    MountState state;

    state.set(mask(StatusBit::Tracking));
    EXPECT_TRUE(state.test(mask(StatusBit::Tracking)));
    EXPECT_FALSE(state.isParked());

    state.clear(mask(StatusBit::Tracking));
    EXPECT_FALSE(state.test(mask(StatusBit::Tracking)));
    EXPECT_TRUE(state.isParked());
}

TEST(MountStateTest, SettingIsAdditiveAndClearingIsSelective)
{
    MountState state;

    state.set(mask(StatusBit::Slewing) | mask(StatusBit::SlewingToTarget) | mask(StatusBit::Tracking));
    state.clear(mask(StatusBit::Slewing));
    state.clear(mask(StatusBit::SlewingToTarget));

    EXPECT_FALSE(state.test(mask(StatusBit::Slewing)));
    EXPECT_FALSE(state.test(mask(StatusBit::SlewingToTarget)));
    EXPECT_TRUE(state.test(mask(StatusBit::Tracking)));
}

TEST(MountStateTest, ResetReplacesTheWholeField)
{
    MountState state;
    state.set(mask(StatusBit::Tracking) | mask(StatusBit::Parking));

    state.reset(mask(StatusBit::ParkingPos));

    EXPECT_TRUE(state.test(mask(StatusBit::ParkingPos)));
    EXPECT_FALSE(state.test(mask(StatusBit::Tracking)));
    EXPECT_FALSE(state.test(mask(StatusBit::Parking)));
}

TEST(MountStateTest, MasksSpanTheirMemberBits)
{
    MountState state;

    state.set(mask(StatusBit::GuidePulseRa));
    // `test()` asks "is any of these set", so the direction mask now matches.
    EXPECT_TRUE(state.test(mask(StatusBit::GuidePulseDir)));
    EXPECT_TRUE(state.test(mask(StatusBit::GuidePulseMask)));
    EXPECT_FALSE(state.test(mask(StatusBit::GuidePulseDec)));
    // ...but the "guiding at all" bit is separate from the direction bits.
    EXPECT_FALSE(state.test(mask(StatusBit::GuidePulse)));

    state.set(mask(StatusBit::GuidePulse));
    EXPECT_TRUE(state.test(mask(StatusBit::GuidePulseMask)));
}

TEST(MountStateTest, TestIsFalseWhenOnlyOtherBitsAreSet)
{
    MountState state;
    state.set(mask(StatusBit::Parking) | mask(StatusBit::FindingHome));

    EXPECT_FALSE(state.test(mask(StatusBit::Tracking)));
    EXPECT_TRUE(state.test(mask(StatusBit::Parking) | mask(StatusBit::Tracking)));
}

TEST(MountStateTest, ClearingABitThatIsNotSetChangesNothing)
{
    MountState state;
    state.set(mask(StatusBit::Tracking));

    state.clear(mask(StatusBit::FindingHome));

    EXPECT_EQ(mask(StatusBit::Tracking), state.value());
}

TEST(MountStateTest, SettingParkedIsNotPossibleBecauseParkedIsZero)
{
    MountState state;
    state.set(mask(StatusBit::Tracking));

    state.set(mask(StatusBit::Parked));

    EXPECT_FALSE(state.isParked());  // "parked" is the absence of every other bit
    EXPECT_TRUE(state.test(mask(StatusBit::Tracking)));
}
