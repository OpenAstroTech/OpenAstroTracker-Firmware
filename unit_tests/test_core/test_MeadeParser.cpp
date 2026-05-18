#include <unity.h>

#include "core/MeadeParser.hpp"

namespace meade = oat::core::meade;

using meade::MeadeCommandDispatchTarget;
using meade::MeadeCommandKind;
using meade::MeadeExtraCommandKind;
using meade::MeadeExtraLeafCommandKind;
using meade::MeadeExtraLeafParseResult;
using meade::MeadeExtraParseResult;
using meade::MeadeFocusCommandKind;
using meade::MeadeFocusParseResult;
using meade::MeadeGpsCommandKind;
using meade::MeadeGpsParseResult;
using meade::MeadeHomeCommandKind;
using meade::MeadeHomeParseResult;
using meade::MeadeMovementCommandKind;
using meade::MeadeMovementParseResult;
using meade::MeadeParseResult;
using meade::MeadeSlewRateCommandKind;
using meade::MeadeSlewRateParseResult;
using meade::MeadeSyncCommandKind;
using meade::MeadeSyncParseResult;
using meade::parseMeadeCommand;
using meade::parseMeadeExtraCommand;
using meade::parseMeadeExtraLeafCommand;
using meade::parseMeadeFocusCommand;
using meade::parseMeadeGpsCommand;
using meade::parseMeadeHomeCommand;
using meade::parseMeadeMovementCommand;
using meade::parseMeadeSlewRateCommand;
using meade::parseMeadeSyncCommand;

void setUp(void)
{
}

void tearDown(void)
{
}

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

void assert_valid_extra_parse(const char *input, MeadeExtraCommandKind expected_kind, const char *expected_payload)
{
    MeadeExtraParseResult result = parseMeadeExtraCommand(input);
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(expected_kind), static_cast<int>(result.kind));
    TEST_ASSERT_EQUAL_STRING(expected_payload, result.payload.c_str());
}

void assert_valid_gps_parse(const char *input, MeadeGpsCommandKind expected_kind, const char *expected_payload)
{
    MeadeGpsParseResult result = parseMeadeGpsCommand(input);
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(expected_kind), static_cast<int>(result.kind));
    TEST_ASSERT_EQUAL_STRING(expected_payload, result.payload.c_str());
}

void assert_valid_sync_parse(const char *input, MeadeSyncCommandKind expected_kind)
{
    MeadeSyncParseResult result = parseMeadeSyncCommand(input);
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(expected_kind), static_cast<int>(result.kind));
    TEST_ASSERT_TRUE(result.payload.empty());
}

void assert_valid_movement_parse(const char *input, MeadeMovementCommandKind expected_kind, const char *expected_payload)
{
    MeadeMovementParseResult result = parseMeadeMovementCommand(input);
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(expected_kind), static_cast<int>(result.kind));
    TEST_ASSERT_EQUAL_STRING(expected_payload, result.payload.c_str());
}

void assert_valid_home_parse(const char *input, MeadeHomeCommandKind expected_kind)
{
    MeadeHomeParseResult result = parseMeadeHomeCommand(input);
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(expected_kind), static_cast<int>(result.kind));
    TEST_ASSERT_TRUE(result.payload.empty());
}

void assert_valid_slew_rate_parse(const char *input, MeadeSlewRateCommandKind expected_kind)
{
    MeadeSlewRateParseResult result = parseMeadeSlewRateCommand(input);
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(expected_kind), static_cast<int>(result.kind));
    TEST_ASSERT_TRUE(result.payload.empty());
}

void assert_valid_focus_parse(const char *input, MeadeFocusCommandKind expected_kind, const char *expected_payload)
{
    MeadeFocusParseResult result = parseMeadeFocusCommand(input);
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(expected_kind), static_cast<int>(result.kind));
    TEST_ASSERT_EQUAL_STRING(expected_payload, result.payload.c_str());
}

void assert_valid_extra_leaf_parse(MeadeExtraCommandKind family,
                                   const char *input,
                                   MeadeExtraLeafCommandKind expected_kind,
                                   const char *expected_payload)
{
    MeadeExtraLeafParseResult result = parseMeadeExtraLeafCommand(family, input);
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(expected_kind), static_cast<int>(result.kind));
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

void test_meade_parser_classifies_gps_family_commands(void)
{
    assert_valid_gps_parse("T", MeadeGpsCommandKind::StartAcquisition, "");
    assert_valid_gps_parse("T120000", MeadeGpsCommandKind::StartAcquisition, "120000");
}

void test_meade_parser_rejects_unknown_gps_family_commands(void)
{
    MeadeGpsParseResult result = parseMeadeGpsCommand("Q");
    TEST_ASSERT_FALSE(result.valid);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MeadeGpsCommandKind::Unknown), static_cast<int>(result.kind));
    TEST_ASSERT_TRUE(result.payload.empty());
}

void test_meade_parser_classifies_sync_family_commands(void)
{
    assert_valid_sync_parse("M", MeadeSyncCommandKind::SyncToTarget);
}

void test_meade_parser_rejects_unknown_sync_family_commands(void)
{
    MeadeSyncParseResult result = parseMeadeSyncCommand("Q");
    TEST_ASSERT_FALSE(result.valid);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MeadeSyncCommandKind::Unknown), static_cast<int>(result.kind));
    TEST_ASSERT_TRUE(result.payload.empty());
}

void test_meade_parser_classifies_movement_family_commands(void)
{
    assert_valid_movement_parse("S", MeadeMovementCommandKind::SlewToTarget, "");
    assert_valid_movement_parse("T1", MeadeMovementCommandKind::TrackingToggle, "1");
    assert_valid_movement_parse("Gn0403", MeadeMovementCommandKind::GuidePulse, "n0403");
    assert_valid_movement_parse("gE0403", MeadeMovementCommandKind::GuidePulse, "E0403");
    assert_valid_movement_parse("AA", MeadeMovementCommandKind::MoveAzAltHome, "");
    assert_valid_movement_parse("AZ+32.1", MeadeMovementCommandKind::MoveAzimuth, "+32.1");
    assert_valid_movement_parse("AL-32.1", MeadeMovementCommandKind::MoveAltitude, "-32.1");
    assert_valid_movement_parse("e", MeadeMovementCommandKind::SlewEast, "");
    assert_valid_movement_parse("w", MeadeMovementCommandKind::SlewWest, "");
    assert_valid_movement_parse("n", MeadeMovementCommandKind::SlewNorth, "");
    assert_valid_movement_parse("s", MeadeMovementCommandKind::SlewSouth, "");
    assert_valid_movement_parse("Xr123", MeadeMovementCommandKind::MoveStepper, "r123");
    assert_valid_movement_parse("HRR30", MeadeMovementCommandKind::HomeRa, "R30");
    assert_valid_movement_parse("HDU45", MeadeMovementCommandKind::HomeDec, "U45");
}

void test_meade_parser_rejects_unknown_movement_family_commands(void)
{
    MeadeMovementParseResult result = parseMeadeMovementCommand("Q");
    TEST_ASSERT_FALSE(result.valid);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MeadeMovementCommandKind::Unknown), static_cast<int>(result.kind));
    TEST_ASSERT_TRUE(result.payload.empty());
}

void test_meade_parser_classifies_home_family_commands(void)
{
    assert_valid_home_parse("P", MeadeHomeCommandKind::Park);
    assert_valid_home_parse("F", MeadeHomeCommandKind::Home);
    assert_valid_home_parse("U", MeadeHomeCommandKind::Unpark);
    assert_valid_home_parse("Z", MeadeHomeCommandKind::SetAzAltHome);
}

void test_meade_parser_rejects_unknown_home_family_commands(void)
{
    MeadeHomeParseResult result = parseMeadeHomeCommand("Q");
    TEST_ASSERT_FALSE(result.valid);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MeadeHomeCommandKind::Unknown), static_cast<int>(result.kind));
    TEST_ASSERT_TRUE(result.payload.empty());
}

void test_meade_parser_classifies_slew_rate_family_commands(void)
{
    assert_valid_slew_rate_parse("S", MeadeSlewRateCommandKind::Slew);
    assert_valid_slew_rate_parse("M", MeadeSlewRateCommandKind::Find);
    assert_valid_slew_rate_parse("C", MeadeSlewRateCommandKind::Center);
    assert_valid_slew_rate_parse("G", MeadeSlewRateCommandKind::Guide);
}

void test_meade_parser_rejects_unknown_slew_rate_family_commands(void)
{
    MeadeSlewRateParseResult result = parseMeadeSlewRateCommand("Q");
    TEST_ASSERT_FALSE(result.valid);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MeadeSlewRateCommandKind::Unknown), static_cast<int>(result.kind));
    TEST_ASSERT_TRUE(result.payload.empty());
}

void test_meade_parser_classifies_focus_family_commands(void)
{
    assert_valid_focus_parse("+", MeadeFocusCommandKind::ContinuousIn, "");
    assert_valid_focus_parse("-", MeadeFocusCommandKind::ContinuousOut, "");
    assert_valid_focus_parse("M123", MeadeFocusCommandKind::MoveBy, "123");
    assert_valid_focus_parse("1", MeadeFocusCommandKind::SetSpeedByRate, "1");
    assert_valid_focus_parse("4extra", MeadeFocusCommandKind::SetSpeedByRate, "4extra");
    assert_valid_focus_parse("F", MeadeFocusCommandKind::SetFastestRate, "");
    assert_valid_focus_parse("S", MeadeFocusCommandKind::SetSlowestRate, "");
    assert_valid_focus_parse("p", MeadeFocusCommandKind::GetPosition, "");
    assert_valid_focus_parse("P321", MeadeFocusCommandKind::SetPosition, "321");
    assert_valid_focus_parse("B", MeadeFocusCommandKind::GetState, "");
    assert_valid_focus_parse("Q", MeadeFocusCommandKind::Stop, "");
}

void test_meade_parser_rejects_unknown_focus_family_commands(void)
{
    MeadeFocusParseResult result = parseMeadeFocusCommand("Z");
    TEST_ASSERT_FALSE(result.valid);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MeadeFocusCommandKind::Unknown), static_cast<int>(result.kind));
    TEST_ASSERT_TRUE(result.payload.empty());
}

void test_meade_parser_classifies_x_family_commands(void)
{
    assert_valid_extra_parse("D120", MeadeExtraCommandKind::DriftAlignment, "120");
    assert_valid_extra_parse("GR", MeadeExtraCommandKind::Get, "R");
    assert_valid_extra_parse("SDLU12", MeadeExtraCommandKind::Set, "DLU12");
    assert_valid_extra_parse("LGC", MeadeExtraCommandKind::Level, "GC");
    assert_valid_extra_parse("FR", MeadeExtraCommandKind::FactoryReset, "");
}

void test_meade_parser_rejects_unknown_x_family_commands(void)
{
    MeadeExtraParseResult result = parseMeadeExtraCommand("Z42");
    TEST_ASSERT_FALSE(result.valid);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MeadeExtraCommandKind::Unknown), static_cast<int>(result.kind));
    TEST_ASSERT_TRUE(result.payload.empty());
}

void test_meade_parser_classifies_xg_leaf_commands(void)
{
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Get, "R", MeadeExtraLeafCommandKind::GetRaStepsPerDegree, "");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Get, "D", MeadeExtraLeafCommandKind::GetDecStepsPerDegree, "");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Get, "DLL", MeadeExtraLeafCommandKind::GetDecLimitLowerOnly, "");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Get, "DLU", MeadeExtraLeafCommandKind::GetDecLimitUpperOnly, "");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Get, "DLQ", MeadeExtraLeafCommandKind::GetDecLimitInvalidVariant, "Q");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Get, "S", MeadeExtraLeafCommandKind::GetTrackingSpeedCalibration, "");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Get, "ST", MeadeExtraLeafCommandKind::GetRemainingSafeTime, "");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Get, "AA", MeadeExtraLeafCommandKind::GetAzAltPositions, "");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Get, "AH", MeadeExtraLeafCommandKind::GetAutoHomingStates, "");
    assert_valid_extra_leaf_parse(
        MeadeExtraCommandKind::Get, "C12.3*45.6", MeadeExtraLeafCommandKind::GetTargetCoordinatePositions, "12.3*45.6");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Get, "MS", MeadeExtraLeafCommandKind::GetStepperInfo, "");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Get, "M", MeadeExtraLeafCommandKind::GetMountHardwareInfo, "");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Get, "HR", MeadeExtraLeafCommandKind::GetRaHomingOffset, "");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Get, "HD", MeadeExtraLeafCommandKind::GetDecHomingOffset, "");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Get, "HS", MeadeExtraLeafCommandKind::GetHemisphere, "");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Get, "HQ", MeadeExtraLeafCommandKind::GetHourAngleInvalidVariant, "Q");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Get, "H", MeadeExtraLeafCommandKind::GetHourAngle, "");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Get, "L", MeadeExtraLeafCommandKind::GetLocalSiderealTime, "");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Get, "N", MeadeExtraLeafCommandKind::GetNetworkStatus, "");
}

void test_meade_parser_rejects_unknown_xg_leaf_commands(void)
{
    MeadeExtraLeafParseResult result = parseMeadeExtraLeafCommand(MeadeExtraCommandKind::Get, "Q");
    TEST_ASSERT_FALSE(result.valid);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MeadeExtraLeafCommandKind::Unknown), static_cast<int>(result.kind));
    TEST_ASSERT_TRUE(result.payload.empty());
}

void test_meade_parser_classifies_xs_leaf_commands(void)
{
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Set, "R12.3", MeadeExtraLeafCommandKind::SetRaStepsPerDegree, "12.3");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Set, "A1.2", MeadeExtraLeafCommandKind::SetAzStepsPerDegree, "1.2");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Set, "L2.3", MeadeExtraLeafCommandKind::SetAltStepsPerDegree, "2.3");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Set, "D3.4", MeadeExtraLeafCommandKind::SetDecStepsPerDegree, "3.4");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Set, "DLL5.6", MeadeExtraLeafCommandKind::SetDecLimitLowerSet, "5.6");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Set, "DLL", MeadeExtraLeafCommandKind::SetDecLimitLowerSet, "");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Set, "DLU7.8", MeadeExtraLeafCommandKind::SetDecLimitUpperSet, "7.8");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Set, "DLU", MeadeExtraLeafCommandKind::SetDecLimitUpperSet, "");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Set, "DLl", MeadeExtraLeafCommandKind::SetDecLimitLowerClear, "");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Set, "DLu", MeadeExtraLeafCommandKind::SetDecLimitUpperClear, "");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Set, "DP123", MeadeExtraLeafCommandKind::SetDecParking, "123");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Set, "S1.111", MeadeExtraLeafCommandKind::SetTrackingSpeedCalibration, "1.111");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Set, "T123", MeadeExtraLeafCommandKind::SetTrackingStepperPosition, "123");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Set, "M1", MeadeExtraLeafCommandKind::SetManualSlewMode, "1");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Set, "X1.5", MeadeExtraLeafCommandKind::SetRaManualSpeed, "1.5");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Set, "Y2.5", MeadeExtraLeafCommandKind::SetDecManualSpeed, "2.5");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Set, "B42", MeadeExtraLeafCommandKind::SetBacklashCorrection, "42");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Set, "HR101", MeadeExtraLeafCommandKind::SetRaHomingOffset, "101");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Set, "HD202", MeadeExtraLeafCommandKind::SetDecHomingOffset, "202");
}

void test_meade_parser_rejects_unknown_xs_leaf_commands(void)
{
    MeadeExtraLeafParseResult result = parseMeadeExtraLeafCommand(MeadeExtraCommandKind::Set, "Q");
    TEST_ASSERT_FALSE(result.valid);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MeadeExtraLeafCommandKind::Unknown), static_cast<int>(result.kind));
    TEST_ASSERT_TRUE(result.payload.empty());
}

void test_meade_parser_classifies_xl_leaf_commands(void)
{
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Level, "GR", MeadeExtraLeafCommandKind::LevelGetReferenceAngles, "");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Level, "GC", MeadeExtraLeafCommandKind::LevelGetCurrentAngles, "");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Level, "GT", MeadeExtraLeafCommandKind::LevelGetTemperature, "");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Level, "GQ", MeadeExtraLeafCommandKind::LevelGetInvalidVariant, "Q");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Level, "SP1.25", MeadeExtraLeafCommandKind::LevelSetReferencePitch, "1.25");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Level, "SR2.5", MeadeExtraLeafCommandKind::LevelSetReferenceRoll, "2.5");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Level, "SQ", MeadeExtraLeafCommandKind::LevelSetInvalidVariant, "Q");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Level, "1", MeadeExtraLeafCommandKind::LevelStartup, "");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Level, "0", MeadeExtraLeafCommandKind::LevelShutdown, "");
    assert_valid_extra_leaf_parse(MeadeExtraCommandKind::Level, "Q", MeadeExtraLeafCommandKind::LevelUnknownVariant, "Q");
}

void test_meade_parser_rejects_unknown_xl_leaf_commands(void)
{
    MeadeExtraLeafParseResult result = parseMeadeExtraLeafCommand(MeadeExtraCommandKind::Level, "");
    TEST_ASSERT_FALSE(result.valid);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MeadeExtraLeafCommandKind::Unknown), static_cast<int>(result.kind));
    TEST_ASSERT_TRUE(result.payload.empty());
}

void test_meade_parser_rejects_leaf_parsing_for_non_leaf_x_family_commands(void)
{
    MeadeExtraLeafParseResult result = parseMeadeExtraLeafCommand(MeadeExtraCommandKind::FactoryReset, "");
    TEST_ASSERT_FALSE(result.valid);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MeadeExtraLeafCommandKind::Unknown), static_cast<int>(result.kind));
    TEST_ASSERT_TRUE(result.payload.empty());
}

void process()
{
    UNITY_BEGIN();
    RUN_TEST(test_meade_parser_rejects_empty_and_too_short_inputs);
    RUN_TEST(test_meade_parser_rejects_missing_colon);
    RUN_TEST(test_meade_parser_returns_family_and_payload_for_get_ra);
    RUN_TEST(test_meade_parser_strips_spaces_and_trailing_hash);
    RUN_TEST(test_meade_parser_preserves_payload_for_quit_command);
    RUN_TEST(test_meade_parser_accepts_command_without_trailing_hash);
    RUN_TEST(test_meade_parser_classifies_all_top_level_families);
    RUN_TEST(test_meade_parser_rejects_unknown_top_level_family);
    RUN_TEST(test_meade_parser_classifies_gps_family_commands);
    RUN_TEST(test_meade_parser_rejects_unknown_gps_family_commands);
    RUN_TEST(test_meade_parser_classifies_sync_family_commands);
    RUN_TEST(test_meade_parser_rejects_unknown_sync_family_commands);
    RUN_TEST(test_meade_parser_classifies_movement_family_commands);
    RUN_TEST(test_meade_parser_rejects_unknown_movement_family_commands);
    RUN_TEST(test_meade_parser_classifies_home_family_commands);
    RUN_TEST(test_meade_parser_rejects_unknown_home_family_commands);
    RUN_TEST(test_meade_parser_classifies_slew_rate_family_commands);
    RUN_TEST(test_meade_parser_rejects_unknown_slew_rate_family_commands);
    RUN_TEST(test_meade_parser_classifies_focus_family_commands);
    RUN_TEST(test_meade_parser_rejects_unknown_focus_family_commands);
    RUN_TEST(test_meade_parser_classifies_x_family_commands);
    RUN_TEST(test_meade_parser_rejects_unknown_x_family_commands);
    RUN_TEST(test_meade_parser_classifies_xg_leaf_commands);
    RUN_TEST(test_meade_parser_rejects_unknown_xg_leaf_commands);
    RUN_TEST(test_meade_parser_classifies_xs_leaf_commands);
    RUN_TEST(test_meade_parser_rejects_unknown_xs_leaf_commands);
    RUN_TEST(test_meade_parser_classifies_xl_leaf_commands);
    RUN_TEST(test_meade_parser_rejects_unknown_xl_leaf_commands);
    RUN_TEST(test_meade_parser_rejects_leaf_parsing_for_non_leaf_x_family_commands);
    UNITY_END();
}

#if defined(ARDUINO)
    #include <Arduino.h>
void setup()
{
    delay(2000);
    process();
}

void loop()
{
}
#else
int main()
{
    process();
    return 0;
}
#endif