// Wire-byte tests for the Meade Init dispatcher (`handleMeadeInit`).
//
// `:I#` hands UI over to serial control; the wire response is empty.

#include <unity.h>

#include "core/MeadeParser.hpp"
#include "core/MeadeParser.hpp"

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
    last = meade::handleMeadeInit(suffix, h);
    return last.c_str();
}

}  // namespace

void setUp(void)
{
}
void tearDown(void)
{
}

void test_init_empty_response()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("", h));
    TEST_ASSERT_EQUAL_INT(1, h.callCount);
}

void test_init_ignores_suffix()
{
    FakeHandlers h;
    TEST_ASSERT_EQUAL_STRING("", dispatch("xyz", h));
    TEST_ASSERT_EQUAL_INT(1, h.callCount);
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_empty_response);
    RUN_TEST(test_init_ignores_suffix);
    return UNITY_END();
}
