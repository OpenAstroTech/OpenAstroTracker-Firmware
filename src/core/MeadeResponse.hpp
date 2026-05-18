#pragma once

/**
 * @file MeadeResponse.hpp
 * @brief Type-safe Meade response API.
 *
 * The orchestrator (`MeadeCommandProcessor`) builds a response by selecting a
 * command kind at compile time; the trait layer (`Response<K>`) maps that
 * kind to a response *shape* (tag) and ultimately to a `makeResponse`
 * overload that owns the wire formatting for that shape. Passing the wrong
 * argument types is rejected at compile time; forgetting to specialise the
 * trait surfaces as an incomplete-type error at the call site.
 *
 * ### Layers
 * - **Tags** (`response::tag::*`): zero-size types identifying a wire shape.
 * - **Factories** (`makeResponse(tag::X, args...)`): exactly one overload per
 *   shape; owns the printf/format logic for that shape.
 * - **Trait** (`Response<K>`): compile-time kind -> tag mapping, producing a
 *   `make(args...)` shim that forwards to `makeResponse`. A single primary
 *   template using `template <auto>` handles every command family.
 * - **Entry point** (`respond<K>(args...)`): user-facing helper that binds
 *   any kind enumerator to its trait.
 *
 * Framing (the trailing `#` byte that terminates each Meade reply) is
 * centralised in `MeadeResponse.cpp` via `appendTerminator`; per-shape
 * formatters only write payload bytes.
 */

#include <stddef.h>
#include <stdint.h>

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
    static constexpr size_t Capacity = 200;

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
    size_t length() const
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
    static constexpr size_t capacity()
    {
        return Capacity;
    }
    /**
     * @brief Internal mutator.
     *
     * Used only by `makeResponse` overloads in `MeadeResponse.cpp`. Not
     * intended for direct caller use.
     */
    void setLength(size_t n)
    {
        _length = n;
    }

  private:
    char _data[Capacity];
    size_t _length;
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
 * @brief Primary template mapping any Meade kind enumerator to its response.
 *
 * A single primary template handles every command family because C++17's
 * `template <auto>` accepts any non-type template argument; each strong-enum
 * value carries its own type, so specialisations across families remain
 * mutually distinct. The primary template is intentionally undefined so a
 * missing binding surfaces as a readable diagnostic at the call site.
 *
 * Bind a kind to a wire-shape tag with `OAT_MEADE_BIND_RESPONSE`.
 */
template <typename E, E K> struct Response;

/**
 * @brief Entry point: build the response for any Meade command kind.
 *
 * Example:
 * @code
 * return respond<MeadeGetCommandKind::FirmwareVersion>(versionString).c_str();
 * @endcode
 *
 * @tparam K    A `Meade<Family>CommandKind` enumerator.
 * @tparam Args Argument pack forwarded to `makeResponse` for the bound tag.
 */
template <auto K, typename... Args> MeadeResponse respond(Args &&...args)
{
    return Response<decltype(K), K>::make(args...);
}

/**
 * @brief Bind a command-kind enumerator to a response wire-shape `Tag`.
 *
 * Forwards all caller arguments to `makeResponse(Tag{}, args...)`.
 *
 * @param KindExpr A fully-qualified enumerator, e.g. `MeadeGetCommandKind::FirmwareVersion`.
 * @param Tag      A tag type in `response::tag`.
 */
#define OAT_MEADE_BIND_RESPONSE(KindExpr, Tag)                                                                                             \
    template <> struct Response<decltype(KindExpr), KindExpr> {                                                                            \
        using type = response::tag::Tag;                                                                                                   \
        template <typename... Args> static MeadeResponse make(Args &&...args)                                                              \
        {                                                                                                                                  \
            return makeResponse(type {}, args...);                                                                                         \
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
#define OAT_MEADE_BIND_RESPONSE_FIXED(KindExpr, Tag, FixedArg)                                                                             \
    template <> struct Response<decltype(KindExpr), KindExpr> {                                                                            \
        using type = response::tag::Tag;                                                                                                   \
        template <typename... Args> static MeadeResponse make(Args &&...args)                                                              \
        {                                                                                                                                  \
            return makeResponse(type {}, FixedArg, args...);                                                                               \
        }                                                                                                                                  \
    }

// ---- Get family ---------------------------------------------------------
// The Get family does not use the kind->tag binding layer. Get commands are
// dispatched and serialised directly by `handleMeadeGet` (see MeadeParser.hpp).

// ---- Set family ---------------------------------------------------------
// The Set family does not use the kind->tag binding layer. Set commands are
// dispatched, parsed and serialised directly by `handleMeadeSet` (see
// MeadeParser.hpp).

// ---- Movement family bindings -------------------------------------------
OAT_MEADE_BIND_RESPONSE_FIXED(MeadeMovementCommandKind::SlewToTarget, SetSuccess, false);
OAT_MEADE_BIND_RESPONSE(MeadeMovementCommandKind::TrackingToggle, SetSuccess);
OAT_MEADE_BIND_RESPONSE(MeadeMovementCommandKind::GuidePulse, Literal);
OAT_MEADE_BIND_RESPONSE(MeadeMovementCommandKind::MoveAzAltHome, SetSuccess);
OAT_MEADE_BIND_RESPONSE(MeadeMovementCommandKind::MoveAzimuth, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeMovementCommandKind::MoveAltitude, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeMovementCommandKind::SlewEast, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeMovementCommandKind::SlewWest, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeMovementCommandKind::SlewNorth, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeMovementCommandKind::SlewSouth, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeMovementCommandKind::MoveStepper, SetSuccess);
OAT_MEADE_BIND_RESPONSE(MeadeMovementCommandKind::HomeRa, SetSuccess);
OAT_MEADE_BIND_RESPONSE(MeadeMovementCommandKind::HomeDec, SetSuccess);

// ---- Home family bindings -----------------------------------------------
OAT_MEADE_BIND_RESPONSE(MeadeHomeCommandKind::Park, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeHomeCommandKind::Home, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeHomeCommandKind::Unpark, SetSuccess);
OAT_MEADE_BIND_RESPONSE(MeadeHomeCommandKind::SetAzAltHome, SetSuccess);

// ---- Quit family bindings -----------------------------------------------
OAT_MEADE_BIND_RESPONSE(MeadeQuitCommandKind::StopAll, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeQuitCommandKind::StopDirectionalAll, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeQuitCommandKind::StopEast, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeQuitCommandKind::StopWest, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeQuitCommandKind::StopNorth, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeQuitCommandKind::StopSouth, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeQuitCommandKind::QuitControlMode, Empty);

// ---- SlewRate family bindings -------------------------------------------
OAT_MEADE_BIND_RESPONSE(MeadeSlewRateCommandKind::Slew, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeSlewRateCommandKind::Find, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeSlewRateCommandKind::Center, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeSlewRateCommandKind::Guide, Empty);

// ---- GPS family bindings ------------------------------------------------
OAT_MEADE_BIND_RESPONSE(MeadeGpsCommandKind::StartAcquisition, SetSuccess);

// ---- Sync family bindings -----------------------------------------------
OAT_MEADE_BIND_RESPONSE_FIXED(MeadeSyncCommandKind::SyncToTarget, Text, "NONE");

// ---- Focus family bindings ----------------------------------------------
OAT_MEADE_BIND_RESPONSE(MeadeFocusCommandKind::ContinuousIn, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeFocusCommandKind::ContinuousOut, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeFocusCommandKind::MoveBy, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeFocusCommandKind::SetSpeedByRate, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeFocusCommandKind::SetFastestRate, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeFocusCommandKind::SetSlowestRate, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeFocusCommandKind::Stop, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeFocusCommandKind::GetPosition, Long);
OAT_MEADE_BIND_RESPONSE(MeadeFocusCommandKind::SetPosition, SetSuccess);
OAT_MEADE_BIND_RESPONSE(MeadeFocusCommandKind::GetState, SetSuccess);

// ---- Extra (top-level) family bindings ----------------------------------
OAT_MEADE_BIND_RESPONSE(MeadeExtraCommandKind::DriftAlignment, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeExtraCommandKind::FactoryReset, Boolean);

// ---- ExtraLeaf family bindings ------------------------------------------
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::GetRaStepsPerDegree, NumericFloat);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::GetDecStepsPerDegree, NumericFloat);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::GetDecLimitBoth, DecLimitsPair);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::GetDecLimitLowerOnly, NumericFloat);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::GetDecLimitUpperOnly, NumericFloat);
OAT_MEADE_BIND_RESPONSE_FIXED(MeadeExtraLeafCommandKind::GetDecLimitInvalidVariant, Boolean, false);
OAT_MEADE_BIND_RESPONSE_FIXED(MeadeExtraLeafCommandKind::GetDecParking, Boolean, false);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::GetTrackingSpeedCalibration, NumericFloat);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::GetRemainingSafeTime, NumericFloat);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::GetTrackingSpeed, NumericFloat);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::GetBacklashSteps, Int);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::GetAltStepsPerDegree, NumericFloat);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::GetAzStepsPerDegree, NumericFloat);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::GetAutoHomingStates, Text);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::GetAzAltPositions, LongPairPipe);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::GetTargetCoordinatePositions, LongPairPipe);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::GetMountHardwareInfo, Text);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::GetStepperInfo, Text);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::GetLogBuffer, Literal);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::GetHourAngle, CompactHms);
OAT_MEADE_BIND_RESPONSE_FIXED(MeadeExtraLeafCommandKind::GetHourAngleInvalidVariant, Boolean, false);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::GetRaHomingOffset, Long);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::GetDecHomingOffset, Long);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::GetHemisphere, Hemisphere);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::GetLocalSiderealTime, CompactHms);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::GetNetworkStatus, Text);
// All Set* sub-leaves return "" after dispatch.
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::SetRaStepsPerDegree, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::SetAzStepsPerDegree, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::SetAltStepsPerDegree, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::SetDecStepsPerDegree, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::SetDecLimitLowerSet, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::SetDecLimitUpperSet, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::SetDecLimitLowerClear, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::SetDecLimitUpperClear, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::SetDecParking, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::SetTrackingSpeedCalibration, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::SetTrackingStepperPosition, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::SetManualSlewMode, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::SetRaManualSpeed, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::SetDecManualSpeed, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::SetBacklashCorrection, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::SetRaHomingOffset, Empty);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::SetDecHomingOffset, Empty);
// Level commands.
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::LevelGetReferenceAngles, AnglePair4);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::LevelGetCurrentAngles, AnglePair4);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::LevelGetTemperature, NumericFloat);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::LevelSetReferencePitch, Boolean);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::LevelSetReferenceRoll, Boolean);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::LevelStartup, Boolean);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::LevelShutdown, Boolean);
OAT_MEADE_BIND_RESPONSE(MeadeExtraLeafCommandKind::LevelUnknownVariant, LevelUnknown);

}  // namespace response

}  // namespace meade
}  // namespace core
}  // namespace oat
