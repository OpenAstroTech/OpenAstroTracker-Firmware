// Wire-byte tests for the Meade Quit-family dispatcher (`handleMeadeQuit`).
//
// Every Quit sub-command emits an empty wire response; the test value lives
// in which handler callback fires, captured by `FakeHandlers`. Unknown
// sub-commands must NOT invoke any callback.

#include <gtest/gtest.h>

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

TEST(MeadeQuit, empty_suffix_stops_all)
{
    FakeHandlers h;
    EXPECT_STREQ("", dispatch("", h));
    EXPECT_STREQ("all", h.lastCall);
}

TEST(MeadeQuit, a_suffix_stops_directional_all)
{
    FakeHandlers h;
    EXPECT_STREQ("", dispatch("a", h));
    EXPECT_STREQ("directional", h.lastCall);
}

TEST(MeadeQuit, e_suffix_stops_east)
{
    FakeHandlers h;
    EXPECT_STREQ("", dispatch("e", h));
    EXPECT_STREQ("east", h.lastCall);
}

TEST(MeadeQuit, w_suffix_stops_west)
{
    FakeHandlers h;
    EXPECT_STREQ("", dispatch("w", h));
    EXPECT_STREQ("west", h.lastCall);
}

TEST(MeadeQuit, n_suffix_stops_north)
{
    FakeHandlers h;
    EXPECT_STREQ("", dispatch("n", h));
    EXPECT_STREQ("north", h.lastCall);
}

TEST(MeadeQuit, s_suffix_stops_south)
{
    FakeHandlers h;
    EXPECT_STREQ("", dispatch("s", h));
    EXPECT_STREQ("south", h.lastCall);
}

TEST(MeadeQuit, q_suffix_quits_control_mode)
{
    FakeHandlers h;
    EXPECT_STREQ("", dispatch("q", h));
    EXPECT_STREQ("quitControl", h.lastCall);
}

TEST(MeadeQuit, unknown_suffix_does_not_call_handler)
{
    FakeHandlers h;
    EXPECT_STREQ("", dispatch("z", h));
    EXPECT_EQ(nullptr, h.lastCall);
}

TEST(MeadeQuit, multi_char_suffix_does_not_call_handler)
{
    FakeHandlers h;
    // Single-char commands only; "ea" is not a valid stop-east.
    EXPECT_STREQ("", dispatch("ea", h));
    EXPECT_EQ(nullptr, h.lastCall);
}
