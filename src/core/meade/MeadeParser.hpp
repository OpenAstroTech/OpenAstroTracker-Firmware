#pragma once

/**
 * @file MeadeParser.hpp
 * @brief Pure parser for the Meade LX200 command protocol used by
 *        OpenAstroTracker.
 *
 * The parser is allocation-light (the captured payload is held in a small
 * fixed-capacity inline buffer) and has no side effects on the mount. It
 * classifies the top-level family, retains the remaining payload bytes, and
 * either exposes a small parse result or dispatches the family directly via
 * typed handler interfaces that return a `MeadeResponse`.
 *
 * The framing characters (`:` prefix and `#` terminator) are handled by
 * the caller and are not part of the inputs to these functions.
 *
 * ### Hierarchy
 * - `parseMeadeCommand` classifies the top-level command family.
 * - Most families then dispatch directly via `handleMeade*` entry points.
 * - The `Extra` family also dispatches directly, but still parses nested
 *   leaf keys inside its `handleMeadeExtra` implementation.
 */

#include <stddef.h>
#include <stdint.h>

namespace oat
{
namespace core
{
namespace meade
{

/**
 * @brief Fixed-capacity NUL-terminated Meade buffer value type.
 *
 * Used for both parser payload capture and final wire responses.
 */
class MeadeResponse
{
  public:
    // Must fit the longest reply: :XGM# (board + two stepper descriptors +
    // the full addon list) exceeds 100 bytes on an OAE. At 64 the reply was
    // silently truncated mid-token and the '#' terminator was dropped,
    // breaking every client waiting for it.
    static constexpr size_t Capacity = 160;

    MeadeResponse() : _length(0)
    {
        _data[0] = '\0';
    }

    const char *c_str() const
    {
        return _data;
    }

    size_t length() const
    {
        return _length;
    }

    bool empty() const
    {
        return _length == 0;
    }

    char operator[](size_t i) const
    {
        return _data[i];
    }

    operator const char *() const
    {
        return _data;
    }

    void assign(const char *s)
    {
        if (s == nullptr)
        {
            _data[0] = '\0';
            _length  = 0;
            return;
        }

        size_t i = 0;
        while ((s[i] != '\0') && (i + 1 < Capacity))
        {
            _data[i] = s[i];
            ++i;
        }
        _data[i] = '\0';
        _length  = i;
    }

    char *buffer()
    {
        return _data;
    }

    static constexpr size_t capacity()
    {
        return Capacity;
    }

    void setLength(size_t n)
    {
        _length = n;
    }

    void clear()
    {
        _length  = 0;
        _data[0] = '\0';
    }

  private:
    char _data[Capacity];
    size_t _length;
};

/** @brief Result of `parseMeadeCommand`. */
struct MeadeParseResult {
    /** @brief `true` if the input was recognised. */
    bool valid = false;
    /** @brief The family character (e.g. 'G', 'M', 'X'); '\0' for unrecognised. */
    char family = '\0';
    /** @brief Remaining bytes after the family prefix. */
    MeadeResponse payload;
};

/**
 * @brief Parse functions consume the bytes between the framing `:` prefix
 * and the `#` terminator (neither is part of the input) and return a result
 * whose `valid` flag indicates whether the command was recognised.
 */

/**
 * @brief Classify a top-level Meade command.
 * @param input NUL-terminated bytes after the leading `:`.
 * @param result Output — populated with family, payload, and validity flag.
 * @return `true` if the command was recognised.
 */
bool parseMeadeCommand(const char *input, MeadeParseResult &result);

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
 * @param r Output buffer (pre-constructed) — populated with the wire response.
 * @param suffix The bytes that follow the family `:G` prefix, with the
 *               trailing `#` already stripped (e.g. `"R"`, `"VN"`, `"IS"`).
 * @param handlers Implementation providing the runtime values.
 */
void handleMeadeGet(MeadeResponse &r, const char *suffix, IMeadeGetHandlers &handlers);

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
 * @param r Output buffer (pre-constructed) — populated with the wire response.
 * @param suffix The bytes that follow the family `:S` prefix, with the
 *               trailing `#` already stripped (e.g. `"d+12*34:56"`).
 * @param handlers Implementation providing the mount-side side effects.
 */
void handleMeadeSet(MeadeResponse &r, const char *suffix, IMeadeSetHandlers &handlers);

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
 * @param r Output buffer (pre-constructed) — left empty for every outcome.
 * @param suffix The bytes that follow the family `:Q` prefix, with the
 *               trailing `#` already stripped. The empty string is the
 *               StopAll variant.
 * @param handlers Implementation providing the mount-side side effects.
 */
void handleMeadeQuit(MeadeResponse &r, const char *suffix, IMeadeQuitHandlers &handlers);

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
 * @param r Output buffer (pre-constructed) — populated with the wire response.
 * @param suffix Bytes following `:D`, trailing `#` already stripped. The
 *               classic command is the empty suffix; any suffix is treated
 *               as the same query (legacy lenient behaviour).
 * @param handlers Implementation providing the slewing-state query.
 */
void handleMeadeDistance(MeadeResponse &r, const char *suffix, IMeadeDistanceHandlers &handlers);

// ---- Init family dispatch (:I...) -------------------------------------
//
// The `:I#` command hands the mount UI over to serial control. It emits
// an empty response on the wire; the only behaviour is the side effect.

class IMeadeInitHandlers
{
  public:
    virtual ~IMeadeInitHandlers() = default;

    /** @brief Enter serial-control mode (suppress LCD menu, show banner). */
    virtual void onEnterSerialControl() = 0;
};

/**
 * @brief Parse + dispatch a Meade Init sub-command in one step.
 *
 * @param r Output buffer (pre-constructed) — left empty.
 * @param suffix Bytes following `:I`, trailing `#` already stripped. Any
 *               suffix is accepted (legacy lenient behaviour).
 * @param handlers Implementation performing the mode switch.
 */
void handleMeadeInit(MeadeResponse &r, const char *suffix, IMeadeInitHandlers &handlers);

// ---- SyncControl family dispatch (:C...) ------------------------------
//
// The only currently supported variant is `:CM#` (sync to target). All
// other suffixes elicit `"FAIL#"`. Successful sync emits `"NONE#"`
// (preserved legacy wire byte).

class IMeadeSyncControlHandlers
{
  public:
    virtual ~IMeadeSyncControlHandlers() = default;

    /** @brief Sync current mount position to the previously-set target. */
    virtual void onSyncToTarget() = 0;
};

/**
 * @brief Parse + dispatch a Meade SyncControl sub-command in one step.
 *
 * @param r Output buffer (pre-constructed) — populated with the wire response.
 * @param suffix Bytes following `:C`, trailing `#` already stripped.
 * @param handlers Implementation performing the sync.
 */
void handleMeadeSyncControl(MeadeResponse &r, const char *suffix, IMeadeSyncControlHandlers &handlers);

// ---- Home family dispatch (:h...) -------------------------------------
//
// Supported suffixes:
//   "P" - park       (side effect, empty response)
//   "F" - slew home  (side effect, empty response)
//   "U" - unpark     (side effect, "1" response)
//   "Z" - set Az/Alt home (side effect, "1" response)
// All other suffixes elicit an empty response.

class IMeadeHomeHandlers
{
  public:
    virtual ~IMeadeHomeHandlers() = default;

    /** @brief Park the mount. */
    virtual void onPark() = 0;
    /** @brief Slew to the home position. */
    virtual void onSlewToHome() = 0;
    /** @brief Resume tracking after unparking. */
    virtual void onUnpark() = 0;
    /** @brief Persist the current Az/Alt position as the home reference. */
    virtual void onSetAzAltHome() = 0;
};

/**
 * @brief Parse + dispatch a Meade Home sub-command in one step.
 *
 * @param r Output buffer (pre-constructed) — populated with the wire response.
 * @param suffix Bytes following `:h`, trailing `#` already stripped.
 * @param handlers Implementation of the four home operations.
 */
void handleMeadeHome(MeadeResponse &r, const char *suffix, IMeadeHomeHandlers &handlers);

// ---------------------------------------------------------------------------
// SetSlewRate-family dispatch
//
// `:R...` selects one of four mount slew rates. Each suffix sets a numeric
// rate value (1..4) and produces an empty wire response:
//   "G" -> 1 (guide)
//   "C" -> 2 (center)
//   "M" -> 3 (find)
//   "S" -> 4 (slew)
// All other suffixes elicit an empty response.

class IMeadeSlewRateHandlers
{
  public:
    virtual ~IMeadeSlewRateHandlers() = default;

    /** @brief Apply the given mount slew rate (1..4). */
    virtual void onSetSlewRate(uint8_t rate) = 0;
};

/**
 * @brief Parse + dispatch a Meade SetSlewRate sub-command in one step.
 *
 * @param r Output buffer (pre-constructed) — left empty.
 * @param suffix Bytes following `:R`, trailing `#` already stripped.
 * @param handlers Slew-rate setter callback.
 */
void handleMeadeSetSlewRate(MeadeResponse &r, const char *suffix, IMeadeSlewRateHandlers &handlers);

// ---------------------------------------------------------------------------
// GPSCommands-family dispatch
//
// `:gT<timeout>` initiates a blocking GPS acquisition attempt. The handler is
// responsible for the millis()-based wait loop. Returns SetSuccess("1")/("0").
// All other suffixes return SetSuccess("0") without invoking the handler.

class IMeadeGpsHandlers
{
  public:
    virtual ~IMeadeGpsHandlers() = default;

    /**
     * @brief Attempt a GPS acquisition.
     * @param timeoutPayload Bytes after `T` (NUL-terminated). May be empty
     *                       (the handler decides the default).
     * @return `true` on successful fix within the timeout, `false` otherwise.
     */
    virtual bool onStartGpsAcquisition(const char *timeoutPayload) = 0;
};

/**
 * @brief Parse + dispatch a Meade GPS sub-command in one step.
 *
 * @param r Output buffer (pre-constructed) — populated with the wire response.
 * @param suffix Bytes following `:g`, trailing `#` already stripped.
 * @param handlers GPS acquisition callback.
 */
void handleMeadeGps(MeadeResponse &r, const char *suffix, IMeadeGpsHandlers &handlers);

// ---------------------------------------------------------------------------
// Focus-family dispatch
//
// `:F...` sub-commands control an optional focus stepper. When the hardware is
// not available, handler overrides should be no-ops; `onFocusGetPosition`
// returns 0 and `onFocusGetState` returns false. `onFocusIsAvailable` gates
// the `:FP<n>` SetPosition response: when false the dispatcher emits empty
// wire bytes (preserving legacy behaviour) instead of `SetSuccess("1")`.

class IMeadeFocusHandlers
{
  public:
    virtual ~IMeadeFocusHandlers() = default;

    /** @brief `:F+#` — start continuous focus-in. */
    virtual void onFocusContinuousIn() = 0;
    /** @brief `:F-#` — start continuous focus-out. */
    virtual void onFocusContinuousOut() = 0;
    /** @brief `:FM<steps>#` — move focus by `steps` (signed). */
    virtual void onFocusMoveBy(long steps) = 0;
    /** @brief `:F<1..4>#`, `:FS#` (rate=1), `:FF#` (rate=4) — set focus speed. */
    virtual void onFocusSetSpeedByRate(int rate) = 0;
    /** @brief `:FQ#` — stop focus motion. */
    virtual void onFocusStop() = 0;
    /** @brief `:Fp#` — return current focus stepper position (0 when disabled). */
    virtual long onFocusGetPosition() = 0;
    /** @brief Whether the focus stepper is compiled in / available. */
    virtual bool onFocusIsAvailable() = 0;
    /** @brief `:FP<steps>#` — set the focus stepper position. */
    virtual void onFocusSetPosition(long steps) = 0;
    /** @brief `:FB#` — return `true` if focus is running, `false` otherwise (or when disabled). */
    virtual bool onFocusGetState() = 0;
};

/**
 * @brief Parse + dispatch a Meade `:F...` Focus sub-command in one step.
 *
 * @param r Output buffer (pre-constructed) — populated with the wire response.
 * @param suffix Bytes following `:F`, trailing `#` already stripped.
 * @param handlers Focus stepper callbacks.
 */
void handleMeadeFocus(MeadeResponse &r, const char *suffix, IMeadeFocusHandlers &handlers);

// ---------------------------------------------------------------------------
// Movement-family dispatch
//
// `:M...` sub-commands cover slewing, tracking toggle, guide pulses, direct
// axis nudges, stepper-by-steps movement, and Hall-sensor auto-homing. The
// dispatcher classifies the suffix shape, parses payload bytes, and invokes
// the matching handler. Compile-time hardware guards (e.g. AZ_STEPPER_TYPE,
// USE_HALL_SENSOR_RA_AUTOHOME) live in the override implementations; the
// dispatcher remains hardware-agnostic.

/** @brief Movement axes addressable via `:MX<axis><steps>#`. */
enum class MovementAxis
{
    Ra,
    Dec,
    Azimuth,
    Altitude,
    Focus,
};

/** @brief Guide pulse / direct slew directions. */
enum class MoveDirection
{
    North,
    South,
    East,
    West,
};

class IMeadeMovementHandlers
{
  public:
    virtual ~IMeadeMovementHandlers() = default;

    /** @brief `:MS#` — start slew to current target coordinates. */
    virtual void onStartSlewToTarget() = 0;
    /** @brief `:MT1#` — engage sidereal tracking. */
    virtual void onTrackingOn() = 0;
    /** @brief `:MT0#` — disengage sidereal tracking. */
    virtual void onTrackingOff() = 0;
    /** @brief `:MG<dir><DDDD>#` — fire a guide pulse for `durationMs` milliseconds. */
    virtual void onGuidePulse(MoveDirection dir, int durationMs) = 0;
    /** @brief `:MAA#` — move AZ/ALT axes back to their home positions. */
    virtual void onMoveAzAltHome() = 0;
    /** @brief `:MAZ<f>#` — nudge the azimuth axis by `arcMinutes`. */
    virtual void onMoveAzimuth(float arcMinutes) = 0;
    /** @brief `:MAL<f>#` — nudge the altitude axis by `arcMinutes`. */
    virtual void onMoveAltitude(float arcMinutes) = 0;
    /** @brief `:Me#` — begin continuous slew east. */
    virtual void onSlewEast() = 0;
    /** @brief `:Mw#` — begin continuous slew west. */
    virtual void onSlewWest() = 0;
    /** @brief `:Mn#` — begin continuous slew north. */
    virtual void onSlewNorth() = 0;
    /** @brief `:Ms#` — begin continuous slew south. */
    virtual void onSlewSouth() = 0;
    /** @brief `:MX<axis><steps>#` — move a stepper by raw step count. */
    virtual void onMoveStepper(MovementAxis axis, long steps) = 0;
    /**
     * @brief `:MHR<R|L>[degrees]#` — search for the RA Hall-sensor home.
     * @param direction `+1` for L, `-1` for R.
     * @param distancePayload Bytes after the direction char (may be empty;
     *        the handler decides the default and clamping policy).
     * @return `true` on success, `false` otherwise (incl. when feature is disabled).
     */
    virtual bool onHomeRa(int direction, const char *distancePayload) = 0;
    /**
     * @brief `:MHD<U|D>[degrees]#` — search for the DEC Hall-sensor home.
     * @param direction `+1` for U, `-1` for D.
     * @param distancePayload Bytes after the direction char.
     */
    virtual bool onHomeDec(int direction, const char *distancePayload) = 0;
};

/**
 * @brief Parse + dispatch a Meade `:M...` Movement sub-command in one step.
 *
 * @param r Output buffer (pre-constructed) — populated with the wire response.
 * @param suffix Bytes following `:M`, trailing `#` already stripped.
 * @param handlers Movement callbacks.
 */
void handleMeadeMovement(MeadeResponse &r, const char *suffix, IMeadeMovementHandlers &handlers);

// ---------------------------------------------------------------------------
// Extra-family (`:X...`) dispatch
//
// The Extra family is a two-level tree: `:X<family><leaf-payload>` where
// `<family>` is one of FR (factory reset), D (drift alignment), G (Get-leaves),
// S (Set-leaves), or L (Level-leaves). `handleMeadeExtra` parses both levels
// directly and dispatches every leaf via a small but wide handler interface.
// Compile-time guards for `USE_GYRO_LEVEL`, `WIFI_ENABLED`, etc. live in the
// override impls.

/** @brief Output of `IMeadeExtraHandlers::onGetTargetCoordinatePositions`. */
struct ExtraStepperCoords {
    long raPos  = 0;
    long decPos = 0;
};

/** @brief Output of the Level-family angle queries. */
struct ExtraPitchRoll {
    float pitch = 0.0f;
    float roll  = 0.0f;
};

/** @brief Output of `IMeadeExtraHandlers::onGetHourAngle` / `onGetLocalSiderealTime`. */
struct ExtraHms {
    int hours   = 0;
    int minutes = 0;
    int seconds = 0;
};

/** @brief Output of `IMeadeExtraHandlers::onGetAzAltPositions`. */
struct ExtraAzAltPositions {
    long az  = 0;
    long alt = 0;
};

/** @brief Output of `IMeadeExtraHandlers::onGetDecLimits` (both lo & hi). */
struct ExtraDecLimits {
    float lo = 0.0f;
    float hi = 0.0f;
};

class IMeadeExtraHandlers
{
  public:
    virtual ~IMeadeExtraHandlers() = default;

    /** @brief `:XFR#` — full configuration wipe. */
    virtual void onFactoryReset() = 0;

    /** @brief `:XD<duration>#` — drift-alignment sequence (duration in seconds). */
    virtual void onDriftAlignment(int duration) = 0;

    // ---- Get-leaves -----------------------------------------------------
    virtual float onGetRaStepsPerDegree()                                                    = 0;
    virtual float onGetDecStepsPerDegree()                                                   = 0;
    virtual float onGetAltStepsPerDegree()                                                   = 0;
    virtual float onGetAzStepsPerDegree()                                                    = 0;
    virtual ExtraDecLimits onGetDecLimits()                                                  = 0;
    virtual float onGetTrackingSpeedCalibration()                                            = 0;
    virtual float onGetRemainingSafeTime()                                                   = 0;
    virtual float onGetTrackingSpeed()                                                       = 0;
    virtual int onGetBacklashSteps()                                                         = 0;
    virtual const char *onGetAutoHomingStates()                                              = 0;
    virtual ExtraAzAltPositions onGetAzAltPositions()                                        = 0;
    virtual ExtraStepperCoords onGetTargetCoordinatePositions(float raCoord, float decCoord) = 0;
    virtual const char *onGetStepperInfo()                                                   = 0;
    virtual const char *onGetMountHardwareInfo()                                             = 0;
    virtual const char *onGetLogBuffer()                                                     = 0;
    virtual long onGetRaHomingOffset()                                                       = 0;
    virtual long onGetDecHomingOffset()                                                      = 0;
    virtual bool onGetHemisphere()                                                           = 0;
    virtual ExtraHms onGetHourAngle()                                                        = 0;
    virtual ExtraHms onGetLocalSiderealTime()                                                = 0;
    virtual const char *onGetNetworkStatus()                                                 = 0;

    // ---- Set-leaves -----------------------------------------------------
    virtual void onSetRaStepsPerDegree(float v)  = 0;
    virtual void onSetDecStepsPerDegree(float v) = 0;
    virtual void onSetAzStepsPerDegree(float v)  = 0;
    virtual void onSetAltStepsPerDegree(float v) = 0;
    /** @brief `:XSDLL[<value>]#` — set lower DEC limit; payload may be empty. */
    virtual void onSetDecLimitLower(bool havePayload, float value) = 0;
    virtual void onSetDecLimitUpper(bool havePayload, float value) = 0;
    virtual void onClearDecLimitLower()                            = 0;
    virtual void onClearDecLimitUpper()                            = 0;
    virtual void onSetTrackingSpeedCalibration(float v)            = 0;
    virtual void onSetTrackingStepperPosition(long v)              = 0;
    virtual void onSetManualSlewMode(bool enable)                  = 0;
    virtual void onSetRaManualSpeed(float v)                       = 0;
    virtual void onSetDecManualSpeed(float v)                      = 0;
    virtual void onSetBacklashCorrection(int v)                    = 0;
    virtual void onSetRaHomingOffset(long v)                       = 0;
    virtual void onSetDecHomingOffset(long v)                      = 0;

    // ---- Level-leaves ---------------------------------------------------
    /** @brief Whether USE_GYRO_LEVEL is compiled in. */
    virtual bool onLevelIsAvailable()                  = 0;
    virtual ExtraPitchRoll onLevelGetReferenceAngles() = 0;
    virtual ExtraPitchRoll onLevelGetCurrentAngles()   = 0;
    virtual float onLevelGetTemperature()              = 0;
    virtual void onLevelSetReferencePitch(float v)     = 0;
    virtual void onLevelSetReferenceRoll(float v)      = 0;
    virtual void onLevelStartup()                      = 0;
    virtual void onLevelShutdown()                     = 0;
};

/**
 * @brief Parse + dispatch a Meade `:X...` Extra sub-command in one step.
 *
 * @param r Output buffer (pre-constructed) — populated with the wire response.
 * @param suffix Bytes following `:X`, trailing `#` already stripped.
 * @param handlers Extra-family callbacks.
 */
void handleMeadeExtra(MeadeResponse &r, const char *suffix, IMeadeExtraHandlers &handlers);

// ---------------------------------------------------------------------------
// Aggregate handler interface
//
// Unifies all 12 family-specific handler interfaces into a single type.
// `dispatchMeadeCommand` takes this aggregate and handles the top-level
// family switch internally — callers pass the raw command (after `:`, before
// `#`) and receive a complete `MeadeResponse`.
// ---------------------------------------------------------------------------

class IMeadeHandlers : public IMeadeGetHandlers,
                       public IMeadeSetHandlers,
                       public IMeadeQuitHandlers,
                       public IMeadeDistanceHandlers,
                       public IMeadeInitHandlers,
                       public IMeadeSyncControlHandlers,
                       public IMeadeHomeHandlers,
                       public IMeadeSlewRateHandlers,
                       public IMeadeGpsHandlers,
                       public IMeadeFocusHandlers,
                       public IMeadeMovementHandlers,
                       public IMeadeExtraHandlers
{
  public:
    virtual ~IMeadeHandlers() = default;
};

/**
 * @brief Parse + classify + dispatch a complete Meade command in one call.
 *
 * Accepts the raw bytes between the framing `:` prefix and `#` terminator.
 * Strips whitespace, validates the family character, and dispatches to the
 * correct family handler. Populates `r` with a fully-formed wire response
 * ready for transmission.
 *
 * @param r Output buffer (pre-constructed) — populated with the wire response.
 * @param input NUL-terminated bytes (may include leading `:` and trailing `#`).
 * @param handlers Implementation of all family callback interfaces.
 */
void dispatchMeadeCommand(MeadeResponse &r, const char *input, IMeadeHandlers &handlers);

}  // namespace meade
}  // namespace core
}  // namespace oat