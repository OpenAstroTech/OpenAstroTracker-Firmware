// Wire-byte tests for the Meade SyncControl dispatcher (`handleMeadeSyncControl`).
//
// `:CM#` syncs the mount to the previously-set target and emits "NONE#".
// Any other suffix elicits "FAIL#" with no handler call.

#include <unity.h>

#include "core/MeadeParser.hpp"
#include "core/MeadeParser.hpp"

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
    last = meade::handleMeadeSyncControl(suffix, h);
    return last.c_str();
}

}  // namespace

void setUp(void)
{
}
void tearDown(void)
{
}

void test_sync_to_target_emits_none()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("NONE#", dispatch("M", h));
    TEST_ASSERT_EQUAL_INT(1, h.callCount);
}

void test_sync_unknown_suffix_emits_fail()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("FAIL#", dispatch("Q", h));
    TEST_ASSERT_EQUAL_INT(0, h.callCount);
}

void test_sync_empty_suffix_emits_fail()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("FAIL#", dispatch("", h));
    TEST_ASSERT_EQUAL_INT(0, h.callCount);
}

void test_sync_trailing_bytes_emit_fail()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("FAIL#", dispatch("Mx", h));
    TEST_ASSERT_EQUAL_INT(0, h.callCount);
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_sync_to_target_emits_none);
    RUN_TEST(test_sync_unknown_suffix_emits_fail);
    RUN_TEST(test_sync_empty_suffix_emits_fail);
    RUN_TEST(test_sync_trailing_bytes_emit_fail);
    return UNITY_END();
}
