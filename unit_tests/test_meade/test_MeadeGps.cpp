// Wire-byte tests for the Meade GPS dispatcher (`handleMeadeGps`).
//
// `:gT<payload>#` invokes onStartGpsAcquisition with the payload bytes and
// emits "1" on success / "0" on timeout. Any other suffix emits "0" without
// invoking the handler.

#include <cstring>

#include <unity.h>

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

namespace
{

void test_gps_t_success_emits_one()
{
    FakeHandlers h;
    h.nextResult = true;
    TEST_ASSERT_EQUAL_STRING("1", dispatch("T", h));
    TEST_ASSERT_TRUE(h.called);
    TEST_ASSERT_EQUAL_STRING("", h.lastPayload);
}

void test_gps_t_failure_emits_zero()
{
    FakeHandlers h;
    h.nextResult = false;
    TEST_ASSERT_EQUAL_STRING("0", dispatch("T", h));
    TEST_ASSERT_TRUE(h.called);
}

void test_gps_t_with_payload_forwards_payload()
{
    FakeHandlers h;
    h.nextResult = true;
    TEST_ASSERT_EQUAL_STRING("1", dispatch("T120000", h));
    TEST_ASSERT_TRUE(h.called);
    TEST_ASSERT_EQUAL_STRING("120000", h.lastPayload);
}

void test_gps_unknown_suffix_emits_zero_no_call()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("0", dispatch("Q", h));
    TEST_ASSERT_FALSE(h.called);
}

void test_gps_empty_suffix_emits_zero_no_call()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("0", dispatch("", h));
    TEST_ASSERT_FALSE(h.called);
}

}  // namespace

void register_meade_gps_tests()
{
    RUN_TEST(test_gps_t_success_emits_one);
    RUN_TEST(test_gps_t_failure_emits_zero);
    RUN_TEST(test_gps_t_with_payload_forwards_payload);
    RUN_TEST(test_gps_unknown_suffix_emits_zero_no_call);
    RUN_TEST(test_gps_empty_suffix_emits_zero_no_call);
}
