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
#include <stdint.h>
#include <string.h>

namespace oat
{
namespace core
{
namespace meade
{

class MeadeResponse;  // defined in MeadeResponse.hpp; full type needed only at call sites of dispatchGet

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
 * @brief Parse a `:gps...` GPS sub-command.
 * @param input NUL-terminated bytes after the `gps` prefix.
 */
MeadeGpsParseResult parseMeadeGpsCommand(const char *input);

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

// ---------------------------------------------------------------------------
// Get-family dispatch
//
// The Get pipeline is collapsed into a single entry point: `handleMeadeGet`
// parses the sub-command character(s), invokes the matching typed callback
// on `IMeadeGetHandlers`, and serialises the returned value directly into a
// `MeadeResponse`. There is no intermediate kind enum, parse-result, or
// tag-binding indirection for the Get family.
//
// All Meade reply formatting (zero-padding, sign rules, terminator) lives on
// the parser side; handlers return plain typed values.
// ---------------------------------------------------------------------------

/** @brief Right-ascension coordinate (hours/minutes/seconds, all non-negative). */
struct RaCoordinate {
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
};

/** @brief Declination coordinate; `degrees` carries the sign (-180..180). */
struct DecCoordinate {
    int16_t degrees;
    uint8_t minutes;
    uint8_t seconds;
};

/** @brief Site latitude; `degrees` is signed (-90..90). */
struct MeadeLatitude {
    int16_t degrees;
    uint8_t minutes;
};

/** @brief Site longitude; `degrees` is signed (-180..180). */
struct MeadeLongitude {
    int16_t degrees;
    uint8_t minutes;
};

/** @brief Wall-clock time (24h). The parser handles 12h conversion for `:Ga#`. */
struct MeadeLocalTime {
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
};

/** @brief Calendar date. `year` is the full 4-digit year; parser truncates to 2 digits. */
struct MeadeLocalDate {
    uint8_t month;
    uint8_t day;
    uint16_t year;
};

/** @brief Clock-format selector; controls wire bytes for `:Gc#`. */
enum class MeadeClockFormat
{
    Hours12,
    Hours24,
};

/** @brief Tracking rate selector; controls wire bytes for `:GT#`. */
enum class MeadeTrackingRate
{
    Sidereal,
    Lunar,
    Solar,
};

/**
 * @brief Pure callback interface for the Meade `:G...` (Get) command family.
 *
 * Each method returns a typed value or pointer to static storage. Returned
 * `const char *` values must outlive the call (use `static const char[]` or
 * compile-time literals).
 */
class IMeadeGetHandlers
{
  public:
    virtual ~IMeadeGetHandlers() = default;

    virtual const char *onFirmwareVersion() = 0;
    virtual const char *onProductName()     = 0;

    virtual RaCoordinate onCurrentRa() = 0;
    virtual RaCoordinate onTargetRa()  = 0;

    virtual DecCoordinate onCurrentDec() = 0;
    virtual DecCoordinate onTargetDec()  = 0;

    virtual const char *onMountStatus() = 0;

    virtual bool onIsSlewing()  = 0;
    virtual bool onIsTracking() = 0;
    virtual bool onIsGuiding()  = 0;

    virtual MeadeLatitude onSiteLatitude()   = 0;
    virtual MeadeLongitude onSiteLongitude() = 0;

    virtual int onUtcOffset() = 0;

    virtual MeadeLocalTime onLocalTime() = 0;
    virtual MeadeLocalDate onLocalDate() = 0;

    virtual MeadeClockFormat onClockFormat()   = 0;
    virtual MeadeTrackingRate onTrackingRate() = 0;

    /** @param index Site name slot, 1..4. */
    virtual const char *onSiteName(uint8_t index) = 0;
};

/**
 * @brief Parse + dispatch + serialise a Meade Get sub-command in one step.
 *
 * @param suffix The bytes that follow the family `:G` prefix, with the
 *               trailing `#` already stripped (e.g. `"R"`, `"VN"`, `"IS"`).
 * @param handlers Implementation providing the runtime values.
 * @return Framed wire response, or an empty response for unknown sub-commands.
 */
MeadeResponse handleMeadeGet(const char *suffix, IMeadeGetHandlers &handlers);

// ---------------------------------------------------------------------------
// Set-family dispatch
//
// Mirrors the Get pipeline: `handleMeadeSet` parses the sub-command key and
// its payload, invokes the matching typed callback on `IMeadeSetHandlers`,
// and serialises the boolean acknowledgement (`"1"`/`"0"`, or the special
// `:SC#` planetary-data ack) into a `MeadeResponse`.
//
// All wire-format parsing lives in the parser; handlers receive validated
// typed values and report success/failure as a bool. Unrecognised or
// malformed sub-commands produce `"0"` without invoking a handler.
// ---------------------------------------------------------------------------

/**
 * @brief Pure callback interface for the Meade `:S...` (Set) command family.
 *
 * Each method returns a bool indicating whether the mount accepted the
 * value. The parser maps this to the wire bytes `"1"` (success) / `"0"`
 * (failure). `:SC#` uses a dedicated ack format implemented by the parser.
 */
class IMeadeSetHandlers
{
  public:
    virtual ~IMeadeSetHandlers() = default;

    virtual bool onSetTargetDec(DecCoordinate dec) = 0;
    virtual bool onSetTargetRa(RaCoordinate ra)    = 0;

    virtual bool onSetLocalSiderealTime(MeadeLocalTime lst) = 0;
    virtual bool onSetHomePoint()                           = 0;

    /** @param hours 0..23 @param minutes 0..59 */
    virtual bool onSetHourAngle(uint8_t hours, uint8_t minutes) = 0;

    virtual bool onSyncCoordinates(DecCoordinate dec, RaCoordinate ra) = 0;

    virtual bool onSetSiteLatitude(MeadeLatitude lat)   = 0;
    virtual bool onSetSiteLongitude(MeadeLongitude lon) = 0;

    /** @param hours Signed wire value (-12..+14). */
    virtual bool onSetUtcOffset(int hours) = 0;

    virtual bool onSetLocalTime(MeadeLocalTime t) = 0;
    virtual bool onSetLocalDate(MeadeLocalDate d) = 0;
};

/**
 * @brief Parse + dispatch + serialise a Meade Set sub-command in one step.
 *
 * @param suffix The bytes that follow the family `:S` prefix, with the
 *               trailing `#` already stripped (e.g. `"d+12*34:56"`).
 * @param handlers Implementation providing the mount-side side effects.
 * @return Framed wire response, or `"0"` for unknown / malformed input.
 */
MeadeResponse handleMeadeSet(const char *suffix, IMeadeSetHandlers &handlers);

// ---------------------------------------------------------------------------
// Quit-family dispatch
// ---------------------------------------------------------------------------
// Mirrors the Get/Set pipelines for the `:Q...` family. All quit commands
// emit an empty wire response on the protocol; the handler interface only
// reports side effects.

/**
 * @brief Pure callback interface for the Meade `:Q...` (Quit / stop) family.
 *
 * Every callback is a side-effect-only operation; the wire response is
 * always empty regardless of which callback fires. Unknown sub-commands
 * produce an empty response without invoking any handler.
 */
class IMeadeQuitHandlers
{
  public:
    virtual ~IMeadeQuitHandlers() = default;

    /** @brief `:Q#` — stop all axes (slew, tracking, az/alt, focus). */
    virtual void onStopAll() = 0;
    /** @brief `:Qa#` — stop slew on all directional axes; leaves tracking on. */
    virtual void onStopDirectionalAll() = 0;
    /** @brief `:Qe#` — stop eastward slew. */
    virtual void onStopEast() = 0;
    /** @brief `:Qw#` — stop westward slew. */
    virtual void onStopWest() = 0;
    /** @brief `:Qn#` — stop northward slew. */
    virtual void onStopNorth() = 0;
    /** @brief `:Qs#` — stop southward slew. */
    virtual void onStopSouth() = 0;
    /** @brief `:Qq#` — leave serial control mode without stopping motors. */
    virtual void onQuitControlMode() = 0;
};

/**
 * @brief Parse + dispatch a Meade Quit sub-command in one step.
 *
 * @param suffix The bytes that follow the family `:Q` prefix, with the
 *               trailing `#` already stripped. The empty string is the
 *               StopAll variant.
 * @param handlers Implementation providing the mount-side side effects.
 * @return Empty wire response (`""`) for every outcome including unknown.
 */
MeadeResponse handleMeadeQuit(const char *suffix, IMeadeQuitHandlers &handlers);

// ---------------------------------------------------------------------------
// Distance family dispatch (:D...)
// ---------------------------------------------------------------------------
// The `:D#` distance bars command reports motion status as a single wire byte
// ('|' while slewing, ' ' when idle) followed by the standard terminator.
// All sub-commands (including the bare `:D#`) collapse to one boolean query.

/**
 * @brief Pure callback interface for the Meade `:D...` (Distance bars) family.
 */
class IMeadeDistanceHandlers
{
  public:
    virtual ~IMeadeDistanceHandlers() = default;

    /** @brief True while either RA or DEC is actively slewing toward target. */
    virtual bool onIsSlewingRaOrDec() = 0;
};

/**
 * @brief Parse + dispatch a Meade Distance sub-command in one step.
 *
 * @param suffix Bytes following `:D`, trailing `#` already stripped. The
 *               classic command is the empty suffix; any suffix is treated
 *               as the same query (legacy lenient behaviour).
 * @param handlers Implementation providing the slewing-state query.
 * @return Wire bytes: `"|#"` while slewing, `" #"` otherwise.
 */
MeadeResponse handleMeadeDistance(const char *suffix, IMeadeDistanceHandlers &handlers);

}  // namespace meade
}  // namespace core
}  // namespace oat