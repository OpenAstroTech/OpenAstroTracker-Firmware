#include "core/MeadeParser.hpp"

#include <cstddef>
#include <cstring>

namespace oat
{
namespace core
{
namespace meade
{

namespace
{

// Lookup-table entry used for keys that must match the input exactly
// (entire remaining input string, terminator included).
template <typename Kind> struct ExactEntry {
    const char *key;
    Kind kind;
};

// Lookup-table entry used for keys that match the input as a prefix.
// `capturePayload`        -> if true, the text after the key is copied into result.payload.
// `requireNonEmptyTail`   -> if true, the match only succeeds when at least one char
//                            follows the key (used by ExtraSet's "D" entry).
template <typename Kind> struct PrefixEntry {
    const char *key;
    Kind kind;
    bool capturePayload;
    bool requireNonEmptyTail;
};

template <typename Kind, std::size_t N> bool lookupExact(const ExactEntry<Kind> (&table)[N], const char *input, Kind &out)
{
    for (std::size_t i = 0; i < N; ++i)
    {
        if (std::strcmp(table[i].key, input) == 0)
        {
            out = table[i].kind;
            return true;
        }
    }
    return false;
}

// First-match-wins prefix lookup. Tables must list longer/more specific keys
// before shorter ones that share a prefix.
template <typename Kind, std::size_t N>
bool lookupPrefix(const PrefixEntry<Kind> (&table)[N], const char *input, Kind &out, const char *&tail, bool &capturesPayload)
{
    for (std::size_t i = 0; i < N; ++i)
    {
        const char *k = table[i].key;
        const char *p = input;
        while ((*k != '\0') && (*k == *p))
        {
            ++k;
            ++p;
        }
        if (*k != '\0')
        {
            continue;
        }
        if (table[i].requireNonEmptyTail && (*p == '\0'))
        {
            continue;
        }
        out             = table[i].kind;
        tail            = p;
        capturesPayload = table[i].capturePayload;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Top-level family classifier
// ---------------------------------------------------------------------------
struct FamilyEntry {
    char family;
    MeadeCommandKind kind;
    MeadeCommandDispatchTarget target;
};

constexpr FamilyEntry kFamilyTable[] = {
    {'S', MeadeCommandKind::Set, MeadeCommandDispatchTarget::SetInfo},
    {'M', MeadeCommandKind::Move, MeadeCommandDispatchTarget::Movement},
    {'G', MeadeCommandKind::Get, MeadeCommandDispatchTarget::GetInfo},
    {'g', MeadeCommandKind::Gps, MeadeCommandDispatchTarget::GpsCommands},
    {'C', MeadeCommandKind::Sync, MeadeCommandDispatchTarget::SyncControl},
    {'h', MeadeCommandKind::Home, MeadeCommandDispatchTarget::Home},
    {'I', MeadeCommandKind::Init, MeadeCommandDispatchTarget::Init},
    {'Q', MeadeCommandKind::Quit, MeadeCommandDispatchTarget::Quit},
    {'R', MeadeCommandKind::SlewRate, MeadeCommandDispatchTarget::SetSlewRate},
    {'D', MeadeCommandKind::Distance, MeadeCommandDispatchTarget::Distance},
    {'X', MeadeCommandKind::Extra, MeadeCommandDispatchTarget::ExtraCommands},
    {'F', MeadeCommandKind::Focus, MeadeCommandDispatchTarget::FocusCommands},
};

// ---------------------------------------------------------------------------
// Per-family tables
// ---------------------------------------------------------------------------
constexpr ExactEntry<MeadeGetCommandKind> kGetTable[] = {
    {"VN", MeadeGetCommandKind::FirmwareVersion}, {"VP", MeadeGetCommandKind::ProductName}, {"r", MeadeGetCommandKind::TargetRa},
    {"d", MeadeGetCommandKind::TargetDec},        {"R", MeadeGetCommandKind::CurrentRa},    {"D", MeadeGetCommandKind::CurrentDec},
    {"X", MeadeGetCommandKind::MountStatus},      {"IS", MeadeGetCommandKind::IsSlewing},   {"IT", MeadeGetCommandKind::IsTracking},
    {"IG", MeadeGetCommandKind::IsGuiding},       {"t", MeadeGetCommandKind::SiteLatitude}, {"g", MeadeGetCommandKind::SiteLongitude},
    {"c", MeadeGetCommandKind::ClockFormat},      {"G", MeadeGetCommandKind::UtcOffset},    {"a", MeadeGetCommandKind::LocalTime12h},
    {"L", MeadeGetCommandKind::LocalTime24h},     {"C", MeadeGetCommandKind::LocalDate},    {"M", MeadeGetCommandKind::SiteName1},
    {"N", MeadeGetCommandKind::SiteName2},        {"O", MeadeGetCommandKind::SiteName3},    {"P", MeadeGetCommandKind::SiteName4},
    {"T", MeadeGetCommandKind::TrackingRate},
};

constexpr PrefixEntry<MeadeGpsCommandKind> kGpsTable[] = {
    {"T", MeadeGpsCommandKind::StartAcquisition, true, false},
};

// Set: every entry takes a payload. HL/HP must come before bare H.
constexpr PrefixEntry<MeadeSetCommandKind> kSetTable[] = {
    {"HL", MeadeSetCommandKind::LocalSiderealTime, true, false},
    {"HP", MeadeSetCommandKind::HomePoint, true, false},
    {"H", MeadeSetCommandKind::HourAngle, true, false},
    {"d", MeadeSetCommandKind::TargetDec, true, false},
    {"r", MeadeSetCommandKind::TargetRa, true, false},
    {"Y", MeadeSetCommandKind::SyncCoordinates, true, false},
    {"t", MeadeSetCommandKind::SiteLatitude, true, false},
    {"g", MeadeSetCommandKind::SiteLongitude, true, false},
    {"G", MeadeSetCommandKind::UtcOffset, true, false},
    {"L", MeadeSetCommandKind::LocalTime, true, false},
    {"C", MeadeSetCommandKind::LocalDate, true, false},
};

constexpr ExactEntry<MeadeSyncCommandKind> kSyncTable[] = {
    {"M", MeadeSyncCommandKind::SyncToTarget},
};

// Movement: S requires exact match. The directional shortcuts e/w/n/s accept
// any trailing characters but never produce a payload (preserves original
// behavior where "e123" matched SlewEast with empty payload).
constexpr ExactEntry<MeadeMovementCommandKind> kMoveExactTable[] = {
    {"S", MeadeMovementCommandKind::SlewToTarget},
};

constexpr PrefixEntry<MeadeMovementCommandKind> kMovePrefixTable[] = {
    {"AA", MeadeMovementCommandKind::MoveAzAltHome, true, false},
    {"AZ", MeadeMovementCommandKind::MoveAzimuth, true, false},
    {"AL", MeadeMovementCommandKind::MoveAltitude, true, false},
    {"HR", MeadeMovementCommandKind::HomeRa, true, false},
    {"HD", MeadeMovementCommandKind::HomeDec, true, false},
    {"T", MeadeMovementCommandKind::TrackingToggle, true, false},
    {"G", MeadeMovementCommandKind::GuidePulse, true, false},
    {"g", MeadeMovementCommandKind::GuidePulse, true, false},
    {"X", MeadeMovementCommandKind::MoveStepper, true, false},
    {"e", MeadeMovementCommandKind::SlewEast, false, false},
    {"w", MeadeMovementCommandKind::SlewWest, false, false},
    {"n", MeadeMovementCommandKind::SlewNorth, false, false},
    {"s", MeadeMovementCommandKind::SlewSouth, false, false},
};

constexpr ExactEntry<MeadeHomeCommandKind> kHomeTable[] = {
    {"P", MeadeHomeCommandKind::Park},
    {"F", MeadeHomeCommandKind::Home},
    {"U", MeadeHomeCommandKind::Unpark},
    {"Z", MeadeHomeCommandKind::SetAzAltHome},
};

// Quit: empty input is the special StopAll case, handled in the parser body.
constexpr ExactEntry<MeadeQuitCommandKind> kQuitTable[] = {
    {"a", MeadeQuitCommandKind::StopDirectionalAll},
    {"e", MeadeQuitCommandKind::StopEast},
    {"w", MeadeQuitCommandKind::StopWest},
    {"n", MeadeQuitCommandKind::StopNorth},
    {"s", MeadeQuitCommandKind::StopSouth},
    {"q", MeadeQuitCommandKind::QuitControlMode},
};

constexpr ExactEntry<MeadeSlewRateCommandKind> kSlewRateTable[] = {
    {"S", MeadeSlewRateCommandKind::Slew},
    {"M", MeadeSlewRateCommandKind::Find},
    {"C", MeadeSlewRateCommandKind::Center},
    {"G", MeadeSlewRateCommandKind::Guide},
};

// Focus: digits '1'-'4' map to SetSpeedByRate with payload == full input, handled
// specially in the parser body. The remaining entries are prefix matches; only
// M and P carry a payload.
constexpr PrefixEntry<MeadeFocusCommandKind> kFocusTable[] = {
    {"+", MeadeFocusCommandKind::ContinuousIn, false, false},
    {"-", MeadeFocusCommandKind::ContinuousOut, false, false},
    {"M", MeadeFocusCommandKind::MoveBy, true, false},
    {"F", MeadeFocusCommandKind::SetFastestRate, false, false},
    {"S", MeadeFocusCommandKind::SetSlowestRate, false, false},
    {"p", MeadeFocusCommandKind::GetPosition, false, false},
    {"P", MeadeFocusCommandKind::SetPosition, true, false},
    {"B", MeadeFocusCommandKind::GetState, false, false},
    {"Q", MeadeFocusCommandKind::Stop, false, false},
};

// Extra: bare 'F' is invalid; only 'FR' is, so FR must come before any future
// 'F' prefix would (none exists today, but ordering documents the intent).
constexpr PrefixEntry<MeadeExtraCommandKind> kExtraTable[] = {
    {"FR", MeadeExtraCommandKind::FactoryReset, true, false},
    {"D", MeadeExtraCommandKind::DriftAlignment, true, false},
    {"G", MeadeExtraCommandKind::Get, true, false},
    {"S", MeadeExtraCommandKind::Set, true, false},
    {"L", MeadeExtraCommandKind::Level, true, false},
};

// ExtraGet leaf: tries exact match first, then prefix fallback.
// Prefix entries handle the "invalid sub-variant" carve-outs (DL.../H...) and
// the always-prefix entries C... and M... (MountHardwareInfo as default).
constexpr ExactEntry<MeadeExtraLeafCommandKind> kExtraGetExactTable[] = {
    {"R", MeadeExtraLeafCommandKind::GetRaStepsPerDegree},
    {"D", MeadeExtraLeafCommandKind::GetDecStepsPerDegree},
    {"DL", MeadeExtraLeafCommandKind::GetDecLimitBoth},
    {"DLL", MeadeExtraLeafCommandKind::GetDecLimitLowerOnly},
    {"DLU", MeadeExtraLeafCommandKind::GetDecLimitUpperOnly},
    {"DP", MeadeExtraLeafCommandKind::GetDecParking},
    {"S", MeadeExtraLeafCommandKind::GetTrackingSpeedCalibration},
    {"ST", MeadeExtraLeafCommandKind::GetRemainingSafeTime},
    {"T", MeadeExtraLeafCommandKind::GetTrackingSpeed},
    {"B", MeadeExtraLeafCommandKind::GetBacklashSteps},
    {"A", MeadeExtraLeafCommandKind::GetAltStepsPerDegree},
    {"AH", MeadeExtraLeafCommandKind::GetAutoHomingStates},
    {"AA", MeadeExtraLeafCommandKind::GetAzAltPositions},
    {"Z", MeadeExtraLeafCommandKind::GetAzStepsPerDegree},
    {"MS", MeadeExtraLeafCommandKind::GetStepperInfo},
    {"O", MeadeExtraLeafCommandKind::GetLogBuffer},
    {"H", MeadeExtraLeafCommandKind::GetHourAngle},
    {"HR", MeadeExtraLeafCommandKind::GetRaHomingOffset},
    {"HD", MeadeExtraLeafCommandKind::GetDecHomingOffset},
    {"HS", MeadeExtraLeafCommandKind::GetHemisphere},
    {"L", MeadeExtraLeafCommandKind::GetLocalSiderealTime},
    {"N", MeadeExtraLeafCommandKind::GetNetworkStatus},
};

constexpr PrefixEntry<MeadeExtraLeafCommandKind> kExtraGetPrefixTable[] = {
    {"DL", MeadeExtraLeafCommandKind::GetDecLimitInvalidVariant, true, false},
    {"C", MeadeExtraLeafCommandKind::GetTargetCoordinatePositions, true, false},
    {"H", MeadeExtraLeafCommandKind::GetHourAngleInvalidVariant, true, false},
    {"M", MeadeExtraLeafCommandKind::GetMountHardwareInfo, false, false},
};

// ExtraSet leaf: exact match for the two "clear" DL variants, then prefix
// fallback. The "D" prefix requires a non-empty tail so bare "D" stays invalid,
// matching original behavior.
constexpr ExactEntry<MeadeExtraLeafCommandKind> kExtraSetExactTable[] = {
    {"DLl", MeadeExtraLeafCommandKind::SetDecLimitLowerClear},
    {"DLu", MeadeExtraLeafCommandKind::SetDecLimitUpperClear},
};

constexpr PrefixEntry<MeadeExtraLeafCommandKind> kExtraSetPrefixTable[] = {
    {"DLL", MeadeExtraLeafCommandKind::SetDecLimitLowerSet, true, false},
    {"DLU", MeadeExtraLeafCommandKind::SetDecLimitUpperSet, true, false},
    {"DP", MeadeExtraLeafCommandKind::SetDecParking, true, false},
    {"HR", MeadeExtraLeafCommandKind::SetRaHomingOffset, true, false},
    {"HD", MeadeExtraLeafCommandKind::SetDecHomingOffset, true, false},
    {"D", MeadeExtraLeafCommandKind::SetDecStepsPerDegree, true, true},
    {"R", MeadeExtraLeafCommandKind::SetRaStepsPerDegree, true, false},
    {"A", MeadeExtraLeafCommandKind::SetAzStepsPerDegree, true, false},
    {"L", MeadeExtraLeafCommandKind::SetAltStepsPerDegree, true, false},
    {"S", MeadeExtraLeafCommandKind::SetTrackingSpeedCalibration, true, false},
    {"T", MeadeExtraLeafCommandKind::SetTrackingStepperPosition, true, false},
    {"M", MeadeExtraLeafCommandKind::SetManualSlewMode, true, false},
    {"X", MeadeExtraLeafCommandKind::SetRaManualSpeed, true, false},
    {"Y", MeadeExtraLeafCommandKind::SetDecManualSpeed, true, false},
    {"B", MeadeExtraLeafCommandKind::SetBacklashCorrection, true, false},
};

// ExtraLevel leaf: pure prefix table. GR/GC/GT never carry a payload (the
// original accepted "GRabc" as LevelGetReferenceAngles too). G/S act as
// catch-alls for their respective families, and the final "" entry catches
// anything else as LevelUnknownVariant with the full input as payload.
constexpr PrefixEntry<MeadeExtraLeafCommandKind> kExtraLevelTable[] = {
    {"GR", MeadeExtraLeafCommandKind::LevelGetReferenceAngles, false, false},
    {"GC", MeadeExtraLeafCommandKind::LevelGetCurrentAngles, false, false},
    {"GT", MeadeExtraLeafCommandKind::LevelGetTemperature, false, false},
    {"G", MeadeExtraLeafCommandKind::LevelGetInvalidVariant, true, false},
    {"SP", MeadeExtraLeafCommandKind::LevelSetReferencePitch, true, false},
    {"SR", MeadeExtraLeafCommandKind::LevelSetReferenceRoll, true, false},
    {"S", MeadeExtraLeafCommandKind::LevelSetInvalidVariant, true, false},
    {"1", MeadeExtraLeafCommandKind::LevelStartup, false, false},
    {"0", MeadeExtraLeafCommandKind::LevelShutdown, false, false},
    {"", MeadeExtraLeafCommandKind::LevelUnknownVariant, true, false},
};

}  // namespace

// ---------------------------------------------------------------------------
// Top-level parser
// ---------------------------------------------------------------------------
MeadeParseResult parseMeadeCommand(const char *input)
{
    MeadeParseResult result;
    if (input == nullptr || input[0] != ':')
    {
        return result;
    }

    std::string normalized;
    for (const char *cursor = input; *cursor != '\0'; ++cursor)
    {
        if (*cursor != ' ')
        {
            normalized.push_back(*cursor);
        }
    }

    if (normalized.length() < 2)
    {
        return result;
    }

    if (!normalized.empty() && normalized.back() == '#')
    {
        normalized.pop_back();
    }

    if (normalized.length() < 2)
    {
        return result;
    }

    const char family = normalized[1];
    for (std::size_t i = 0; i < (sizeof(kFamilyTable) / sizeof(kFamilyTable[0])); ++i)
    {
        if (kFamilyTable[i].family == family)
        {
            result.valid          = true;
            result.kind           = kFamilyTable[i].kind;
            result.dispatchTarget = kFamilyTable[i].target;
            result.payload        = normalized.substr(2);
            return result;
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// Subcommand parsers
// ---------------------------------------------------------------------------
MeadeGetParseResult parseMeadeGetCommand(const char *input)
{
    MeadeGetParseResult result;
    if (input == nullptr)
    {
        return result;
    }
    MeadeGetCommandKind kind;
    if (lookupExact(kGetTable, input, kind))
    {
        result.valid = true;
        result.kind  = kind;
    }
    return result;
}

MeadeGpsParseResult parseMeadeGpsCommand(const char *input)
{
    MeadeGpsParseResult result;
    if (input == nullptr)
    {
        return result;
    }
    MeadeGpsCommandKind kind;
    const char *tail = nullptr;
    bool capture     = false;
    if (lookupPrefix(kGpsTable, input, kind, tail, capture))
    {
        result.valid = true;
        result.kind  = kind;
        if (capture)
        {
            result.payload = tail;
        }
    }
    return result;
}

MeadeSetParseResult parseMeadeSetCommand(const char *input)
{
    MeadeSetParseResult result;
    if (input == nullptr || input[0] == '\0')
    {
        return result;
    }
    MeadeSetCommandKind kind;
    const char *tail = nullptr;
    bool capture     = false;
    if (lookupPrefix(kSetTable, input, kind, tail, capture))
    {
        result.valid = true;
        result.kind  = kind;
        if (capture)
        {
            result.payload = tail;
        }
    }
    return result;
}

MeadeSyncParseResult parseMeadeSyncCommand(const char *input)
{
    MeadeSyncParseResult result;
    if (input == nullptr)
    {
        return result;
    }
    MeadeSyncCommandKind kind;
    if (lookupExact(kSyncTable, input, kind))
    {
        result.valid = true;
        result.kind  = kind;
    }
    return result;
}

MeadeMovementParseResult parseMeadeMovementCommand(const char *input)
{
    MeadeMovementParseResult result;
    if (input == nullptr || input[0] == '\0')
    {
        return result;
    }
    MeadeMovementCommandKind kind;
    if (lookupExact(kMoveExactTable, input, kind))
    {
        result.valid = true;
        result.kind  = kind;
        return result;
    }
    const char *tail = nullptr;
    bool capture     = false;
    if (lookupPrefix(kMovePrefixTable, input, kind, tail, capture))
    {
        result.valid = true;
        result.kind  = kind;
        if (capture)
        {
            result.payload = tail;
        }
    }
    return result;
}

MeadeHomeParseResult parseMeadeHomeCommand(const char *input)
{
    MeadeHomeParseResult result;
    if (input == nullptr)
    {
        return result;
    }
    MeadeHomeCommandKind kind;
    if (lookupExact(kHomeTable, input, kind))
    {
        result.valid = true;
        result.kind  = kind;
    }
    return result;
}

MeadeQuitParseResult parseMeadeQuitCommand(const char *input)
{
    MeadeQuitParseResult result;
    if (input == nullptr)
    {
        return result;
    }
    if (input[0] == '\0')
    {
        result.valid = true;
        result.kind  = MeadeQuitCommandKind::StopAll;
        return result;
    }
    MeadeQuitCommandKind kind;
    if (lookupExact(kQuitTable, input, kind))
    {
        result.valid = true;
        result.kind  = kind;
    }
    return result;
}

MeadeSlewRateParseResult parseMeadeSlewRateCommand(const char *input)
{
    MeadeSlewRateParseResult result;
    if (input == nullptr)
    {
        return result;
    }
    MeadeSlewRateCommandKind kind;
    if (lookupExact(kSlewRateTable, input, kind))
    {
        result.valid = true;
        result.kind  = kind;
    }
    return result;
}

MeadeFocusParseResult parseMeadeFocusCommand(const char *input)
{
    MeadeFocusParseResult result;
    if (input == nullptr || input[0] == '\0')
    {
        return result;
    }
    if ((input[0] >= '1') && (input[0] <= '4'))
    {
        result.valid   = true;
        result.kind    = MeadeFocusCommandKind::SetSpeedByRate;
        result.payload = input;
        return result;
    }
    MeadeFocusCommandKind kind;
    const char *tail = nullptr;
    bool capture     = false;
    if (lookupPrefix(kFocusTable, input, kind, tail, capture))
    {
        result.valid = true;
        result.kind  = kind;
        if (capture)
        {
            result.payload = tail;
        }
    }
    return result;
}

MeadeExtraParseResult parseMeadeExtraCommand(const char *input)
{
    MeadeExtraParseResult result;
    if (input == nullptr || input[0] == '\0')
    {
        return result;
    }
    MeadeExtraCommandKind kind;
    const char *tail = nullptr;
    bool capture     = false;
    if (lookupPrefix(kExtraTable, input, kind, tail, capture))
    {
        result.valid = true;
        result.kind  = kind;
        if (capture)
        {
            result.payload = tail;
        }
    }
    return result;
}

namespace
{

MeadeExtraLeafParseResult parseMeadeExtraGetLeafCommand(const char *input)
{
    MeadeExtraLeafParseResult result;
    if (input == nullptr || input[0] == '\0')
    {
        return result;
    }
    MeadeExtraLeafCommandKind kind;
    if (lookupExact(kExtraGetExactTable, input, kind))
    {
        result.valid = true;
        result.kind  = kind;
        return result;
    }
    const char *tail = nullptr;
    bool capture     = false;
    if (lookupPrefix(kExtraGetPrefixTable, input, kind, tail, capture))
    {
        result.valid = true;
        result.kind  = kind;
        if (capture)
        {
            result.payload = tail;
        }
    }
    return result;
}

MeadeExtraLeafParseResult parseMeadeExtraSetLeafCommand(const char *input)
{
    MeadeExtraLeafParseResult result;
    if (input == nullptr || input[0] == '\0')
    {
        return result;
    }
    MeadeExtraLeafCommandKind kind;
    if (lookupExact(kExtraSetExactTable, input, kind))
    {
        result.valid = true;
        result.kind  = kind;
        return result;
    }
    const char *tail = nullptr;
    bool capture     = false;
    if (lookupPrefix(kExtraSetPrefixTable, input, kind, tail, capture))
    {
        result.valid = true;
        result.kind  = kind;
        if (capture)
        {
            result.payload = tail;
        }
    }
    return result;
}

MeadeExtraLeafParseResult parseMeadeExtraLevelLeafCommand(const char *input)
{
    MeadeExtraLeafParseResult result;
    if (input == nullptr || input[0] == '\0')
    {
        return result;
    }
    MeadeExtraLeafCommandKind kind;
    const char *tail = nullptr;
    bool capture     = false;
    if (lookupPrefix(kExtraLevelTable, input, kind, tail, capture))
    {
        result.valid = true;
        result.kind  = kind;
        if (capture)
        {
            result.payload = tail;
        }
    }
    return result;
}

}  // namespace

MeadeExtraLeafParseResult parseMeadeExtraLeafCommand(MeadeExtraCommandKind kind, const char *input)
{
    switch (kind)
    {
        case MeadeExtraCommandKind::Get:
            return parseMeadeExtraGetLeafCommand(input);

        case MeadeExtraCommandKind::Set:
            return parseMeadeExtraSetLeafCommand(input);

        case MeadeExtraCommandKind::Level:
            return parseMeadeExtraLevelLeafCommand(input);

        case MeadeExtraCommandKind::Unknown:
        case MeadeExtraCommandKind::DriftAlignment:
        case MeadeExtraCommandKind::FactoryReset:
            return MeadeExtraLeafParseResult();
    }
    return MeadeExtraLeafParseResult();
}

}  // namespace meade
}  // namespace core
}  // namespace oat
