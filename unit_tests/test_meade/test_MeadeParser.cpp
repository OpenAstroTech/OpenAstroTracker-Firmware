#include <unity.h>

#include "core/MeadeParser.hpp"

namespace meade = oat::core::meade;

using meade::MeadeCommandDispatchTarget;
using meade::MeadeCommandKind;
using meade::MeadeParseResult;
using meade::parseMeadeCommand;

namespace
{

void assert_invalid_parse(const char *input)
{
    MeadeParseResult result = parseMeadeCommand(input);
    TEST_ASSERT_FALSE(result.valid);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MeadeCommandKind::Unknown), static_cast<int>(result.kind));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MeadeCommandDispatchTarget::Unknown), static_cast<int>(result.dispatchTarget));
    TEST_ASSERT_TRUE(result.payload.empty());
}

void assert_valid_parse(const char *input,
                        MeadeCommandKind expected_kind,
                        MeadeCommandDispatchTarget expected_dispatch_target,
                        const char *expected_payload)
{
    MeadeParseResult result = parseMeadeCommand(input);
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(expected_kind), static_cast<int>(result.kind));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(expected_dispatch_target), static_cast<int>(result.dispatchTarget));
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
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MeadeCommandKind::Get), static_cast<int>(result.kind));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MeadeCommandDispatchTarget::GetInfo), static_cast<int>(result.dispatchTarget));
    TEST_ASSERT_EQUAL_STRING("R", result.payload.c_str());
}

void test_meade_parser_strips_spaces_and_trailing_hash(void)
{
    MeadeParseResult result = parseMeadeCommand(": G R #");
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MeadeCommandKind::Get), static_cast<int>(result.kind));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MeadeCommandDispatchTarget::GetInfo), static_cast<int>(result.dispatchTarget));
    TEST_ASSERT_EQUAL_STRING("R", result.payload.c_str());
}

void test_meade_parser_preserves_payload_for_quit_command(void)
{
    MeadeParseResult result = parseMeadeCommand(":Qq#");
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MeadeCommandKind::Quit), static_cast<int>(result.kind));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MeadeCommandDispatchTarget::Quit), static_cast<int>(result.dispatchTarget));
    TEST_ASSERT_EQUAL_STRING("q", result.payload.c_str());
}

void test_meade_parser_accepts_command_without_trailing_hash(void)
{
    MeadeParseResult result = parseMeadeCommand(":MS");
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MeadeCommandKind::Move), static_cast<int>(result.kind));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MeadeCommandDispatchTarget::Movement), static_cast<int>(result.dispatchTarget));
    TEST_ASSERT_EQUAL_STRING("S", result.payload.c_str());
}

void test_meade_parser_classifies_all_top_level_families(void)
{
    struct ParseCase {
        const char *input;
        MeadeCommandKind kind;
        MeadeCommandDispatchTarget dispatchTarget;
        const char *payload;
    };

    static const ParseCase parse_cases[] = {
        {":Sd+12*34:56#", MeadeCommandKind::Set, MeadeCommandDispatchTarget::SetInfo, "d+12*34:56"},
        {":MS#", MeadeCommandKind::Move, MeadeCommandDispatchTarget::Movement, "S"},
        {":GR#", MeadeCommandKind::Get, MeadeCommandDispatchTarget::GetInfo, "R"},
        {":gT#", MeadeCommandKind::Gps, MeadeCommandDispatchTarget::GpsCommands, "T"},
        {":CM#", MeadeCommandKind::Sync, MeadeCommandDispatchTarget::SyncControl, "M"},
        {":hP#", MeadeCommandKind::Home, MeadeCommandDispatchTarget::Home, "P"},
        {":I#", MeadeCommandKind::Init, MeadeCommandDispatchTarget::Init, ""},
        {":Qq#", MeadeCommandKind::Quit, MeadeCommandDispatchTarget::Quit, "q"},
        {":RS#", MeadeCommandKind::SlewRate, MeadeCommandDispatchTarget::SetSlewRate, "S"},
        {":D#", MeadeCommandKind::Distance, MeadeCommandDispatchTarget::Distance, ""},
        {":XFR#", MeadeCommandKind::Extra, MeadeCommandDispatchTarget::ExtraCommands, "FR"},
        {":F+#", MeadeCommandKind::Focus, MeadeCommandDispatchTarget::FocusCommands, "+"},
    };

    for (unsigned int index = 0; index < (sizeof(parse_cases) / sizeof(parse_cases[0])); ++index)
    {
        assert_valid_parse(
            parse_cases[index].input, parse_cases[index].kind, parse_cases[index].dispatchTarget, parse_cases[index].payload);
    }
}

void test_meade_parser_rejects_unknown_top_level_family(void)
{
    MeadeParseResult result = parseMeadeCommand(":Z12#");
    TEST_ASSERT_FALSE(result.valid);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MeadeCommandKind::Unknown), static_cast<int>(result.kind));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MeadeCommandDispatchTarget::Unknown), static_cast<int>(result.dispatchTarget));
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