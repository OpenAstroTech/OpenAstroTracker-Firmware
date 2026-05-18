// Wire-byte tests for the Meade SetSlewRate dispatcher (`handleMeadeSetSlewRate`).
//
// `:RS#`/`:RM#`/`:RC#`/`:RG#` map to mount slew rates 4/3/2/1 with empty wire
// responses. Unknown or malformed suffixes are ignored and emit empty.

#include <unity.h>

#include "core/MeadeParser.hpp"
#include "core/MeadeParser.hpp"

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
    last = meade::handleMeadeSetSlewRate(suffix, h);
    return last.c_str();
}

}  // namespace

void setUp(void)
{
}
void tearDown(void)
{
}

void test_slew_rate_s_sets_4()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("S", h));
    TEST_ASSERT_EQUAL_INT(4, h.lastRate);
}

void test_slew_rate_m_sets_3()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("M", h));
    TEST_ASSERT_EQUAL_INT(3, h.lastRate);
}

void test_slew_rate_c_sets_2()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("C", h));
    TEST_ASSERT_EQUAL_INT(2, h.lastRate);
}

void test_slew_rate_g_sets_1()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("G", h));
    TEST_ASSERT_EQUAL_INT(1, h.lastRate);
}

void test_slew_rate_unknown_suffix_no_call()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("Q", h));
    TEST_ASSERT_EQUAL_INT(0, h.callCount);
}

void test_slew_rate_multi_char_suffix_no_call()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("SS", h));
    TEST_ASSERT_EQUAL_INT(0, h.callCount);
}

void test_slew_rate_empty_suffix_no_call()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("", h));
    TEST_ASSERT_EQUAL_INT(0, h.callCount);
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_slew_rate_s_sets_4);
    RUN_TEST(test_slew_rate_m_sets_3);
    RUN_TEST(test_slew_rate_c_sets_2);
    RUN_TEST(test_slew_rate_g_sets_1);
    RUN_TEST(test_slew_rate_unknown_suffix_no_call);
    RUN_TEST(test_slew_rate_multi_char_suffix_no_call);
    RUN_TEST(test_slew_rate_empty_suffix_no_call);
    return UNITY_END();
}
