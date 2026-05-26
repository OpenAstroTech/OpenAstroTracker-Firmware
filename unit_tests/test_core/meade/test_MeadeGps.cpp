// Wire-byte tests for the Meade GPS dispatcher (`handleMeadeGps`).
//
// `:gT<payload>#` invokes onStartGpsAcquisition with the payload bytes and
// emits "1" on success / "0" on timeout. Any other suffix emits "0" without
// invoking the handler.

#include <gtest/gtest.h>

#include <cstring>

#include "core/meade/MeadeParser.hpp"

namespace meade = oat::core::meade;

namespace
{

class FakeHandlers : public meade::IMeadeGpsHandlers
{
  public:
    bool nextResult      = false;
    bool called          = false;
    char lastPayload[64] = {0};

    bool onStartGpsAcquisition(const char *timeoutPayload) override
    {
        called = true;
        std::strncpy(lastPayload, timeoutPayload ? timeoutPayload : "", sizeof(lastPayload) - 1);
        return nextResult;
    }
};

const char *dispatch(const char *suffix, FakeHandlers &h)
{
    static meade::MeadeResponse last;
    last.clear();
    meade::handleMeadeGps(last, suffix, h);
    return last.c_str();
}

}  // namespace

TEST(MeadeGps, t_success_emits_one)
{
    FakeHandlers h;
    h.nextResult = true;
    EXPECT_STREQ("1", dispatch("T", h));
    EXPECT_TRUE(h.called);
    EXPECT_STREQ("", h.lastPayload);
}

TEST(MeadeGps, t_failure_emits_zero)
{
    FakeHandlers h;
    h.nextResult = false;
    EXPECT_STREQ("0", dispatch("T", h));
    EXPECT_TRUE(h.called);
}

TEST(MeadeGps, t_with_payload_forwards_payload)
{
    FakeHandlers h;
    h.nextResult = true;
    EXPECT_STREQ("1", dispatch("T120000", h));
    EXPECT_TRUE(h.called);
    EXPECT_STREQ("120000", h.lastPayload);
}

TEST(MeadeGps, unknown_suffix_emits_zero_no_call)
{
    FakeHandlers h;
    EXPECT_STREQ("0", dispatch("Q", h));
    EXPECT_FALSE(h.called);
}

TEST(MeadeGps, empty_suffix_emits_zero_no_call)
{
    FakeHandlers h;
    EXPECT_STREQ("0", dispatch("", h));
    EXPECT_FALSE(h.called);
}
