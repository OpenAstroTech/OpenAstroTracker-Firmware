#include <gtest/gtest.h>

#include "core/meade/MeadeParser.hpp"

namespace meade = oat::core::meade;

using meade::MeadeParseResult;
using meade::parseMeadeCommand;

namespace
{

void assert_invalid_parse(const char *input)
{
    MeadeParseResult result;
    bool ok = parseMeadeCommand(input, result);
    EXPECT_FALSE(ok);
    EXPECT_FALSE(result.valid);
    EXPECT_EQ('\0', result.family);
    EXPECT_TRUE(result.payload.empty());
}

void assert_valid_parse(const char *input, char expected_family, const char *expected_payload)
{
    MeadeParseResult result;
    bool ok = parseMeadeCommand(input, result);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(expected_family, result.family);
    EXPECT_STREQ(expected_payload, result.payload.c_str());
}

}  // namespace

TEST(MeadeParser, rejects_empty_and_too_short_inputs)
{
    assert_invalid_parse("");
    assert_invalid_parse(":");
}

TEST(MeadeParser, rejects_missing_colon)
{
    assert_invalid_parse("GR#");
}

TEST(MeadeParser, returns_family_and_payload_for_get_ra)
{
    MeadeParseResult result;
    parseMeadeCommand(":GR#", result);
    EXPECT_TRUE(result.valid);
    EXPECT_EQ('G', result.family);
    EXPECT_STREQ("R", result.payload.c_str());
}

TEST(MeadeParser, strips_spaces_and_trailing_hash)
{
    MeadeParseResult result;
    parseMeadeCommand(": G R #", result);
    EXPECT_TRUE(result.valid);
    EXPECT_EQ('G', result.family);
    EXPECT_STREQ("R", result.payload.c_str());
}

TEST(MeadeParser, preserves_payload_for_quit_command)
{
    MeadeParseResult result;
    parseMeadeCommand(":Qq#", result);
    EXPECT_TRUE(result.valid);
    EXPECT_EQ('Q', result.family);
    EXPECT_STREQ("q", result.payload.c_str());
}

TEST(MeadeParser, accepts_command_without_trailing_hash)
{
    MeadeParseResult result;
    parseMeadeCommand(":MS", result);
    EXPECT_TRUE(result.valid);
    EXPECT_EQ('M', result.family);
    EXPECT_STREQ("S", result.payload.c_str());
}

TEST(MeadeParser, classifies_all_top_level_families)
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

TEST(MeadeParser, rejects_unknown_top_level_family)
{
    MeadeParseResult result;
    parseMeadeCommand(":Z12#", result);
    EXPECT_FALSE(result.valid);
    EXPECT_EQ('\0', result.family);
    EXPECT_TRUE(result.payload.empty());
}
