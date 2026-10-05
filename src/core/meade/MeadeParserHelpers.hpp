#pragma once

/**
 * @file MeadeParserHelpers.hpp
 * @brief Private helper declarations shared across MeadeParser family implementations.
 *
 * This header is internal to src/core/ — not part of the public API.
 * All symbols live in oat::core::meade.
 */

#include "MeadeParser.hpp"

#include <stddef.h>
#include <stdint.h>

namespace oat
{
namespace core
{
namespace meade
{

// ---------------------------------------------------------------------------
// Cursor — single-pass input cursor with small grammar primitives
//
// Forward-only; never backtracks. The matching primitives return `false` on
// mismatch and advance only on success. The two unconditional ones are the
// exception: `advance` returns nothing and `optionalSign` always returns
// `true`. Ideal for fixed-format Meade sub-commands like coordinates, times,
// and dates.
// ---------------------------------------------------------------------------

class Cursor
{
  public:
    explicit Cursor(const char *p);

    bool atEnd() const;
    char peek() const;
    const char *remaining() const;

    /// Consume one character if it matches `c`; advance on success.
    bool match(char c);

    /// Consume one character if it is any of the chars in `set`.
    bool matchIn(const char *set);

    /// Consume one character unconditionally; a no-op at end of input.
    void advance();

    /// Read exactly `n` decimal digits into `out` (big-endian, no separators).
    bool digits(int n, unsigned &out);

    /// Consume a leading '+' or '-' if present and report it in `sign` as -1
    /// or +1 (+1 when absent). Always succeeds — callers that require a sign
    /// check `peek()` first. Keeping the sign out of the magnitude is what
    /// lets "-00" survive; a signed magnitude cannot hold it.
    bool optionalSign(int &sign);

  private:
    const char *_p;
};

// ---------------------------------------------------------------------------
// Low-level write primitives — mutate a MeadeResponse incrementally.
// They never append the `#` terminator — the caller adds it via
// `writeTerminator` when needed.
// ---------------------------------------------------------------------------

void writeChar(MeadeResponse &r, char c);
void writeText(MeadeResponse &r, const char *s);
void writeTerminator(MeadeResponse &r);
void writeUnsignedPadded(MeadeResponse &r, unsigned value, int width);
void writeSignedPadded(MeadeResponse &r, int value, int digits);
void writeBool01(MeadeResponse &r, bool b);
void writeCString(MeadeResponse &r, const char *s);

void writeRa(MeadeResponse &r, const RaCoordinate &ra);
void writeDec(MeadeResponse &r, const DecCoordinate &d);
void writeLatitude(MeadeResponse &r, const MeadeLatitude &l);
void writeLongitude(MeadeResponse &r, const MeadeLongitude &l);
void writeTime24h(MeadeResponse &r, const MeadeLocalTime &t);
void writeTime12h(MeadeResponse &r, const MeadeLocalTime &t);
void writeLocalDate(MeadeResponse &r, const MeadeLocalDate &d);
void writeUtcOffset(MeadeResponse &r, int hours);
void writeClockFormat(MeadeResponse &r, MeadeClockFormat f);
void writeTrackingRate(MeadeResponse &r, MeadeTrackingRate t);

void writeInt(MeadeResponse &r, int value);
void writeSignedInt(MeadeResponse &r, int value);
void writeLong(MeadeResponse &r, long value);
void writeFloat(MeadeResponse &r, float value, int precision);

// ---------------------------------------------------------------------------
// High-level fill*Response — thin wrappers: populate an existing MeadeResponse,
// delegate to write*, append `#` when needed. Caller owns the response object.
// ---------------------------------------------------------------------------

void fillLiteralResponse(MeadeResponse &r, const char *text);
void fillSetSuccessResponse(MeadeResponse &r, bool ok);
void fillFramedTextResponse(MeadeResponse &r, const char *text);
void fillLongResponse(MeadeResponse &r, long value);
void fillBooleanResponse(MeadeResponse &r, bool flag);
void fillNumericFloatResponse(MeadeResponse &r, float value, int precision);
void fillIntResponse(MeadeResponse &r, int value);
void fillLongPairPipeResponse(MeadeResponse &r, long a, long b);
void fillDecLimitsPairResponse(MeadeResponse &r, float lo, float hi);
void fillHemisphereResponse(MeadeResponse &r, bool north);
void fillCompactHmsResponse(MeadeResponse &r, int hours, int minutes, int seconds);
void fillAnglePair4Response(MeadeResponse &r, float a, float b);
void fillLevelUnknownResponse(MeadeResponse &r, const char *echoedCmd);

// ---------------------------------------------------------------------------
// String helpers
// ---------------------------------------------------------------------------

bool isExact(const char *input, const char *key);
bool startsWith(const char *input, const char *prefix);

}  // namespace meade
}  // namespace core
}  // namespace oat
