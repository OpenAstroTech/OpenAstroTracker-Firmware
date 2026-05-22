// Wire-byte tests for the Meade SyncControl dispatcher (`handleMeadeSyncControl`).
//
// `:CM#` syncs the mount to the previously-set target and emits "NONE#".
// Any other suffix elicits "FAIL#" with no handler call.

#include <gtest/gtest.h>

#include "core/meade/MeadeParser.hpp"

namespace meade = oat::core::meade;

namespace
{

class FakeHandlers : public meade::IMeadeSyncControlHandlers
{
  public:
    int callCount = 0;
    void onSyncToTarget() override
    {
        ++callCount;
    }
};

const char *dispatch(const char *suffix, FakeHandlers &h)
{
    static meade::MeadeResponse last;
    last.clear();
    meade::handleMeadeSyncControl(last, suffix, h);
    return last.c_str();
}

}  // namespace

TEST(MeadeSync, to_target_emits_none)
{
    FakeHandlers h;
    EXPECT_STREQ("NONE#", dispatch("M", h));
    EXPECT_EQ(1, h.callCount);
}

TEST(MeadeSync, unknown_suffix_emits_fail)
{
    FakeHandlers h;
    EXPECT_STREQ("FAIL#", dispatch("Q", h));
    EXPECT_EQ(0, h.callCount);
}

TEST(MeadeSync, empty_suffix_emits_fail)
{
    FakeHandlers h;
    EXPECT_STREQ("FAIL#", dispatch("", h));
    EXPECT_EQ(0, h.callCount);
}

TEST(MeadeSync, trailing_bytes_emit_fail)
{
    FakeHandlers h;
    EXPECT_STREQ("FAIL#", dispatch("Mx", h));
    EXPECT_EQ(0, h.callCount);
}
