// Wire-byte tests for the Meade Home dispatcher (`handleMeadeHome`).
//
// `:hP#`/`:hF#` perform park/slew-to-home and emit an empty response.
// `:hU#`/`:hZ#` unpark / set the Az/Alt home and emit `"1"`. Any other
// suffix is ignored and produces an empty response.

#include <unity.h>

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

namespace
{

void test_home_p_parks_and_emits_empty()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("P", h));
    TEST_ASSERT_EQUAL_STRING("park", h.lastCall);
}

void test_home_f_slews_home_and_emits_empty()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("F", h));
    TEST_ASSERT_EQUAL_STRING("home", h.lastCall);
}

void test_home_u_unparks_and_emits_one()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("1", dispatch("U", h));
    TEST_ASSERT_EQUAL_STRING("unpark", h.lastCall);
}

void test_home_z_sets_azalt_and_emits_one()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("1", dispatch("Z", h));
    TEST_ASSERT_EQUAL_STRING("azalt", h.lastCall);
}

void test_home_unknown_suffix_emits_empty()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("Q", h));
    TEST_ASSERT_EQUAL_STRING("", h.lastCall);
}

void test_home_empty_suffix_emits_empty()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("", h));
    TEST_ASSERT_EQUAL_STRING("", h.lastCall);
}

void test_home_trailing_bytes_do_not_call_handler()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("Px", h));
    TEST_ASSERT_EQUAL_STRING("", h.lastCall);
}

}  // namespace

void register_meade_home_tests()
{
    RUN_TEST(test_home_p_parks_and_emits_empty);
    RUN_TEST(test_home_f_slews_home_and_emits_empty);
    RUN_TEST(test_home_u_unparks_and_emits_one);
    RUN_TEST(test_home_z_sets_azalt_and_emits_one);
    RUN_TEST(test_home_unknown_suffix_emits_empty);
    RUN_TEST(test_home_empty_suffix_emits_empty);
    RUN_TEST(test_home_trailing_bytes_do_not_call_handler);
}
