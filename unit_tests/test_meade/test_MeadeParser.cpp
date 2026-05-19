#include <unity.h>

#include "core/MeadeParser.hpp"

namespace meade = oat::core::meade;

using meade::MeadeParseResult;
using meade::parseMeadeCommand;

namespace
{

void assert_invalid_parse(const char *input)
{
    MeadeParseResult result = parseMeadeCommand(input);
    TEST_ASSERT_FALSE(result.valid);
    TEST_ASSERT_EQUAL_CHAR('\0', result.family);
    TEST_ASSERT_TRUE(result.payload.empty());
}

void assert_valid_parse(const char *input, char expected_family, const char *expected_payload)
{
    MeadeParseResult result = parseMeadeCommand(input);
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_CHAR(expected_family, result.family);
    TEST_ASSERT_EQUAL_STRING(expected_payload, result.payload.c_str());
}

void test_meade_parser_rejects_empty_and_too_short_inputs(void)
{
    assert_invalid_parse("");
    assert_invalid_parse(":");
}

void test_meade_parser_rejects_missing_colon(void)
{
    assert_invalid_parse("GR#");
}

void test_meade_parser_returns_family_and_payload_for_get_ra(void)
{
    MeadeParseResult result = parseMeadeCommand(":GR#");
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_CHAR('G', result.family);
    TEST_ASSERT_EQUAL_STRING("R", result.payload.c_str());
}

void test_meade_parser_strips_spaces_and_trailing_hash(void)
{
    MeadeParseResult result = parseMeadeCommand(": G R #");
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_CHAR('G', result.family);
    TEST_ASSERT_EQUAL_STRING("R", result.payload.c_str());
}

void test_meade_parser_preserves_payload_for_quit_command(void)
{
    MeadeParseResult result = parseMeadeCommand(":Qq#");
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_CHAR('Q', result.family);
    TEST_ASSERT_EQUAL_STRING("q", result.payload.c_str());
}

void test_meade_parser_accepts_command_without_trailing_hash(void)
{
    MeadeParseResult result = parseMeadeCommand(":MS");
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_CHAR('M', result.family);
    TEST_ASSERT_EQUAL_STRING("S", result.payload.c_str());
}

void test_meade_parser_classifies_all_top_level_families(void)
{
    struct ParseCase {
        const char *input;
        char family;
        const char *payload;
    };

    static const ParseCase parse_cases[] = {
        {":Sd+12*34:56#", 'S', "d+12*34:56"},
        {":MS#", 'M', "S"},
        {":GR#", 'G', "R"},
        {":gT#", 'g', "T"},
        {":CM#", 'C', "M"},
        {":hP#", 'h', "P"},
        {":I#", 'I', ""},
        {":Qq#", 'Q', "q"},
        {":RS#", 'R', "S"},
        {":D#", 'D', ""},
        {":XFR#", 'X', "FR"},
        {":F+#", 'F', "+"},
    };

    for (unsigned int index = 0; index < (sizeof(parse_cases) / sizeof(parse_cases[0])); ++index)
    {
        assert_valid_parse(parse_cases[index].input, parse_cases[index].family, parse_cases[index].payload);
    }
}

void test_meade_parser_rejects_unknown_top_level_family(void)
{
    MeadeParseResult result = parseMeadeCommand(":Z12#");
    TEST_ASSERT_FALSE(result.valid);
    TEST_ASSERT_EQUAL_CHAR('\0', result.family);
    TEST_ASSERT_TRUE(result.payload.empty());
}

}  // namespace

void register_meade_parser_tests()
{
    RUN_TEST(test_meade_parser_rejects_empty_and_too_short_inputs);
    RUN_TEST(test_meade_parser_rejects_missing_colon);
    RUN_TEST(test_meade_parser_returns_family_and_payload_for_get_ra);
    RUN_TEST(test_meade_parser_strips_spaces_and_trailing_hash);
    RUN_TEST(test_meade_parser_preserves_payload_for_quit_command);
    RUN_TEST(test_meade_parser_accepts_command_without_trailing_hash);
    RUN_TEST(test_meade_parser_classifies_all_top_level_families);
    RUN_TEST(test_meade_parser_rejects_unknown_top_level_family);
}
