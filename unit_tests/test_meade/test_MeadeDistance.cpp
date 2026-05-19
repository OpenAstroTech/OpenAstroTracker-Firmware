// Wire-byte tests for the Meade Distance-bars dispatcher (`handleMeadeDistance`).
//
// The `:D#` command reports motion status as a single byte ('|' while
// slewing, ' ' when idle) followed by the standard terminator. Any suffix
// is treated identically (legacy lenient behaviour).

#include <unity.h>

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
    last = meade::handleMeadeDistance(suffix, h);
    return last.c_str();
}

}  // namespace

namespace
{

void test_distance_idle_emits_space()
{
    FakeHandlers h;
    h.slewing = false;
    TEST_ASSERT_EQUAL_STRING(" #", dispatch("", h));
    TEST_ASSERT_EQUAL_INT(1, h.callCount);
}

void test_distance_slewing_emits_pipe()
{
    FakeHandlers h;
    h.slewing = true;
    TEST_ASSERT_EQUAL_STRING("|#", dispatch("", h));
    TEST_ASSERT_EQUAL_INT(1, h.callCount);
}

void test_distance_ignores_suffix_bytes()
{
    FakeHandlers h;
    h.slewing = true;
    TEST_ASSERT_EQUAL_STRING("|#", dispatch("xyz", h));
    TEST_ASSERT_EQUAL_INT(1, h.callCount);
}

}  // namespace

void register_meade_distance_tests()
{
    RUN_TEST(test_distance_idle_emits_space);
    RUN_TEST(test_distance_slewing_emits_pipe);
    RUN_TEST(test_distance_ignores_suffix_bytes);
}
