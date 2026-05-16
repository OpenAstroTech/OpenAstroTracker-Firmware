#pragma once

/**
 * @file MeadeParser.hpp
 * @brief Pure parser for the Meade LX200 command protocol used by
 *        OpenAstroTracker.
 *
 * The parser is allocation-light (the captured payload is held in a small
 * fixed-capacity inline buffer) and has no side effects on the mount: it
 * inspects the raw command bytes, classifies them into a `Meade*CommandKind`
 * enum, and returns a `Meade*ParseResult` describing the dispatch.
 *
 * The framing characters (`:` prefix and `#` terminator) are handled by
 * the caller and are not part of the inputs to these functions.
 *
 * ### Hierarchy
 * - `parseMeadeCommand` classifies the top-level command family.
 * - Per-family parsers (`parseMeadeGetCommand`, ...) decode the family
 *   payload into a fine-grained kind.
 * - `parseMeadeExtraLeafCommand` is dispatched separately because the
 *   `Extra` family has nested sub-commands keyed by
 *   `MeadeExtraCommandKind`.
 */

#include <stddef.h>
#include <string.h>

namespace oat
{
namespace core
{
namespace meade
{

/**
 * @brief Small fixed-capacity owning payload buffer.
 *
 * Replaces `std::string` so the parser is usable on bare AVR builds that
 * ship without libstdc++. Mimics the subset of the `std::string` interface
 * the codebase relies on (`empty()`, `c_str()`, `operator[]`, `length()`).
 */
class MeadePayload
{
  public:
    static constexpr size_t Capacity = 200;

    MeadePayload()
    {
        _data[0] = '\0';
    }

    /** @brief `true` if no payload bytes have been captured. */
    bool empty() const
    {
        return _data[0] == '\0';
    }

    /** @brief NUL-terminated pointer to the captured bytes. */
    const char *c_str() const
    {
        return _data;
    }

    /** @brief Length of the captured bytes, excluding the trailing NUL. */
    size_t length() const
    {
        size_t n = 0;
        while (_data[n] != '\0')
        {
            ++n;
        }
        return n;
    }

    /** @brief Byte access. Behaviour is undefined if `i >= length()`. */
    char operator[](size_t i) const
    {
        return _data[i];
    }

    /** @brief Copy a NUL-terminated source into the buffer (truncating if needed). */
    void assign(const char *s)
    {
        if (s == nullptr)
        {
            _data[0] = '\0';
            return;
        }
        size_t i = 0;
        while ((s[i] != '\0') && (i + 1 < Capacity))
        {
            _data[i] = s[i];
            ++i;
        }
        _data[i] = '\0';
    }

  private:
    char _data[Capacity];
};

/** @brief Top-level Meade command families (first parser pass). */
enum class MeadeCommandKind
{
    Unknown,
    Set,
    Move,
    Get,
    Gps,
    Sync,
    Home,
    Init,
    Quit,
    SlewRate,
    Distance,
    Extra,
    Focus,
};

/**
 * @brief Dispatch label corresponding to a `MeadeCommandKind`, matching the
 * handler-naming used by `MeadeCommandProcessor`.
 */
enum class MeadeCommandDispatchTarget
{
    Unknown,
    SetInfo,
    Movement,
    GetInfo,
    GpsCommands,
    SyncControl,
    Home,
    Init,
    Quit,
    SetSlewRate,
    Distance,
    ExtraCommands,
    FocusCommands,
};

/** @brief Result of `parseMeadeCommand`. */
struct MeadeParseResult {
    /** @brief `true` if the input was recognised. */
    bool valid = false;
    /** @brief Family classification. */
    MeadeCommandKind kind = MeadeCommandKind::Unknown;
    /** @brief Handler dispatch label. */
    MeadeCommandDispatchTarget dispatchTarget = MeadeCommandDispatchTarget::Unknown;
    /** @brief Remaining bytes after the family prefix. */
    MeadePayload payload;
};

/** @brief Sub-commands of the `:X...` extra family. */
enum class MeadeExtraCommandKind
{
    Unknown,
    DriftAlignment,
    Get,
    Set,
    Level,
    FactoryReset,
};

/** @brief Result of `parseMeadeExtraCommand`. */
struct MeadeExtraParseResult {
    /** @brief `true` if the extra sub-command was recognised. */
    bool valid = false;
    /** @brief Extra sub-command classification. */
    MeadeExtraCommandKind kind = MeadeExtraCommandKind::Unknown;
    /** @brief Remaining bytes after the sub-command prefix. */
    MeadePayload payload;
};

/** @brief Leaf sub-commands of the `:X` extra family (one enum spans Get/Set/Level). */
enum class MeadeExtraLeafCommandKind
{
    Unknown,
    GetRaStepsPerDegree,
    GetDecStepsPerDegree,
    GetDecLimitBoth,
    GetDecLimitLowerOnly,
    GetDecLimitUpperOnly,
    GetDecLimitInvalidVariant,
    GetDecParking,
    GetTrackingSpeedCalibration,
    GetRemainingSafeTime,
    GetTrackingSpeed,
    GetBacklashSteps,
    GetAltStepsPerDegree,
    GetAzStepsPerDegree,
    GetAutoHomingStates,
    GetAzAltPositions,
    GetTargetCoordinatePositions,
    GetMountHardwareInfo,
    GetStepperInfo,
    GetLogBuffer,
    GetHourAngle,
    GetHourAngleInvalidVariant,
    GetRaHomingOffset,
    GetDecHomingOffset,
    GetHemisphere,
    GetLocalSiderealTime,
    GetNetworkStatus,
    SetRaStepsPerDegree,
    SetAzStepsPerDegree,
    SetAltStepsPerDegree,
    SetDecStepsPerDegree,
    SetDecLimitLowerSet,
    SetDecLimitUpperSet,
    SetDecLimitLowerClear,
    SetDecLimitUpperClear,
    SetDecParking,
    SetTrackingSpeedCalibration,
    SetTrackingStepperPosition,
    SetManualSlewMode,
    SetRaManualSpeed,
    SetDecManualSpeed,
    SetBacklashCorrection,
    SetRaHomingOffset,
    SetDecHomingOffset,
    LevelGetReferenceAngles,
    LevelGetCurrentAngles,
    LevelGetTemperature,
    LevelGetInvalidVariant,
    LevelSetReferencePitch,
    LevelSetReferenceRoll,
    LevelSetInvalidVariant,
    LevelStartup,
    LevelShutdown,
    LevelUnknownVariant,
};

/** @brief Result of `parseMeadeExtraLeafCommand`. */
struct MeadeExtraLeafParseResult {
    /** @brief `true` if the leaf was recognised. */
    bool valid = false;
    /** @brief Leaf classification. */
    MeadeExtraLeafCommandKind kind = MeadeExtraLeafCommandKind::Unknown;
    /** @brief Remaining bytes after the leaf prefix. */
    MeadePayload payload;
};

/** @brief `:G...` Get sub-commands. */
enum class MeadeGetCommandKind
{
    Unknown,
    FirmwareVersion,
    ProductName,
    TargetRa,
    TargetDec,
    CurrentRa,
    CurrentDec,
    MountStatus,
    IsSlewing,
    IsTracking,
    IsGuiding,
    SiteLatitude,
    SiteLongitude,
    ClockFormat,
    UtcOffset,
    LocalTime12h,
    LocalTime24h,
    LocalDate,
    SiteName1,
    SiteName2,
    SiteName3,
    SiteName4,
    TrackingRate,
};

/** @brief Result of `parseMeadeGetCommand`. */
struct MeadeGetParseResult {
    /** @brief `true` if the get sub-command was recognised. */
    bool valid = false;
    /** @brief Get sub-command classification. */
    MeadeGetCommandKind kind = MeadeGetCommandKind::Unknown;
    /** @brief Remaining bytes after the sub-command prefix. */
    MeadePayload payload;
};

/** @brief `:gps...` GPS sub-commands. */
enum class MeadeGpsCommandKind
{
    Unknown,
    StartAcquisition,
};

/** @brief Result of `parseMeadeGpsCommand`. */
struct MeadeGpsParseResult {
    /** @brief `true` if the GPS sub-command was recognised. */
    bool valid = false;
    /** @brief GPS sub-command classification. */
    MeadeGpsCommandKind kind = MeadeGpsCommandKind::Unknown;
    /** @brief Remaining bytes after the sub-command prefix. */
    MeadePayload payload;
};

/** @brief `:S...` Set sub-commands. */
enum class MeadeSetCommandKind
{
    Unknown,
    TargetDec,
    TargetRa,
    LocalSiderealTime,
    HomePoint,
    HourAngle,
    SyncCoordinates,
    SiteLatitude,
    SiteLongitude,
    UtcOffset,
    LocalTime,
    LocalDate,
};

/** @brief Result of `parseMeadeSetCommand`. */
struct MeadeSetParseResult {
    /** @brief `true` if the set sub-command was recognised. */
    bool valid = false;
    /** @brief Set sub-command classification. */
    MeadeSetCommandKind kind = MeadeSetCommandKind::Unknown;
    /** @brief Remaining bytes after the sub-command prefix. */
    MeadePayload payload;
};

/** @brief `:CM...` Sync sub-commands. */
enum class MeadeSyncCommandKind
{
    Unknown,
    SyncToTarget,
};

/** @brief Result of `parseMeadeSyncCommand`. */
struct MeadeSyncParseResult {
    /** @brief `true` if the sync sub-command was recognised. */
    bool valid = false;
    /** @brief Sync sub-command classification. */
    MeadeSyncCommandKind kind = MeadeSyncCommandKind::Unknown;
    /** @brief Remaining bytes after the sub-command prefix. */
    MeadePayload payload;
};

/** @brief `:M...` Movement sub-commands. */
enum class MeadeMovementCommandKind
{
    Unknown,
    SlewToTarget,
    TrackingToggle,
    GuidePulse,
    MoveAzAltHome,
    MoveAzimuth,
    MoveAltitude,
    SlewEast,
    SlewWest,
    SlewNorth,
    SlewSouth,
    MoveStepper,
    HomeRa,
    HomeDec,
};

/** @brief Result of `parseMeadeMovementCommand`. */
struct MeadeMovementParseResult {
    /** @brief `true` if the movement sub-command was recognised. */
    bool valid = false;
    /** @brief Movement sub-command classification. */
    MeadeMovementCommandKind kind = MeadeMovementCommandKind::Unknown;
    /** @brief Remaining bytes after the sub-command prefix. */
    MeadePayload payload;
};

/** @brief `:h...` Home / park sub-commands. */
enum class MeadeHomeCommandKind
{
    Unknown,
    Park,
    Home,
    Unpark,
    SetAzAltHome,
};

/** @brief Result of `parseMeadeHomeCommand`. */
struct MeadeHomeParseResult {
    /** @brief `true` if the home sub-command was recognised. */
    bool valid = false;
    /** @brief Home sub-command classification. */
    MeadeHomeCommandKind kind = MeadeHomeCommandKind::Unknown;
    /** @brief Remaining bytes after the sub-command prefix. */
    MeadePayload payload;
};

/** @brief `:Q...` Quit / stop sub-commands. */
enum class MeadeQuitCommandKind
{
    Unknown,
    StopAll,
    StopDirectionalAll,
    StopEast,
    StopWest,
    StopNorth,
    StopSouth,
    QuitControlMode,
};

/** @brief Result of `parseMeadeQuitCommand`. */
struct MeadeQuitParseResult {
    /** @brief `true` if the quit sub-command was recognised. */
    bool valid = false;
    /** @brief Quit sub-command classification. */
    MeadeQuitCommandKind kind = MeadeQuitCommandKind::Unknown;
    /** @brief Remaining bytes after the sub-command prefix. */
    MeadePayload payload;
};

/** @brief `:R...` Slew-rate sub-commands. */
enum class MeadeSlewRateCommandKind
{
    Unknown,
    Slew,
    Find,
    Center,
    Guide,
};

/** @brief Result of `parseMeadeSlewRateCommand`. */
struct MeadeSlewRateParseResult {
    /** @brief `true` if the slew-rate sub-command was recognised. */
    bool valid = false;
    /** @brief Slew-rate sub-command classification. */
    MeadeSlewRateCommandKind kind = MeadeSlewRateCommandKind::Unknown;
    /** @brief Remaining bytes after the sub-command prefix. */
    MeadePayload payload;
};

/** @brief `:F...` Focus sub-commands. */
enum class MeadeFocusCommandKind
{
    Unknown,
    ContinuousIn,
    ContinuousOut,
    MoveBy,
    SetSpeedByRate,
    SetFastestRate,
    SetSlowestRate,
    GetPosition,
    SetPosition,
    GetState,
    Stop,
};

/** @brief Result of `parseMeadeFocusCommand`. */
struct MeadeFocusParseResult {
    /** @brief `true` if the focus sub-command was recognised. */
    bool valid = false;
    /** @brief Focus sub-command classification. */
    MeadeFocusCommandKind kind = MeadeFocusCommandKind::Unknown;
    /** @brief Remaining bytes after the sub-command prefix. */
    MeadePayload payload;
};

/**
 * @brief Parse functions consume the bytes between the framing `:` prefix
 * and the `#` terminator (neither is part of the input) and return a result
 * whose `valid` flag indicates whether the command was recognised.
 */

/**
 * @brief Classify a top-level Meade command.
 * @param input NUL-terminated bytes after the leading `:`.
 */
MeadeParseResult parseMeadeCommand(const char *input);

/**
 * @brief Parse a `:G...` Get sub-command.
 * @param input NUL-terminated bytes after the `G` prefix.
 */
MeadeGetParseResult parseMeadeGetCommand(const char *input);

/**
 * @brief Parse a `:gps...` GPS sub-command.
 * @param input NUL-terminated bytes after the `gps` prefix.
 */
MeadeGpsParseResult parseMeadeGpsCommand(const char *input);

/**
 * @brief Parse a `:S...` Set sub-command.
 * @param input NUL-terminated bytes after the `S` prefix.
 */
MeadeSetParseResult parseMeadeSetCommand(const char *input);

/**
 * @brief Parse a `:CM...` Sync sub-command.
 * @param input NUL-terminated bytes after the `CM` prefix.
 */
MeadeSyncParseResult parseMeadeSyncCommand(const char *input);

/**
 * @brief Parse a `:M...` Movement sub-command.
 * @param input NUL-terminated bytes after the `M` prefix.
 */
MeadeMovementParseResult parseMeadeMovementCommand(const char *input);

/**
 * @brief Parse a `:h...` Home / park sub-command.
 * @param input NUL-terminated bytes after the `h` prefix.
 */
MeadeHomeParseResult parseMeadeHomeCommand(const char *input);

/**
 * @brief Parse a `:Q...` Quit / stop sub-command.
 * @param input NUL-terminated bytes after the `Q` prefix.
 */
MeadeQuitParseResult parseMeadeQuitCommand(const char *input);

/**
 * @brief Parse a `:R...` Slew-rate sub-command.
 * @param input NUL-terminated bytes after the `R` prefix.
 */
MeadeSlewRateParseResult parseMeadeSlewRateCommand(const char *input);

/**
 * @brief Parse a `:F...` Focus sub-command.
 * @param input NUL-terminated bytes after the `F` prefix.
 */
MeadeFocusParseResult parseMeadeFocusCommand(const char *input);

/**
 * @brief Parse a `:X...` Extra sub-command at the first level.
 * @param input NUL-terminated bytes after the `X` prefix.
 */
MeadeExtraParseResult parseMeadeExtraCommand(const char *input);

/**
 * @brief Parse a leaf sub-command beneath the `:X...` Extra family.
 * @param kind  Result of a prior call to `parseMeadeExtraCommand`; selects
 *              the appropriate leaf grammar.
 * @param input NUL-terminated bytes after the Extra sub-command prefix.
 */
MeadeExtraLeafParseResult parseMeadeExtraLeafCommand(MeadeExtraCommandKind kind, const char *input);

}  // namespace meade
}  // namespace core
}  // namespace oat