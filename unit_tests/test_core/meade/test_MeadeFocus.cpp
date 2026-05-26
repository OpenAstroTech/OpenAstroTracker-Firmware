// Wire-byte tests for the Meade Focus dispatcher (`handleMeadeFocus`).
//
// Covers all `:F...` sub-commands: continuous motion, MoveBy, speed-by-rate
// (digits 1-4 + F/S aliases), GetPosition, SetPosition (gated by
// onFocusIsAvailable), GetState, and Stop.

#include <gtest/gtest.h>

#include "core/meade/MeadeParser.hpp"

namespace meade = oat::core::meade;

namespace
{

class FakeHandlers : public meade::IMeadeFocusHandlers
{
  public:
    bool continuousIn   = false;
    bool continuousOut  = false;
    bool moveByCalled   = false;
    long moveBySteps    = 0;
    bool setSpeedCalled = false;
    int setSpeedRate    = 0;
    bool stopCalled     = false;
    bool setPosCalled   = false;
    long setPosSteps    = 0;
    long position       = 0;
    bool available      = true;
    bool runningState   = false;

    void onFocusContinuousIn() override
    {
        continuousIn = true;
    }
    void onFocusContinuousOut() override
    {
        continuousOut = true;
    }
    void onFocusMoveBy(long steps) override
    {
        moveByCalled = true;
        moveBySteps  = steps;
    }
    void onFocusSetSpeedByRate(int rate) override
    {
        setSpeedCalled = true;
        setSpeedRate   = rate;
    }
    void onFocusStop() override
    {
        stopCalled = true;
    }
    long onFocusGetPosition() override
    {
        return position;
    }
    bool onFocusIsAvailable() override
    {
        return available;
    }
    void onFocusSetPosition(long steps) override
    {
        setPosCalled = true;
        setPosSteps  = steps;
    }
    bool onFocusGetState() override
    {
        return runningState;
    }
};

const char *dispatch(const char *suffix, FakeHandlers &h)
{
    static meade::MeadeResponse last;
    last.clear();
    meade::handleMeadeFocus(last, suffix, h);
    return last.c_str();
}

}  // namespace

TEST(MeadeFocus, continuous_in_empty_response)
{
    FakeHandlers h;
    EXPECT_STREQ("", dispatch("+", h));
    EXPECT_TRUE(h.continuousIn);
}

TEST(MeadeFocus, continuous_out_empty_response)
{
    FakeHandlers h;
    EXPECT_STREQ("", dispatch("-", h));
    EXPECT_TRUE(h.continuousOut);
}

TEST(MeadeFocus, move_by_parses_signed_long)
{
    FakeHandlers h;
    EXPECT_STREQ("", dispatch("M-123", h));
    EXPECT_TRUE(h.moveByCalled);
    EXPECT_EQ(-123, h.moveBySteps);
}

TEST(MeadeFocus, speed_by_rate_digit_1)
{
    FakeHandlers h;
    EXPECT_STREQ("", dispatch("1", h));
    EXPECT_TRUE(h.setSpeedCalled);
    EXPECT_EQ(1, h.setSpeedRate);
}

TEST(MeadeFocus, speed_by_rate_digit_4)
{
    FakeHandlers h;
    EXPECT_STREQ("", dispatch("4", h));
    EXPECT_TRUE(h.setSpeedCalled);
    EXPECT_EQ(4, h.setSpeedRate);
}

TEST(MeadeFocus, set_fastest_rate_F_maps_to_4)
{
    FakeHandlers h;
    EXPECT_STREQ("", dispatch("F", h));
    EXPECT_TRUE(h.setSpeedCalled);
    EXPECT_EQ(4, h.setSpeedRate);
}

TEST(MeadeFocus, set_slowest_rate_S_maps_to_1)
{
    FakeHandlers h;
    EXPECT_STREQ("", dispatch("S", h));
    EXPECT_TRUE(h.setSpeedCalled);
    EXPECT_EQ(1, h.setSpeedRate);
}

TEST(MeadeFocus, get_position_emits_long_with_terminator)
{
    FakeHandlers h;
    h.position = 12345;
    EXPECT_STREQ("12345#", dispatch("p", h));
}

TEST(MeadeFocus, get_position_zero_emits_zero_terminator)
{
    FakeHandlers h;
    h.position = 0;
    EXPECT_STREQ("0#", dispatch("p", h));
}

TEST(MeadeFocus, set_position_available_emits_one_no_terminator)
{
    FakeHandlers h;
    h.available = true;
    EXPECT_STREQ("1", dispatch("P777", h));
    EXPECT_TRUE(h.setPosCalled);
    EXPECT_EQ(777, h.setPosSteps);
}

TEST(MeadeFocus, set_position_unavailable_emits_empty_no_call)
{
    FakeHandlers h;
    h.available = false;
    EXPECT_STREQ("", dispatch("P777", h));
    EXPECT_FALSE(h.setPosCalled);
}

TEST(MeadeFocus, get_state_true_emits_one)
{
    FakeHandlers h;
    h.runningState = true;
    EXPECT_STREQ("1", dispatch("B", h));
}

TEST(MeadeFocus, get_state_false_emits_zero)
{
    FakeHandlers h;
    h.runningState = false;
    EXPECT_STREQ("0", dispatch("B", h));
}

TEST(MeadeFocus, stop_emits_empty)
{
    FakeHandlers h;
    EXPECT_STREQ("", dispatch("Q", h));
    EXPECT_TRUE(h.stopCalled);
}

TEST(MeadeFocus, empty_suffix_emits_empty_no_call)
{
    FakeHandlers h;
    EXPECT_STREQ("", dispatch("", h));
    EXPECT_FALSE(h.continuousIn);
    EXPECT_FALSE(h.continuousOut);
    EXPECT_FALSE(h.moveByCalled);
    EXPECT_FALSE(h.setSpeedCalled);
    EXPECT_FALSE(h.stopCalled);
}

TEST(MeadeFocus, unknown_suffix_emits_empty_no_call)
{
    FakeHandlers h;
    EXPECT_STREQ("", dispatch("Z", h));
    EXPECT_FALSE(h.setSpeedCalled);
}
