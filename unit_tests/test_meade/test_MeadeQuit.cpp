// Wire-byte tests for the Meade Quit-family dispatcher (`handleMeadeQuit`).
//
// Every Quit sub-command emits an empty wire response; the test value lives
// in which handler callback fires, captured by `FakeHandlers`. Unknown
// sub-commands must NOT invoke any callback.

#include <unity.h>

#include <string.h>

#include "core/meade/MeadeParser.hpp"

namespace meade = oat::core::meade;

namespace
{

class FakeHandlers : public meade::IMeadeQuitHandlers
{
  public:
    const char *lastCall = nullptr;

    void onStopAll() override
    {
        lastCall = "all";
    }
    void onStopDirectionalAll() override
    {
        lastCall = "directional";
    }
    void onStopEast() override
    {
        lastCall = "east";
    }
    void onStopWest() override
    {
        lastCall = "west";
    }
    void onStopNorth() override
    {
        lastCall = "north";
    }
    void onStopSouth() override
    {
        lastCall = "south";
    }
    void onQuitControlMode() override
    {
        lastCall = "quitControl";
    }
};

const char *dispatch(const char *suffix, FakeHandlers &h)
{
    static meade::MeadeResponse last;
    last.clear();
    meade::handleMeadeQuit(last, suffix, h);
    return last.c_str();
}

}  // namespace

namespace
{

void test_empty_suffix_stops_all()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("", h));
    TEST_ASSERT_EQUAL_STRING("all", h.lastCall);
}

void test_a_suffix_stops_directional_all()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("a", h));
    TEST_ASSERT_EQUAL_STRING("directional", h.lastCall);
}

void test_e_suffix_stops_east()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("e", h));
    TEST_ASSERT_EQUAL_STRING("east", h.lastCall);
}

void test_w_suffix_stops_west()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("w", h));
    TEST_ASSERT_EQUAL_STRING("west", h.lastCall);
}

void test_n_suffix_stops_north()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("n", h));
    TEST_ASSERT_EQUAL_STRING("north", h.lastCall);
}

void test_s_suffix_stops_south()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("s", h));
    TEST_ASSERT_EQUAL_STRING("south", h.lastCall);
}

void test_q_suffix_quits_control_mode()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("q", h));
    TEST_ASSERT_EQUAL_STRING("quitControl", h.lastCall);
}

void test_unknown_suffix_does_not_call_handler()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("z", h));
    TEST_ASSERT_NULL(h.lastCall);
}

void test_multi_char_suffix_does_not_call_handler()
{
    FakeHandlers h;
    // Single-char commands only; "ea" is not a valid stop-east.
    TEST_ASSERT_EQUAL_STRING("", dispatch("ea", h));
    TEST_ASSERT_NULL(h.lastCall);
}

}  // namespace

void register_meade_quit_tests()
{
    RUN_TEST(test_empty_suffix_stops_all);
    RUN_TEST(test_a_suffix_stops_directional_all);
    RUN_TEST(test_e_suffix_stops_east);
    RUN_TEST(test_w_suffix_stops_west);
    RUN_TEST(test_n_suffix_stops_north);
    RUN_TEST(test_s_suffix_stops_south);
    RUN_TEST(test_q_suffix_quits_control_mode);
    RUN_TEST(test_unknown_suffix_does_not_call_handler);
    RUN_TEST(test_multi_char_suffix_does_not_call_handler);
}
