#pragma once

/**
 * @file MeadeResponse.hpp
 * @brief Type-safe Meade response API.
 *
 * The orchestrator (`MeadeCommandProcessor`) builds a response by selecting a
 * command kind at compile time; the trait layer (`GetResponse<K>` etc.) maps
 * that kind to a response *shape* (tag) and ultimately to a `makeResponse`
 * overload that owns the wire formatting for that shape. Passing the wrong
 * argument types is rejected at compile time; forgetting to specialise a
 * trait surfaces as an incomplete-type error at the call site.
 *
 * ### Layers
 * - **Tags** (`response::tag::*`): zero-size types identifying a wire shape.
 * - **Factories** (`makeResponse(tag::X, args...)`): exactly one overload per
 *   shape; owns the printf/format logic for that shape.
 * - **Traits** (`GetResponse<K>` etc.): compile-time kind -> tag mapping,
 *   producing a `make(args...)` shim that forwards to `makeResponse`.
 * - **Entry points** (`respondGet<K>(args...)` etc.): user-facing helpers
 *   that bind a kind to its trait.
 *
 * Framing (the trailing `#` byte that terminates each Meade reply) is
 * centralised in `MeadeResponse.cpp` via `appendTerminator`; per-shape
 * formatters only write payload bytes.
 */

#include <cstddef>
#include <cstdint>
#include <utility>

#include "core/MeadeParser.hpp"

namespace oat
{
namespace core
{
namespace meade
{

/**
 * @brief Fixed-capacity NUL-terminated Meade reply value type.
 *
 * Implicitly convertible to `const char *` so existing call sites that
 * return `const char *` can be migrated incrementally. The buffer is
 * embedded (no heap), making the type safe to use from interrupt-adjacent
 * contexts.
 */
class MeadeResponse
{
  public:
    /**
     * @brief Maximum payload length, including the framing terminator and
     * the trailing NUL byte.
     */
    static constexpr std::size_t Capacity = 200;

    /**
     * @brief Construct an empty (zero-length) response.
     */
    MeadeResponse();

    /**
     * @return NUL-terminated pointer to the reply bytes.
     */
    const char *c_str() const
    {
        return _data;
    }
    /**
     * @return Length of the reply in bytes, excluding the trailing NUL.
     */
    std::size_t length() const
    {
        return _length;
    }
    /**
     * @return `true` if no bytes have been written.
     */
    bool empty() const
    {
        return _length == 0;
    }

    /**
     * @brief Implicit conversion to `const char *` for legacy call sites.
     */
    operator const char *() const
    {
        return _data;
    }

    /**
     * @brief Internal mutator.
     *
     * Used only by `makeResponse` overloads in `MeadeResponse.cpp`. Not
     * intended for direct caller use.
     */
    char *buffer()
    {
        return _data;
    }
    /**
     * @brief Buffer capacity.
     *
     * Used only by `makeResponse` overloads in `MeadeResponse.cpp`. Not
     * intended for direct caller use.
     */
    static constexpr std::size_t capacity()
    {
        return Capacity;
    }
    /**
     * @brief Internal mutator.
     *
     * Used only by `makeResponse` overloads in `MeadeResponse.cpp`. Not
     * intended for direct caller use.
     */
    void setLength(std::size_t n)
    {
        _length = n;
    }

  private:
    char _data[Capacity];
    std::size_t _length;
};

namespace response
{

/**
 * @brief Zero-size tag types identifying response wire shapes.
 *
 * One tag exists per distinct shape (wire format), not per command kind.
 * Several command kinds map to the same tag when they share a shape.
 * Each tag has exactly one corresponding `makeResponse` overload.
 */
namespace tag
{
/** @brief Empty payload (no bytes, no terminator). */
struct Empty {
};
/** @brief Verbatim, unframed text: `const char *text` -> `text` as-is. */
struct Literal {
};
/** @brief Framed text: `const char *body` -> `"<body>" + #`. */
struct Text {
};
/** @brief Framed boolean: `bool flag` -> `"0#"` or `"1#"`. */
struct Boolean {
};
/** @brief Unframed Set-command ack: `bool ok` -> `"0"` or `"1"` (no `#`). */
struct SetSuccess {
};
/** @brief Framed float with caller-chosen precision: `float, int prec` -> `"<num>#"`. */
struct NumericFloat {
};
/** @brief Fixed `"24#"` reply for clock-format query. */
struct ClockFormat24 {
};
/** @brief Fixed `"60.0#"` reply for tracking-rate query. */
struct TrackingRate {
};
/** @brief Signed two-digit hours: `int hours` -> `"+HH#"` or `"-HH#"`. */
struct UtcOffset {
};
/** @brief Calendar date: `int m, int d, int y` -> `"MM/DD/YY#"`. */
struct LocalDate {
};
/** @brief Site name slot: `int slot` -> `"OAT<slot>#"`. */
struct SiteNameSlot {
};
/** @brief Right ascension: `int h, m, s` (or preformatted) -> `"HH:MM:SS#"`. */
struct RaCoordinate {
};
/** @brief Declination: `char sign, int d, m, s` (or preformatted) -> `"sDD*MM'SS#"`. */
struct DecCoordinate {
};
/** @brief Site latitude: `char sign, int d, m` (or preformatted) -> `"sDD*MM#"`. */
struct SiteLatitude {
};
/** @brief Site longitude: `char sign, int d, m` (or preformatted) -> `"sDDD*MM#"`. */
struct SiteLongitude {
};
/** @brief Local time: `int h, m, s` (or preformatted) -> `"HH:MM:SS#"`. */
struct LocalTime {
};
/** @brief Declination limits: `float lo, hi` -> `"<lo>|<hi>#"` (1 dp). */
struct DecLimitsPair {
};
/** @brief Pair of angles with 2-decimal precision: `"<a>,<b>#"`. */
struct AnglePair {
};
/** @brief Pair of angles with 4-decimal precision: `"<a>,<b>#"`. */
struct AnglePair4 {
};
/** @brief Hemisphere flag: `bool north` -> `"N#"` or `"S#"`. */
struct Hemisphere {
};
/** @brief Date-set ack: on success two framed records, on failure `"0"`. */
struct SetLocalDateAck {
};
/** @brief Unknown Level sub-command echo (unframed): `"Unknown Level command: X<echo>"`. */
struct LevelUnknown {
};
/** @brief Framed decimal integer: `int n` -> `"<n>#"`. */
struct Int {
};
/** @brief Framed decimal long: `long n` -> `"<n>#"`. */
struct Long {
};
/** @brief Pipe-separated longs: `long a, b` -> `"<a>|<b>#"`. */
struct LongPairPipe {
};
/** @brief Compact h/m/s without separators: `"HHMMSS#"`. */
struct CompactHms {
};
}  // namespace tag

/**
 * @brief Factory: exactly one overload exists per response shape.
 *
 * Each owns the wire formatting for that shape, writing payload bytes
 * followed (where applicable) by the framing terminator.
 */
MeadeResponse makeResponse(tag::Empty);
MeadeResponse makeResponse(tag::Literal, const char *text);
MeadeResponse makeResponse(tag::Text, const char *body);
MeadeResponse makeResponse(tag::Boolean, bool flag);
MeadeResponse makeResponse(tag::SetSuccess, bool ok);
MeadeResponse makeResponse(tag::NumericFloat, float value, int precision);
MeadeResponse makeResponse(tag::ClockFormat24);
MeadeResponse makeResponse(tag::TrackingRate);
MeadeResponse makeResponse(tag::UtcOffset, int hours);
MeadeResponse makeResponse(tag::LocalDate, int month, int day, int year);
MeadeResponse makeResponse(tag::SiteNameSlot, int slot);
MeadeResponse makeResponse(tag::RaCoordinate, int hours, int minutes, int seconds);
MeadeResponse makeResponse(tag::RaCoordinate, const char *preformatted);
MeadeResponse makeResponse(tag::DecCoordinate, char sign, int degrees, int minutes, int seconds);
MeadeResponse makeResponse(tag::DecCoordinate, const char *preformatted);
MeadeResponse makeResponse(tag::SiteLatitude, char sign, int degrees, int minutes);
MeadeResponse makeResponse(tag::SiteLatitude, const char *preformatted);
MeadeResponse makeResponse(tag::SiteLongitude, char sign, int degrees, int minutes);
MeadeResponse makeResponse(tag::SiteLongitude, const char *preformatted);
MeadeResponse makeResponse(tag::LocalTime, int hours, int minutes, int seconds);
MeadeResponse makeResponse(tag::LocalTime, const char *preformatted);
MeadeResponse makeResponse(tag::DecLimitsPair, float lo, float hi);
MeadeResponse makeResponse(tag::AnglePair, float a, float b);
MeadeResponse makeResponse(tag::AnglePair4, float a, float b);
MeadeResponse makeResponse(tag::Hemisphere, bool north);
MeadeResponse makeResponse(tag::SetLocalDateAck, bool ok);
MeadeResponse makeResponse(tag::LevelUnknown, const char *echoedCmd);
MeadeResponse makeResponse(tag::Int, int value);
MeadeResponse makeResponse(tag::Long, long value);
MeadeResponse makeResponse(tag::LongPairPipe, long a, long b);
MeadeResponse makeResponse(tag::CompactHms, int hours, int minutes, int seconds);

/**
 * @brief Primary template for the Get family kind -> tag mapping.
 *
 * Each specialisation exposes a `make(args...)` that forwards to the
 * matching `makeResponse` overload. C++11/14 has no `template <auto>`, so
 * one primary template exists per command-kind enum. Missing
 * specialisations instantiate the (undefined) primary template, producing
 * a compile error that points at the call site.
 */
template <MeadeGetCommandKind K> struct GetResponse;
/**
 * @brief Primary template for the Set family kind -> tag mapping.
 *
 * See `GetResponse` for details.
 */
template <MeadeSetCommandKind K> struct SetResponse;
/**
 * @brief Primary template for the Movement family kind -> tag mapping.
 *
 * See `GetResponse` for details.
 */
template <MeadeMovementCommandKind K> struct MovementResponse;
/**
 * @brief Primary template for the Home family kind -> tag mapping.
 *
 * See `GetResponse` for details.
 */
template <MeadeHomeCommandKind K> struct HomeResponse;
/**
 * @brief Primary template for the Quit family kind -> tag mapping.
 *
 * See `GetResponse` for details.
 */
template <MeadeQuitCommandKind K> struct QuitResponse;
/**
 * @brief Primary template for the SlewRate family kind -> tag mapping.
 *
 * See `GetResponse` for details.
 */
template <MeadeSlewRateCommandKind K> struct SlewRateResponse;
/**
 * @brief Primary template for the Extra family kind -> tag mapping.
 *
 * See `GetResponse` for details.
 */
template <MeadeExtraCommandKind K> struct ExtraResponse;
/**
 * @brief Primary template for the ExtraLeaf family kind -> tag mapping.
 *
 * See `GetResponse` for details.
 */
template <MeadeExtraLeafCommandKind K> struct ExtraLeafResponse;
/**
 * @brief Primary template for the Focus family kind -> tag mapping.
 *
 * See `GetResponse` for details.
 */
template <MeadeFocusCommandKind K> struct FocusResponse;
/**
 * @brief Primary template for the Gps family kind -> tag mapping.
 *
 * See `GetResponse` for details.
 */
template <MeadeGpsCommandKind K> struct GpsResponse;
/**
 * @brief Primary template for the Sync family kind -> tag mapping.
 *
 * See `GetResponse` for details.
 */
template <MeadeSyncCommandKind K> struct SyncResponse;

/**
 * @brief Per-family entry point: build the response for a Get-family kind.
 *
 * Example:
 * @code
 * return respondGet<MeadeGetCommandKind::FirmwareVersion>(versionString).c_str();
 * @endcode
 */
template <MeadeGetCommandKind K, typename... Args> MeadeResponse respondGet(Args &&...args)
{
    return GetResponse<K>::make(std::forward<Args>(args)...);
}
/**
 * @brief Per-family entry point: build the response for a Set-family kind.
 */
template <MeadeSetCommandKind K, typename... Args> MeadeResponse respondSet(Args &&...args)
{
    return SetResponse<K>::make(std::forward<Args>(args)...);
}
/**
 * @brief Per-family entry point: build the response for a Movement-family kind.
 */
template <MeadeMovementCommandKind K, typename... Args> MeadeResponse respondMovement(Args &&...args)
{
    return MovementResponse<K>::make(std::forward<Args>(args)...);
}
/**
 * @brief Per-family entry point: build the response for a Home-family kind.
 */
template <MeadeHomeCommandKind K, typename... Args> MeadeResponse respondHome(Args &&...args)
{
    return HomeResponse<K>::make(std::forward<Args>(args)...);
}
/**
 * @brief Per-family entry point: build the response for a Quit-family kind.
 */
template <MeadeQuitCommandKind K, typename... Args> MeadeResponse respondQuit(Args &&...args)
{
    return QuitResponse<K>::make(std::forward<Args>(args)...);
}
/**
 * @brief Per-family entry point: build the response for a SlewRate-family kind.
 */
template <MeadeSlewRateCommandKind K, typename... Args> MeadeResponse respondSlewRate(Args &&...args)
{
    return SlewRateResponse<K>::make(std::forward<Args>(args)...);
}
/**
 * @brief Per-family entry point: build the response for an Extra-family kind.
 */
template <MeadeExtraCommandKind K, typename... Args> MeadeResponse respondExtra(Args &&...args)
{
    return ExtraResponse<K>::make(std::forward<Args>(args)...);
}
/**
 * @brief Per-family entry point: build the response for an ExtraLeaf kind.
 */
template <MeadeExtraLeafCommandKind K, typename... Args> MeadeResponse respondExtraLeaf(Args &&...args)
{
    return ExtraLeafResponse<K>::make(std::forward<Args>(args)...);
}
/**
 * @brief Per-family entry point: build the response for a Focus-family kind.
 */
template <MeadeFocusCommandKind K, typename... Args> MeadeResponse respondFocus(Args &&...args)
{
    return FocusResponse<K>::make(std::forward<Args>(args)...);
}
/**
 * @brief Per-family entry point: build the response for a Gps-family kind.
 */
template <MeadeGpsCommandKind K, typename... Args> MeadeResponse respondGps(Args &&...args)
{
    return GpsResponse<K>::make(std::forward<Args>(args)...);
}
/**
 * @brief Per-family entry point: build the response for a Sync-family kind.
 */
template <MeadeSyncCommandKind K, typename... Args> MeadeResponse respondSync(Args &&...args)
{
    return SyncResponse<K>::make(std::forward<Args>(args)...);
}

/**
 * @brief Bind a `(Family, Kind)` pair to a response `Tag`.
 *
 * Forwards all caller arguments to `makeResponse(Tag{}, args...)`.
 *
 * @param Family One of `Get`, `Set`, `Movement`, `Home`, `Quit`, `SlewRate`,
 *               `Extra`, `ExtraLeaf`, `Focus`, `Gps`, `Sync`.
 * @param Kind   An enumerator of `Meade<Family>CommandKind`.
 * @param Tag    A tag type in `response::tag`.
 */
#define OAT_MEADE_BIND_RESPONSE(Family, Kind, Tag)                                                                                         \
    template <> struct Family##Response<Meade##Family##CommandKind::Kind> {                                                                \
        using type = response::tag::Tag;                                                                                                   \
        template <typename... Args> static MeadeResponse make(Args &&...args)                                                              \
        {                                                                                                                                  \
            return makeResponse(type {}, std::forward<Args>(args)...);                                                                     \
        }                                                                                                                                  \
    }

/**
 * @brief Like `OAT_MEADE_BIND_RESPONSE` but prepends a fixed argument.
 *
 * Useful when several kinds share a tag but differ only in a constant
 * leading parameter (e.g. site-name slot index).
 *
 * @param FixedArg A constant expression injected as the first argument to
 *                 `makeResponse(Tag{}, FixedArg, args...)`.
 */
#define OAT_MEADE_BIND_RESPONSE_FIXED(Family, Kind, Tag, FixedArg)                                                                         \
    template <> struct Family##Response<Meade##Family##CommandKind::Kind> {                                                                \
        using type = response::tag::Tag;                                                                                                   \
        template <typename... Args> static MeadeResponse make(Args &&...args)                                                              \
        {                                                                                                                                  \
            return makeResponse(type {}, FixedArg, std::forward<Args>(args)...);                                                           \
        }                                                                                                                                  \
    }

// ---- Get family bindings ------------------------------------------------
OAT_MEADE_BIND_RESPONSE(Get, FirmwareVersion, Text);
OAT_MEADE_BIND_RESPONSE(Get, ProductName, Text);
OAT_MEADE_BIND_RESPONSE(Get, MountStatus, Text);
OAT_MEADE_BIND_RESPONSE(Get, TargetRa, RaCoordinate);
OAT_MEADE_BIND_RESPONSE(Get, CurrentRa, RaCoordinate);
OAT_MEADE_BIND_RESPONSE(Get, TargetDec, DecCoordinate);
OAT_MEADE_BIND_RESPONSE(Get, CurrentDec, DecCoordinate);
OAT_MEADE_BIND_RESPONSE(Get, IsSlewing, Boolean);
OAT_MEADE_BIND_RESPONSE(Get, IsTracking, Boolean);
OAT_MEADE_BIND_RESPONSE(Get, IsGuiding, Boolean);
OAT_MEADE_BIND_RESPONSE(Get, SiteLatitude, SiteLatitude);
OAT_MEADE_BIND_RESPONSE(Get, SiteLongitude, SiteLongitude);
OAT_MEADE_BIND_RESPONSE(Get, ClockFormat, ClockFormat24);
OAT_MEADE_BIND_RESPONSE(Get, UtcOffset, UtcOffset);
OAT_MEADE_BIND_RESPONSE(Get, LocalTime12h, LocalTime);
OAT_MEADE_BIND_RESPONSE(Get, LocalTime24h, LocalTime);
OAT_MEADE_BIND_RESPONSE(Get, LocalDate, LocalDate);
OAT_MEADE_BIND_RESPONSE_FIXED(Get, SiteName1, SiteNameSlot, 1);
OAT_MEADE_BIND_RESPONSE_FIXED(Get, SiteName2, SiteNameSlot, 2);
OAT_MEADE_BIND_RESPONSE_FIXED(Get, SiteName3, SiteNameSlot, 3);
OAT_MEADE_BIND_RESPONSE_FIXED(Get, SiteName4, SiteNameSlot, 4);
OAT_MEADE_BIND_RESPONSE(Get, TrackingRate, TrackingRate);

// ---- Set family bindings ------------------------------------------------
OAT_MEADE_BIND_RESPONSE(Set, TargetDec, SetSuccess);
OAT_MEADE_BIND_RESPONSE(Set, TargetRa, SetSuccess);
OAT_MEADE_BIND_RESPONSE(Set, LocalSiderealTime, SetSuccess);
OAT_MEADE_BIND_RESPONSE(Set, HomePoint, SetSuccess);
OAT_MEADE_BIND_RESPONSE(Set, HourAngle, SetSuccess);
OAT_MEADE_BIND_RESPONSE(Set, SyncCoordinates, SetSuccess);
OAT_MEADE_BIND_RESPONSE(Set, SiteLatitude, SetSuccess);
OAT_MEADE_BIND_RESPONSE(Set, SiteLongitude, SetSuccess);
OAT_MEADE_BIND_RESPONSE(Set, UtcOffset, SetSuccess);
OAT_MEADE_BIND_RESPONSE(Set, LocalTime, SetSuccess);
OAT_MEADE_BIND_RESPONSE(Set, LocalDate, SetLocalDateAck);

// ---- Movement family bindings -------------------------------------------
OAT_MEADE_BIND_RESPONSE_FIXED(Movement, SlewToTarget, SetSuccess, false);
OAT_MEADE_BIND_RESPONSE(Movement, TrackingToggle, SetSuccess);
OAT_MEADE_BIND_RESPONSE(Movement, GuidePulse, Literal);
OAT_MEADE_BIND_RESPONSE(Movement, MoveAzAltHome, SetSuccess);
OAT_MEADE_BIND_RESPONSE(Movement, MoveAzimuth, Empty);
OAT_MEADE_BIND_RESPONSE(Movement, MoveAltitude, Empty);
OAT_MEADE_BIND_RESPONSE(Movement, SlewEast, Empty);
OAT_MEADE_BIND_RESPONSE(Movement, SlewWest, Empty);
OAT_MEADE_BIND_RESPONSE(Movement, SlewNorth, Empty);
OAT_MEADE_BIND_RESPONSE(Movement, SlewSouth, Empty);
OAT_MEADE_BIND_RESPONSE(Movement, MoveStepper, SetSuccess);
OAT_MEADE_BIND_RESPONSE(Movement, HomeRa, SetSuccess);
OAT_MEADE_BIND_RESPONSE(Movement, HomeDec, SetSuccess);

// ---- Home family bindings -----------------------------------------------
OAT_MEADE_BIND_RESPONSE(Home, Park, Empty);
OAT_MEADE_BIND_RESPONSE(Home, Home, Empty);
OAT_MEADE_BIND_RESPONSE(Home, Unpark, SetSuccess);
OAT_MEADE_BIND_RESPONSE(Home, SetAzAltHome, SetSuccess);

// ---- Quit family bindings -----------------------------------------------
OAT_MEADE_BIND_RESPONSE(Quit, StopAll, Empty);
OAT_MEADE_BIND_RESPONSE(Quit, StopDirectionalAll, Empty);
OAT_MEADE_BIND_RESPONSE(Quit, StopEast, Empty);
OAT_MEADE_BIND_RESPONSE(Quit, StopWest, Empty);
OAT_MEADE_BIND_RESPONSE(Quit, StopNorth, Empty);
OAT_MEADE_BIND_RESPONSE(Quit, StopSouth, Empty);
OAT_MEADE_BIND_RESPONSE(Quit, QuitControlMode, Empty);

// ---- SlewRate family bindings -------------------------------------------
OAT_MEADE_BIND_RESPONSE(SlewRate, Slew, Empty);
OAT_MEADE_BIND_RESPONSE(SlewRate, Find, Empty);
OAT_MEADE_BIND_RESPONSE(SlewRate, Center, Empty);
OAT_MEADE_BIND_RESPONSE(SlewRate, Guide, Empty);

// ---- GPS family bindings ------------------------------------------------
OAT_MEADE_BIND_RESPONSE(Gps, StartAcquisition, SetSuccess);

// ---- Sync family bindings -----------------------------------------------
OAT_MEADE_BIND_RESPONSE_FIXED(Sync, SyncToTarget, Text, "NONE");

// ---- Focus family bindings ----------------------------------------------
OAT_MEADE_BIND_RESPONSE(Focus, ContinuousIn, Empty);
OAT_MEADE_BIND_RESPONSE(Focus, ContinuousOut, Empty);
OAT_MEADE_BIND_RESPONSE(Focus, MoveBy, Empty);
OAT_MEADE_BIND_RESPONSE(Focus, SetSpeedByRate, Empty);
OAT_MEADE_BIND_RESPONSE(Focus, SetFastestRate, Empty);
OAT_MEADE_BIND_RESPONSE(Focus, SetSlowestRate, Empty);
OAT_MEADE_BIND_RESPONSE(Focus, Stop, Empty);
OAT_MEADE_BIND_RESPONSE(Focus, GetPosition, Long);
OAT_MEADE_BIND_RESPONSE(Focus, SetPosition, SetSuccess);
OAT_MEADE_BIND_RESPONSE(Focus, GetState, SetSuccess);

// ---- Extra (top-level) family bindings ----------------------------------
OAT_MEADE_BIND_RESPONSE(Extra, DriftAlignment, Empty);
OAT_MEADE_BIND_RESPONSE(Extra, FactoryReset, Boolean);

// ---- ExtraLeaf family bindings ------------------------------------------
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, GetRaStepsPerDegree, NumericFloat);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, GetDecStepsPerDegree, NumericFloat);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, GetDecLimitBoth, DecLimitsPair);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, GetDecLimitLowerOnly, NumericFloat);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, GetDecLimitUpperOnly, NumericFloat);
OAT_MEADE_BIND_RESPONSE_FIXED(ExtraLeaf, GetDecLimitInvalidVariant, Boolean, false);
OAT_MEADE_BIND_RESPONSE_FIXED(ExtraLeaf, GetDecParking, Boolean, false);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, GetTrackingSpeedCalibration, NumericFloat);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, GetRemainingSafeTime, NumericFloat);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, GetTrackingSpeed, NumericFloat);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, GetBacklashSteps, Int);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, GetAltStepsPerDegree, NumericFloat);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, GetAzStepsPerDegree, NumericFloat);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, GetAutoHomingStates, Text);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, GetAzAltPositions, LongPairPipe);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, GetTargetCoordinatePositions, LongPairPipe);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, GetMountHardwareInfo, Text);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, GetStepperInfo, Text);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, GetLogBuffer, Literal);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, GetHourAngle, CompactHms);
OAT_MEADE_BIND_RESPONSE_FIXED(ExtraLeaf, GetHourAngleInvalidVariant, Boolean, false);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, GetRaHomingOffset, Long);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, GetDecHomingOffset, Long);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, GetHemisphere, Hemisphere);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, GetLocalSiderealTime, CompactHms);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, GetNetworkStatus, Text);
// All Set* sub-leaves return "" after dispatch.
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, SetRaStepsPerDegree, Empty);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, SetAzStepsPerDegree, Empty);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, SetAltStepsPerDegree, Empty);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, SetDecStepsPerDegree, Empty);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, SetDecLimitLowerSet, Empty);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, SetDecLimitUpperSet, Empty);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, SetDecLimitLowerClear, Empty);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, SetDecLimitUpperClear, Empty);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, SetDecParking, Empty);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, SetTrackingSpeedCalibration, Empty);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, SetTrackingStepperPosition, Empty);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, SetManualSlewMode, Empty);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, SetRaManualSpeed, Empty);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, SetDecManualSpeed, Empty);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, SetBacklashCorrection, Empty);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, SetRaHomingOffset, Empty);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, SetDecHomingOffset, Empty);
// Level commands.
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, LevelGetReferenceAngles, AnglePair4);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, LevelGetCurrentAngles, AnglePair4);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, LevelGetTemperature, NumericFloat);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, LevelSetReferencePitch, Boolean);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, LevelSetReferenceRoll, Boolean);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, LevelStartup, Boolean);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, LevelShutdown, Boolean);
OAT_MEADE_BIND_RESPONSE(ExtraLeaf, LevelUnknownVariant, LevelUnknown);

}  // namespace response

}  // namespace meade
}  // namespace core
}  // namespace oat
