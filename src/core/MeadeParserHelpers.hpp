#pragma once

/**
 * @file MeadeParserHelpers.hpp
 * @brief Private helper declarations shared across MeadeParser family implementations.
 *
 * This header is internal to src/core/ — not part of the public API.
 * All symbols live in oat::core::meade.
 */

#include "core/MeadeParser.hpp"

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
// Forward-only; never backtracks. Each primitive returns `false` on mismatch
// (cursor is advanced on success). Ideal for fixed-format Meade sub-commands
// like coordinates, times, and dates.
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

    /// Read exactly `n` decimal digits into `out` (big-endian, no separators).
    bool digits(int n, unsigned &out);

    /// Read "+DD" or "-DD" into a signed int.
    bool signed2(int &out);

    /// Read "+DDD" or "-DDD" into a signed int.
    bool signed3(int &out);

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
// High-level make*Response — thin wrappers: create a response, delegate to
// write*, append `#` when needed, return.
// ---------------------------------------------------------------------------

MeadeResponse makeLiteralResponse(const char *text);
MeadeResponse makeSetSuccessResponse(bool ok);
MeadeResponse makeFramedTextResponse(const char *text);
MeadeResponse makeLongResponse(long value);
MeadeResponse makeBooleanResponse(bool flag);
MeadeResponse makeNumericFloatResponse(float value, int precision);
MeadeResponse makeIntResponse(int value);
MeadeResponse makeLongPairPipeResponse(long a, long b);
MeadeResponse makeDecLimitsPairResponse(float lo, float hi);
MeadeResponse makeHemisphereResponse(bool north);
MeadeResponse makeCompactHmsResponse(int hours, int minutes, int seconds);
MeadeResponse makeAnglePair4Response(float a, float b);
MeadeResponse makeLevelUnknownResponse(const char *echoedCmd);

// ---------------------------------------------------------------------------
// String helpers
// ---------------------------------------------------------------------------

bool isExact(const char *input, const char *key);
bool startsWith(const char *input, const char *prefix);

}  // namespace meade
}  // namespace core
}  // namespace oat
