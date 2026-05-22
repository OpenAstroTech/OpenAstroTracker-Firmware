// Wire-byte tests for the Meade SetSlewRate dispatcher (`handleMeadeSetSlewRate`).
//
// `:RS#`/`:RM#`/`:RC#`/`:RG#` map to mount slew rates 4/3/2/1 with empty wire
// responses. Unknown or malformed suffixes are ignored and emit empty.

#include <gtest/gtest.h>

#include "core/meade/MeadeParser.hpp"

namespace meade = oat::core::meade;

namespace
{

class FakeHandlers : public meade::IMeadeSlewRateHandlers
{
  public:
    int lastRate  = -1;
    int callCount = 0;

    void onSetSlewRate(uint8_t rate) override
    {
        lastRate = static_cast<int>(rate);
        ++callCount;
    }
};

const char *dispatch(const char *suffix, FakeHandlers &h)
{
    static meade::MeadeResponse last;
    last.clear();
    meade::handleMeadeSetSlewRate(last, suffix, h);
    return last.c_str();
}

}  // namespace

TEST(MeadeSlewRate, s_sets_4)
{
    FakeHandlers h;
    EXPECT_STREQ("", dispatch("S", h));
    EXPECT_EQ(4, h.lastRate);
}

TEST(MeadeSlewRate, m_sets_3)
{
    FakeHandlers h;
    EXPECT_STREQ("", dispatch("M", h));
    EXPECT_EQ(3, h.lastRate);
}

TEST(MeadeSlewRate, c_sets_2)
{
    FakeHandlers h;
    EXPECT_STREQ("", dispatch("C", h));
    EXPECT_EQ(2, h.lastRate);
}

TEST(MeadeSlewRate, g_sets_1)
{
    FakeHandlers h;
    EXPECT_STREQ("", dispatch("G", h));
    EXPECT_EQ(1, h.lastRate);
}

TEST(MeadeSlewRate, unknown_suffix_no_call)
{
    FakeHandlers h;
    EXPECT_STREQ("", dispatch("Q", h));
    EXPECT_EQ(0, h.callCount);
}

TEST(MeadeSlewRate, multi_char_suffix_no_call)
{
    FakeHandlers h;
    EXPECT_STREQ("", dispatch("SS", h));
    EXPECT_EQ(0, h.callCount);
}

TEST(MeadeSlewRate, empty_suffix_no_call)
{
    FakeHandlers h;
    EXPECT_STREQ("", dispatch("", h));
    EXPECT_EQ(0, h.callCount);
}
