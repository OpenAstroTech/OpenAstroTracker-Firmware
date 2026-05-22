// Wire-byte tests for the Meade Init dispatcher (`handleMeadeInit`).
//
// `:I#` hands UI over to serial control; the wire response is empty.

#include <gtest/gtest.h>

#include "core/meade/MeadeParser.hpp"

namespace meade = oat::core::meade;

namespace
{

class FakeHandlers : public meade::IMeadeInitHandlers
{
  public:
    int callCount = 0;
    void onEnterSerialControl() override
    {
        ++callCount;
    }
};

const char *dispatch(const char *suffix, FakeHandlers &h)
{
    static meade::MeadeResponse last;
    last.clear();
    meade::handleMeadeInit(last, suffix, h);
    return last.c_str();
}

}  // namespace

TEST(MeadeInit, empty_response)
{
    FakeHandlers h;
    EXPECT_STREQ("", dispatch("", h));
    EXPECT_EQ(1, h.callCount);
}

TEST(MeadeInit, ignores_suffix)
{
    FakeHandlers h;
    EXPECT_STREQ("", dispatch("xyz", h));
    EXPECT_EQ(1, h.callCount);
}
