#include <unity.h>

#include "MappedDict.hpp"

void test_function_mapped_dict_simple_lookups(void)
{
    MappedDict<char, int>::DictEntry_t lookupTable[] = {
        {'0', 0},
        {'1', 1},
        {'2', 2},
    };
    auto idxLookup = MappedDict<char, int>(lookupTable, 3);
    int intIdx;
    (void) idxLookup.tryGet('0', &intIdx);
    TEST_ASSERT_EQUAL(0, intIdx);
    (void) idxLookup.tryGet('1', &intIdx);
    TEST_ASSERT_EQUAL(1, intIdx);
    (void) idxLookup.tryGet('2', &intIdx);
    TEST_ASSERT_EQUAL(2, intIdx);
}

void test_function_mapped_dict_bounds(void)
{
    MappedDict<char, int>::DictEntry_t lookupTable[] = {
        {'0', 0},
        {'1', 1},
    };
    auto idxLookup = MappedDict<char, int>(lookupTable, 2);
    int intIdx;
    bool found = idxLookup.tryGet('0', &intIdx);
    TEST_ASSERT_EQUAL(0, intIdx);
    TEST_ASSERT_TRUE(found);
    found = idxLookup.tryGet('2', &intIdx);
    TEST_ASSERT_NOT_EQUAL(2, intIdx);
    TEST_ASSERT_FALSE(found);
}

void test_function_mapped_dict_zero_size(void)
{
    MappedDict<char, int>::DictEntry_t lookupTable[] = {};

    auto idxLookup = MappedDict<char, int>(lookupTable, 0);
    int intIdx;
    bool found = idxLookup.tryGet('0', &intIdx);
    TEST_ASSERT_FALSE(found);
}

void test_function_mapped_dict_pointer_types(void)
{
    MappedDict<const char *, int>::DictEntry_t lookupTable[] = {
        {"One", 1},
        {"Two", 2},
    };
    auto idxLookup = MappedDict<const char *, int>(lookupTable, 2);
    int intIdx;
    bool found = idxLookup.tryGet("One", &intIdx);
    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_EQUAL(1, intIdx);
    found = idxLookup.tryGet("Two", &intIdx);
    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_EQUAL(2, intIdx);
}

void process()
{
    UNITY_BEGIN();
    RUN_TEST(test_function_mapped_dict_simple_lookups);
    RUN_TEST(test_function_mapped_dict_bounds);
    RUN_TEST(test_function_mapped_dict_zero_size);
    RUN_TEST(test_function_mapped_dict_pointer_types);
    UNITY_END();
}

#if defined(ARDUINO)
    #include <Arduino.h>
void setup()
{
    delay(2000);  // Just if board doesn't support software reset via Serial.DTR/RTS
    process();
}

void loop()
{
    digitalWrite(13, HIGH);
    delay(100);
    digitalWrite(13, LOW);
    delay(500);
}
#else
int main(int argc, char **argv)
{
    process();
    return 0;
}
#endif
