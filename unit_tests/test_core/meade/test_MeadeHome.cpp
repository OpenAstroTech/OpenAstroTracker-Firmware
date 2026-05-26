// Wire-byte tests for the Meade Home dispatcher (`handleMeadeHome`).
//
// `:hP#`/`:hF#` perform park/slew-to-home and emit an empty response.
// `:hU#`/`:hZ#` unpark / set the Az/Alt home and emit `"1"`. Any other
// suffix is ignored and produces an empty response.

#include <gtest/gtest.h>

#include "core/meade/MeadeParser.hpp"

namespace meade = oat::core::meade;

namespace
{

class FakeHandlers : public meade::IMeadeHomeHandlers
{
  public:
    const char *lastCall = "";

    void onPark() override
    {
        lastCall = "park";
    }
    void onSlewToHome() override
    {
        lastCall = "home";
    }
    void onUnpark() override
    {
        lastCall = "unpark";
    }
    void onSetAzAltHome() override
    {
        lastCall = "azalt";
    }
};

const char *dispatch(const char *suffix, FakeHandlers &h)
{
    static meade::MeadeResponse last;
    last.clear();
    meade::handleMeadeHome(last, suffix, h);
    return last.c_str();
}

}  // namespace

TEST(MeadeHome, p_parks_and_emits_empty)
{
    FakeHandlers h;
    EXPECT_STREQ("", dispatch("P", h));
    EXPECT_STREQ("park", h.lastCall);
}

TEST(MeadeHome, f_slews_home_and_emits_empty)
{
    FakeHandlers h;
    EXPECT_STREQ("", dispatch("F", h));
    EXPECT_STREQ("home", h.lastCall);
}

TEST(MeadeHome, u_unparks_and_emits_one)
{
    FakeHandlers h;
    EXPECT_STREQ("1", dispatch("U", h));
    EXPECT_STREQ("unpark", h.lastCall);
}

TEST(MeadeHome, z_sets_azalt_and_emits_one)
{
    FakeHandlers h;
    EXPECT_STREQ("1", dispatch("Z", h));
    EXPECT_STREQ("azalt", h.lastCall);
}

TEST(MeadeHome, unknown_suffix_emits_empty)
{
    FakeHandlers h;
    EXPECT_STREQ("", dispatch("Q", h));
    EXPECT_STREQ("", h.lastCall);
}

TEST(MeadeHome, empty_suffix_emits_empty)
{
    FakeHandlers h;
    EXPECT_STREQ("", dispatch("", h));
    EXPECT_STREQ("", h.lastCall);
}

TEST(MeadeHome, trailing_bytes_do_not_call_handler)
{
    FakeHandlers h;
    EXPECT_STREQ("", dispatch("Px", h));
    EXPECT_STREQ("", h.lastCall);
}
