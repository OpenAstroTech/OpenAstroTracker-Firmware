// Wire-byte tests for the Meade Distance-bars dispatcher (`handleMeadeDistance`).
//
// The `:D#` command reports motion status as a single byte ('|' while
// slewing, ' ' when idle) followed by the standard terminator. Any suffix
// is treated identically (legacy lenient behaviour).

#include <gtest/gtest.h>

#include "core/meade/MeadeParser.hpp"

namespace meade = oat::core::meade;

namespace
{

class FakeHandlers : public meade::IMeadeDistanceHandlers
{
  public:
    bool slewing  = false;
    int callCount = 0;

    bool onIsSlewingRaOrDec() override
    {
        ++callCount;
        return slewing;
    }
};

const char *dispatch(const char *suffix, FakeHandlers &h)
{
    static meade::MeadeResponse last;
    last.clear();
    meade::handleMeadeDistance(last, suffix, h);
    return last.c_str();
}

}  // namespace

TEST(MeadeDistance, idle_emits_space)
{
    FakeHandlers h;
    h.slewing = false;
    EXPECT_STREQ(" #", dispatch("", h));
    EXPECT_EQ(1, h.callCount);
}

TEST(MeadeDistance, slewing_emits_pipe)
{
    FakeHandlers h;
    h.slewing = true;
    EXPECT_STREQ("|#", dispatch("", h));
    EXPECT_EQ(1, h.callCount);
}

TEST(MeadeDistance, ignores_suffix_bytes)
{
    FakeHandlers h;
    h.slewing = true;
    EXPECT_STREQ("|#", dispatch("xyz", h));
    EXPECT_EQ(1, h.callCount);
}
