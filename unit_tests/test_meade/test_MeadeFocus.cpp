// Wire-byte tests for the Meade Focus dispatcher (`handleMeadeFocus`).
//
// Covers all `:F...` sub-commands: continuous motion, MoveBy, speed-by-rate
// (digits 1-4 + F/S aliases), GetPosition, SetPosition (gated by
// onFocusIsAvailable), GetState, and Stop.

#include <unity.h>

#include "core/MeadeParser.hpp"

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
    last = meade::handleMeadeFocus(suffix, h);
    return last.c_str();
}

}  // namespace

namespace
{

void test_focus_continuous_in_empty_response()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("+", h));
    TEST_ASSERT_TRUE(h.continuousIn);
}

void test_focus_continuous_out_empty_response()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("-", h));
    TEST_ASSERT_TRUE(h.continuousOut);
}

void test_focus_move_by_parses_signed_long()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("M-123", h));
    TEST_ASSERT_TRUE(h.moveByCalled);
    TEST_ASSERT_EQUAL_INT(-123, h.moveBySteps);
}

void test_focus_speed_by_rate_digit_1()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("1", h));
    TEST_ASSERT_TRUE(h.setSpeedCalled);
    TEST_ASSERT_EQUAL_INT(1, h.setSpeedRate);
}

void test_focus_speed_by_rate_digit_4()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("4", h));
    TEST_ASSERT_TRUE(h.setSpeedCalled);
    TEST_ASSERT_EQUAL_INT(4, h.setSpeedRate);
}

void test_focus_set_fastest_rate_F_maps_to_4()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("F", h));
    TEST_ASSERT_TRUE(h.setSpeedCalled);
    TEST_ASSERT_EQUAL_INT(4, h.setSpeedRate);
}

void test_focus_set_slowest_rate_S_maps_to_1()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("S", h));
    TEST_ASSERT_TRUE(h.setSpeedCalled);
    TEST_ASSERT_EQUAL_INT(1, h.setSpeedRate);
}

void test_focus_get_position_emits_long_with_terminator()
{
    FakeHandlers h;
    h.position = 12345;
    TEST_ASSERT_EQUAL_STRING("12345#", dispatch("p", h));
}

void test_focus_get_position_zero_emits_zero_terminator()
{
    FakeHandlers h;
    h.position = 0;
    TEST_ASSERT_EQUAL_STRING("0#", dispatch("p", h));
}

void test_focus_set_position_available_emits_one_no_terminator()
{
    FakeHandlers h;
    h.available = true;
    TEST_ASSERT_EQUAL_STRING("1", dispatch("P777", h));
    TEST_ASSERT_TRUE(h.setPosCalled);
    TEST_ASSERT_EQUAL_INT(777, h.setPosSteps);
}

void test_focus_set_position_unavailable_emits_empty_no_call()
{
    FakeHandlers h;
    h.available = false;
    TEST_ASSERT_EQUAL_STRING("", dispatch("P777", h));
    TEST_ASSERT_FALSE(h.setPosCalled);
}

void test_focus_get_state_true_emits_one()
{
    FakeHandlers h;
    h.runningState = true;
    TEST_ASSERT_EQUAL_STRING("1", dispatch("B", h));
}

void test_focus_get_state_false_emits_zero()
{
    FakeHandlers h;
    h.runningState = false;
    TEST_ASSERT_EQUAL_STRING("0", dispatch("B", h));
}

void test_focus_stop_emits_empty()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("Q", h));
    TEST_ASSERT_TRUE(h.stopCalled);
}

void test_focus_empty_suffix_emits_empty_no_call()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("", h));
    TEST_ASSERT_FALSE(h.continuousIn);
    TEST_ASSERT_FALSE(h.continuousOut);
    TEST_ASSERT_FALSE(h.moveByCalled);
    TEST_ASSERT_FALSE(h.setSpeedCalled);
    TEST_ASSERT_FALSE(h.stopCalled);
}

void test_focus_unknown_suffix_emits_empty_no_call()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("Z", h));
    TEST_ASSERT_FALSE(h.setSpeedCalled);
}

}  // namespace

void register_meade_focus_tests()
{
    RUN_TEST(test_focus_continuous_in_empty_response);
    RUN_TEST(test_focus_continuous_out_empty_response);
    RUN_TEST(test_focus_move_by_parses_signed_long);
    RUN_TEST(test_focus_speed_by_rate_digit_1);
    RUN_TEST(test_focus_speed_by_rate_digit_4);
    RUN_TEST(test_focus_set_fastest_rate_F_maps_to_4);
    RUN_TEST(test_focus_set_slowest_rate_S_maps_to_1);
    RUN_TEST(test_focus_get_position_emits_long_with_terminator);
    RUN_TEST(test_focus_get_position_zero_emits_zero_terminator);
    RUN_TEST(test_focus_set_position_available_emits_one_no_terminator);
    RUN_TEST(test_focus_set_position_unavailable_emits_empty_no_call);
    RUN_TEST(test_focus_get_state_true_emits_one);
    RUN_TEST(test_focus_get_state_false_emits_zero);
    RUN_TEST(test_focus_stop_emits_empty);
    RUN_TEST(test_focus_empty_suffix_emits_empty_no_call);
    RUN_TEST(test_focus_unknown_suffix_emits_empty_no_call);
}
