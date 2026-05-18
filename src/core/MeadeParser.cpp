/**
 * @file MeadeParser.cpp
 * @brief Implementation of the Meade LX200 command parser.
 *
 * Parsing is table-driven via `ExactEntry` (full-string match) and
 * `PrefixEntry` (prefix match with optional payload capture). Each
 * `parseMeade*Command` function scans a small static table for its
 * family and falls back to an `Unknown` result otherwise.
 */

#include "core/MeadeParser.hpp"
#include "core/MeadeResponse.hpp"

#include <stddef.h>
#include <string.h>

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

template <typename Kind, size_t N> bool lookupExact(const ExactEntry<Kind> (&table)[N], const char *input, Kind &out)
{
    for (size_t i = 0; i < N; ++i)
    {
        if (strcmp(table[i].key, input) == 0)
        {
            out = table[i].kind;
            return true;
        }
    }
    return false;
}

// First-match-wins prefix lookup. Tables must list longer/more specific keys
// before shorter ones that share a prefix.
template <typename Kind, size_t N>
bool lookupPrefix(const PrefixEntry<Kind> (&table)[N], const char *input, Kind &out, const char *&tail, bool &capturesPayload)
{
    for (size_t i = 0; i < N; ++i)
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

constexpr PrefixEntry<MeadeGpsCommandKind> kGpsTable[] = {
    {"T", MeadeGpsCommandKind::StartAcquisition, true, false},
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

    // Copy the input into a stack buffer with whitespace stripped and the
    // optional trailing `#` removed. Using a fixed-capacity char buffer keeps
    // the parser usable on bare AVR builds that lack libstdc++ (`std::string`).
    char normalized[MeadePayload::Capacity];
    size_t nlen = 0;
    for (const char *cursor = input; *cursor != '\0' && nlen + 1 < sizeof(normalized); ++cursor)
    {
        if (*cursor != ' ')
        {
            normalized[nlen++] = *cursor;
        }
    }
    normalized[nlen] = '\0';

    if (nlen < 2)
    {
        return result;
    }

    if (normalized[nlen - 1] == '#')
    {
        --nlen;
        normalized[nlen] = '\0';
    }

    if (nlen < 2)
    {
        return result;
    }

    const char family = normalized[1];
    for (size_t i = 0; i < (sizeof(kFamilyTable) / sizeof(kFamilyTable[0])); ++i)
    {
        if (kFamilyTable[i].family == family)
        {
            result.valid          = true;
            result.kind           = kFamilyTable[i].kind;
            result.dispatchTarget = kFamilyTable[i].target;
            result.payload.assign(normalized + 2);
            return result;
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// Subcommand parsers
// ---------------------------------------------------------------------------
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
            result.payload.assign(tail);
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
            result.payload.assign(tail);
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
        result.valid = true;
        result.kind  = MeadeFocusCommandKind::SetSpeedByRate;
        result.payload.assign(input);
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
            result.payload.assign(tail);
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
            result.payload.assign(tail);
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
            result.payload.assign(tail);
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
            result.payload.assign(tail);
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
            result.payload.assign(tail);
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

// ---------------------------------------------------------------------------
// Get-family dispatch
//
// Single entry point: parse the suffix, call the typed handler, serialise the
// result. No intermediate enum, lookup table, or tag binding.
// ---------------------------------------------------------------------------

namespace
{

void writeChar(MeadeResponse &r, char c)
{
    const size_t n = r.length();
    if (n + 1 >= r.capacity())
    {
        return;
    }
    r.buffer()[n]     = c;
    r.buffer()[n + 1] = '\0';
    r.setLength(n + 1);
}

void writeText(MeadeResponse &r, const char *s)
{
    if (!s)
    {
        return;
    }
    while (*s)
    {
        writeChar(r, *s++);
    }
}

void writeTerminator(MeadeResponse &r)
{
    writeChar(r, '#');
}

void writeUnsignedPadded(MeadeResponse &r, unsigned value, int width)
{
    char buf[12];
    int n = 0;
    if (value == 0)
    {
        buf[n++] = '0';
    }
    else
    {
        while (value > 0 && n < 11)
        {
            buf[n++] = static_cast<char>('0' + (value % 10));
            value /= 10;
        }
    }
    while (n < width && n < 11)
    {
        buf[n++] = '0';
    }
    while (n > 0)
    {
        writeChar(r, buf[--n]);
    }
}

void writeSignedPadded(MeadeResponse &r, int value, int digits)
{
    writeChar(r, value < 0 ? '-' : '+');
    if (value < 0)
    {
        value = -value;
    }
    writeUnsignedPadded(r, static_cast<unsigned>(value), digits);
}

void writeBool01(MeadeResponse &r, bool b)
{
    writeChar(r, b ? '1' : '0');
    writeTerminator(r);
}

void writeCString(MeadeResponse &r, const char *s)
{
    writeText(r, s);
    writeTerminator(r);
}

void writeRa(MeadeResponse &r, const RaCoordinate &ra)
{
    writeUnsignedPadded(r, ra.hours, 2);
    writeChar(r, ':');
    writeUnsignedPadded(r, ra.minutes, 2);
    writeChar(r, ':');
    writeUnsignedPadded(r, ra.seconds, 2);
    writeTerminator(r);
}

void writeDec(MeadeResponse &r, const DecCoordinate &d)
{
    int deg = d.degrees;
    writeChar(r, deg < 0 ? '-' : '+');
    if (deg < 0)
    {
        deg = -deg;
    }
    writeUnsignedPadded(r, static_cast<unsigned>(deg), 2);
    writeChar(r, '*');
    writeUnsignedPadded(r, d.minutes, 2);
    writeChar(r, '\'');
    writeUnsignedPadded(r, d.seconds, 2);
    writeTerminator(r);
}

void writeLatitude(MeadeResponse &r, const MeadeLatitude &l)
{
    int deg = l.degrees;
    writeChar(r, deg < 0 ? '-' : '+');
    if (deg < 0)
    {
        deg = -deg;
    }
    writeUnsignedPadded(r, static_cast<unsigned>(deg), 2);
    writeChar(r, '*');
    writeUnsignedPadded(r, l.minutes, 2);
    writeTerminator(r);
}

void writeLongitude(MeadeResponse &r, const MeadeLongitude &l)
{
    int deg = l.degrees;
    writeChar(r, deg < 0 ? '-' : '+');
    if (deg < 0)
    {
        deg = -deg;
    }
    writeUnsignedPadded(r, static_cast<unsigned>(deg), 3);
    writeChar(r, '*');
    writeUnsignedPadded(r, l.minutes, 2);
    writeTerminator(r);
}

void writeTime24h(MeadeResponse &r, const MeadeLocalTime &t)
{
    writeUnsignedPadded(r, t.hours, 2);
    writeChar(r, ':');
    writeUnsignedPadded(r, t.minutes, 2);
    writeChar(r, ':');
    writeUnsignedPadded(r, t.seconds, 2);
    writeTerminator(r);
}

void writeTime12h(MeadeResponse &r, const MeadeLocalTime &t)
{
    // The :Ga# Meade command returns 12h wall-clock time. Conversion: 0 -> 12,
    // 13..23 -> 1..11 (PM); 1..12 unchanged. The wire format omits AM/PM markers.
    uint8_t h = t.hours;
    if (h == 0)
    {
        h = 12;
    }
    else if (h > 12)
    {
        h = static_cast<uint8_t>(h - 12);
    }
    MeadeLocalTime t12 = {h, t.minutes, t.seconds};
    writeTime24h(r, t12);
}

void writeLocalDate(MeadeResponse &r, const MeadeLocalDate &d)
{
    writeUnsignedPadded(r, d.month, 2);
    writeChar(r, '/');
    writeUnsignedPadded(r, d.day, 2);
    writeChar(r, '/');
    writeUnsignedPadded(r, static_cast<unsigned>(d.year % 100), 2);
    writeTerminator(r);
}

void writeUtcOffset(MeadeResponse &r, int hours)
{
    writeSignedPadded(r, hours, 2);
    writeTerminator(r);
}

void writeClockFormat(MeadeResponse &r, MeadeClockFormat f)
{
    writeText(r, f == MeadeClockFormat::Hours24 ? "24" : "12");
    writeTerminator(r);
}

void writeTrackingRate(MeadeResponse &r, MeadeTrackingRate t)
{
    const char *s = "60.0";
    switch (t)
    {
        case MeadeTrackingRate::Sidereal:
            s = "60.0";
            break;
        case MeadeTrackingRate::Lunar:
            s = "57.9";
            break;
        case MeadeTrackingRate::Solar:
            s = "60.1";
            break;
    }
    writeText(r, s);
    writeTerminator(r);
}

}  // namespace

MeadeResponse handleMeadeGet(const char *s, IMeadeGetHandlers &h)
{
    MeadeResponse r;
    if (!s || s[0] == '\0')
    {
        return r;
    }

    // Two-character commands.
    if (s[1] != '\0' && s[2] == '\0')
    {
        if (s[0] == 'V')
        {
            switch (s[1])
            {
                case 'N':
                    writeCString(r, h.onFirmwareVersion());
                    return r;
                case 'P':
                    writeCString(r, h.onProductName());
                    return r;
                default:
                    return r;
            }
        }
        if (s[0] == 'I')
        {
            switch (s[1])
            {
                case 'S':
                    writeBool01(r, h.onIsSlewing());
                    return r;
                case 'T':
                    writeBool01(r, h.onIsTracking());
                    return r;
                case 'G':
                    writeBool01(r, h.onIsGuiding());
                    return r;
                default:
                    return r;
            }
        }
        return r;
    }

    // Single-character commands.
    if (s[1] != '\0')
    {
        return r;
    }

    switch (s[0])
    {
        case 'R':
            writeRa(r, h.onCurrentRa());
            return r;
        case 'r':
            writeRa(r, h.onTargetRa());
            return r;
        case 'D':
            writeDec(r, h.onCurrentDec());
            return r;
        case 'd':
            writeDec(r, h.onTargetDec());
            return r;
        case 'X':
            writeCString(r, h.onMountStatus());
            return r;
        case 't':
            writeLatitude(r, h.onSiteLatitude());
            return r;
        case 'g':
            writeLongitude(r, h.onSiteLongitude());
            return r;
        case 'G':
            writeUtcOffset(r, h.onUtcOffset());
            return r;
        case 'a':
            writeTime12h(r, h.onLocalTime());
            return r;
        case 'L':
            writeTime24h(r, h.onLocalTime());
            return r;
        case 'C':
            writeLocalDate(r, h.onLocalDate());
            return r;
        case 'c':
            writeClockFormat(r, h.onClockFormat());
            return r;
        case 'T':
            writeTrackingRate(r, h.onTrackingRate());
            return r;
        case 'M':
            writeCString(r, h.onSiteName(1));
            return r;
        case 'N':
            writeCString(r, h.onSiteName(2));
            return r;
        case 'O':
            writeCString(r, h.onSiteName(3));
            return r;
        case 'P':
            writeCString(r, h.onSiteName(4));
            return r;
        default:
            return r;
    }
}

// ---------------------------------------------------------------------------
// Set-family dispatch
// ---------------------------------------------------------------------------

namespace
{

inline bool isDecimalDigit(char c)
{
    return c >= '0' && c <= '9';
}

template <int N> bool readFixedDigits(const char *p, unsigned &out)
{
    unsigned v = 0;
    for (int i = 0; i < N; ++i)
    {
        if (!isDecimalDigit(p[i]))
        {
            return false;
        }
        v = v * 10 + static_cast<unsigned>(p[i] - '0');
    }
    out = v;
    return true;
}

// Parse "+DD" / "-DD" into a signed int.
bool readSignedFixed2(const char *p, int &out)
{
    if ((p[0] != '+' && p[0] != '-') || !isDecimalDigit(p[1]) || !isDecimalDigit(p[2]))
    {
        return false;
    }
    int v = (p[1] - '0') * 10 + (p[2] - '0');
    out   = (p[0] == '-') ? -v : v;
    return true;
}

// Parse "+DDD" / "-DDD" into a signed int.
bool readSignedFixed3(const char *p, int &out)
{
    if ((p[0] != '+' && p[0] != '-') || !isDecimalDigit(p[1]) || !isDecimalDigit(p[2]) || !isDecimalDigit(p[3]))
    {
        return false;
    }
    int v = (p[1] - '0') * 100 + (p[2] - '0') * 10 + (p[3] - '0');
    out   = (p[0] == '-') ? -v : v;
    return true;
}

// Format: "[+-]DD<sep>MM:SS" where sep in {'*', ':'}. 9 chars.
bool readDecCoordinate(const char *p, size_t len, DecCoordinate &out)
{
    if (len != 9)
    {
        return false;
    }
    int deg;
    unsigned mm, ss;
    if (!readSignedFixed2(p, deg))
    {
        return false;
    }
    if (p[3] != '*' && p[3] != ':')
    {
        return false;
    }
    if (!readFixedDigits<2>(p + 4, mm))
    {
        return false;
    }
    if (p[6] != ':')
    {
        return false;
    }
    if (!readFixedDigits<2>(p + 7, ss))
    {
        return false;
    }
    out.degrees = static_cast<int16_t>(deg);
    out.minutes = static_cast<uint8_t>(mm);
    out.seconds = static_cast<uint8_t>(ss);
    return true;
}

// Format: "HH:MM:SS". 8 chars.
bool readRaCoordinate(const char *p, size_t len, RaCoordinate &out)
{
    if (len != 8)
    {
        return false;
    }
    unsigned hh, mm, ss;
    if (!readFixedDigits<2>(p, hh) || p[2] != ':' || !readFixedDigits<2>(p + 3, mm) || p[5] != ':' || !readFixedDigits<2>(p + 6, ss))
    {
        return false;
    }
    out.hours   = static_cast<uint8_t>(hh);
    out.minutes = static_cast<uint8_t>(mm);
    out.seconds = static_cast<uint8_t>(ss);
    return true;
}

// Format: "[+-]DD<sep>MM" where sep in {'*', ':'}. 6 chars.
bool readLatitude(const char *p, size_t len, MeadeLatitude &out)
{
    if (len != 6)
    {
        return false;
    }
    int deg;
    unsigned mm;
    if (!readSignedFixed2(p, deg) || (p[3] != '*' && p[3] != ':') || !readFixedDigits<2>(p + 4, mm))
    {
        return false;
    }
    out.degrees = static_cast<int16_t>(deg);
    out.minutes = static_cast<uint8_t>(mm);
    return true;
}

// Format: "[+-]DDD<sep>MM" where sep in {'*', ':'}. 7 chars.
bool readLongitude(const char *p, size_t len, MeadeLongitude &out)
{
    if (len != 7)
    {
        return false;
    }
    int deg;
    unsigned mm;
    if (!readSignedFixed3(p, deg) || (p[4] != '*' && p[4] != ':') || !readFixedDigits<2>(p + 5, mm))
    {
        return false;
    }
    out.degrees = static_cast<int16_t>(deg);
    out.minutes = static_cast<uint8_t>(mm);
    return true;
}

// Set ack: "1" on success, "0" on failure. No framing terminator.
void writeSetAck(MeadeResponse &r, bool ok)
{
    writeChar(r, ok ? '1' : '0');
}

// :SC# success ack: "1Updating Planetary Data#<30 spaces>#". "0" on failure.
void writeSetLocalDateAck(MeadeResponse &r, bool ok)
{
    if (!ok)
    {
        writeChar(r, '0');
        return;
    }
    writeText(r, "1Updating Planetary Data");
    writeTerminator(r);
    for (int i = 0; i < 30; ++i)
    {
        writeChar(r, ' ');
    }
    writeTerminator(r);
}

}  // namespace

MeadeResponse handleMeadeSet(const char *s, IMeadeSetHandlers &h)
{
    MeadeResponse r;
    if (!s || s[0] == '\0')
    {
        writeChar(r, '0');
        return r;
    }

    const size_t len = strlen(s);

    switch (s[0])
    {
        case 'd':
            {
                DecCoordinate dec;
                if (!readDecCoordinate(s + 1, len - 1, dec))
                {
                    writeChar(r, '0');
                    return r;
                }
                writeSetAck(r, h.onSetTargetDec(dec));
                return r;
            }

        case 'r':
            {
                RaCoordinate ra;
                if (!readRaCoordinate(s + 1, len - 1, ra))
                {
                    writeChar(r, '0');
                    return r;
                }
                writeSetAck(r, h.onSetTargetRa(ra));
                return r;
            }

        case 'H':
            if (len >= 2 && s[1] == 'L')
            {
                // HLhhmmss (8 chars) or HLhhmm (6 chars) — no separators on the wire.
                unsigned hh = 0, mm = 0, ss = 0;
                bool ok = false;
                if (len == 8)
                {
                    ok = readFixedDigits<2>(s + 2, hh) && readFixedDigits<2>(s + 4, mm) && readFixedDigits<2>(s + 6, ss);
                }
                else if (len == 6)
                {
                    ok = readFixedDigits<2>(s + 2, hh) && readFixedDigits<2>(s + 4, mm);
                }
                if (!ok)
                {
                    writeChar(r, '0');
                    return r;
                }
                MeadeLocalTime t {static_cast<uint8_t>(hh), static_cast<uint8_t>(mm), static_cast<uint8_t>(ss)};
                writeSetAck(r, h.onSetLocalSiderealTime(t));
                return r;
            }
            if (len == 2 && s[1] == 'P')
            {
                writeSetAck(r, h.onSetHomePoint());
                return r;
            }
            // Bare H = HourAngle: H<hh><sep><mm>. Total 6 chars. Separator at s[3] is not validated
            // (legacy behaviour: any single char accepted).
            if (len == 6)
            {
                unsigned hh, mm;
                if (!readFixedDigits<2>(s + 1, hh) || !readFixedDigits<2>(s + 4, mm))
                {
                    writeChar(r, '0');
                    return r;
                }
                writeSetAck(r, h.onSetHourAngle(static_cast<uint8_t>(hh), static_cast<uint8_t>(mm)));
                return r;
            }
            writeChar(r, '0');
            return r;

        case 'Y':
            {
                // Y<dec(9)>.<ra(8)>  total 19 chars including 'Y'.
                if (len != 19 || s[10] != '.')
                {
                    writeChar(r, '0');
                    return r;
                }
                DecCoordinate dec;
                RaCoordinate ra;
                if (!readDecCoordinate(s + 1, 9, dec) || !readRaCoordinate(s + 11, 8, ra))
                {
                    writeChar(r, '0');
                    return r;
                }
                writeSetAck(r, h.onSyncCoordinates(dec, ra));
                return r;
            }

        case 't':
            {
                MeadeLatitude lat;
                if (!readLatitude(s + 1, len - 1, lat))
                {
                    writeChar(r, '0');
                    return r;
                }
                writeSetAck(r, h.onSetSiteLatitude(lat));
                return r;
            }

        case 'g':
            {
                MeadeLongitude lon;
                if (!readLongitude(s + 1, len - 1, lon))
                {
                    writeChar(r, '0');
                    return r;
                }
                writeSetAck(r, h.onSetSiteLongitude(lon));
                return r;
            }

        case 'G':
            {
                // G<sign><DD>  4 chars total.
                if (len != 4)
                {
                    writeChar(r, '0');
                    return r;
                }
                int hours;
                if (!readSignedFixed2(s + 1, hours))
                {
                    writeChar(r, '0');
                    return r;
                }
                writeSetAck(r, h.onSetUtcOffset(hours));
                return r;
            }

        case 'L':
            {
                // L<HH>:<MM>:<SS>  9 chars total.
                if (len != 9)
                {
                    writeChar(r, '0');
                    return r;
                }
                unsigned hh, mm, ss;
                if (!readFixedDigits<2>(s + 1, hh) || s[3] != ':' || !readFixedDigits<2>(s + 4, mm) || s[6] != ':'
                    || !readFixedDigits<2>(s + 7, ss))
                {
                    writeChar(r, '0');
                    return r;
                }
                MeadeLocalTime t {static_cast<uint8_t>(hh), static_cast<uint8_t>(mm), static_cast<uint8_t>(ss)};
                writeSetAck(r, h.onSetLocalTime(t));
                return r;
            }

        case 'C':
            {
                // C<MM>/<DD>/<YY>  9 chars total.
                if (len != 9)
                {
                    writeChar(r, '0');
                    return r;
                }
                unsigned mo, dd, yy;
                if (!readFixedDigits<2>(s + 1, mo) || s[3] != '/' || !readFixedDigits<2>(s + 4, dd) || s[6] != '/'
                    || !readFixedDigits<2>(s + 7, yy))
                {
                    writeChar(r, '0');
                    return r;
                }
                MeadeLocalDate d;
                d.month = static_cast<uint8_t>(mo);
                d.day   = static_cast<uint8_t>(dd);
                d.year  = static_cast<uint16_t>(2000 + yy);
                writeSetLocalDateAck(r, h.onSetLocalDate(d));
                return r;
            }

        default:
            writeChar(r, '0');
            return r;
    }
}

// ---------------------------------------------------------------------------
// Quit-family dispatcher
// ---------------------------------------------------------------------------

MeadeResponse handleMeadeQuit(const char *suffix, IMeadeQuitHandlers &h)
{
    MeadeResponse r;
    if (suffix == nullptr)
    {
        return r;
    }

    // Empty suffix == :Q# == StopAll.
    if (suffix[0] == '\0')
    {
        h.onStopAll();
        return r;
    }

    // All remaining variants are a single character.
    if (suffix[1] != '\0')
    {
        return r;
    }

    switch (suffix[0])
    {
        case 'a':
            h.onStopDirectionalAll();
            break;
        case 'e':
            h.onStopEast();
            break;
        case 'w':
            h.onStopWest();
            break;
        case 'n':
            h.onStopNorth();
            break;
        case 's':
            h.onStopSouth();
            break;
        case 'q':
            h.onQuitControlMode();
            break;
        default:
            break;
    }
    return r;
}

// ---------------------------------------------------------------------------
// Distance family
// ---------------------------------------------------------------------------

MeadeResponse handleMeadeDistance(const char *, IMeadeDistanceHandlers &h)
{
    MeadeResponse r;
    writeChar(r, h.onIsSlewingRaOrDec() ? '|' : ' ');
    writeTerminator(r);
    return r;
}

}  // namespace meade
}  // namespace core
}  // namespace oat
