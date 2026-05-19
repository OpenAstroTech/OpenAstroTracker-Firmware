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

#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

namespace oat
{
namespace core
{
namespace meade
{

namespace
{

bool isExact(const char *input, const char *key)
{
    return strcmp(input != nullptr ? input : "", key) == 0;
}

bool startsWith(const char *input, const char *prefix)
{
    if (input == nullptr || prefix == nullptr)
    {
        return false;
    }

    while (*prefix != '\0')
    {
        if (*input == '\0' || *input != *prefix)
        {
            return false;
        }
        ++input;
        ++prefix;
    }
    return true;
}

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
    explicit Cursor(const char *p) : _p(p ? p : "") {}

    bool atEnd() const { return *_p == '\0'; }
    char peek()   const { return *_p; }
    const char *remaining() const { return _p; }

    /// Consume one character if it matches `c`; advance on success.
    bool match(char c)
    {
        if (*_p != c) return false;
        ++_p; return true;
    }

    /// Consume one character if it is any of the chars in `set`.
    bool matchIn(const char *set)
    {
        if (*_p == '\0') return false;
        for (const char *s = set; *s; ++s)
        {
            if (*_p == *s) { ++_p; return true; }
        }
        return false;
    }

    /// Read exactly `n` decimal digits into `out` (big-endian, no separators).
    bool digits(int n, unsigned &out)
    {
        unsigned v = 0;
        for (int i = 0; i < n; ++i)
        {
            char c = *_p;
            if (c < '0' || c > '9') return false;
            v = v * 10 + static_cast<unsigned>(c - '0');
            ++_p;
        }
        out = v; return true;
    }

    /// Read "+DD" or "-DD" into a signed int.
    bool signed2(int &out)
    {
        char sign = peek();
        if (sign != '+' && sign != '-') return false;
        ++_p;
        unsigned v = 0;
        if (!digits(2, v)) return false;
        out = (sign == '-') ? -static_cast<int>(v) : static_cast<int>(v);
        return true;
    }

    /// Read "+DDD" or "-DDD" into a signed int.
    bool signed3(int &out)
    {
        char sign = peek();
        if (sign != '+' && sign != '-') return false;
        ++_p;
        unsigned v = 0;
        if (!digits(3, v)) return false;
        out = (sign == '-') ? -static_cast<int>(v) : static_cast<int>(v);
        return true;
    }

  private:
    const char *_p;
};

// Forward declarations for write* primitives (defined later in this namespace).
void writeChar(MeadeResponse &r, char c);
void writeText(MeadeResponse &r, const char *s);
void writeTerminator(MeadeResponse &r);
void writeUnsignedPadded(MeadeResponse &r, unsigned value, int width);
void writeInt(MeadeResponse &r, int value);
void writeSignedInt(MeadeResponse &r, int value);
void writeLong(MeadeResponse &r, long value);
void writeFloat(MeadeResponse &r, float value, int precision);

// ---------------------------------------------------------------------------
// Response-building primitives
//
// Low-level `write*` functions mutate a `MeadeResponse&` incrementally.
// They never append the `#` terminator — the caller adds it via
// `writeTerminator` when needed.
//
// High-level `make*Response` functions are thin wrappers: create a response,
// delegate to `write*`, append `#`, return.
// ---------------------------------------------------------------------------

MeadeResponse makeLiteralResponse(const char *text)
{
    MeadeResponse r;
    writeText(r, text != nullptr ? text : "");
    return r;
}

MeadeResponse makeSetSuccessResponse(bool ok)
{
    MeadeResponse r;
    writeChar(r, ok ? '1' : '0');
    return r;
}

MeadeResponse makeFramedTextResponse(const char *text)
{
    MeadeResponse r;
    writeText(r, text != nullptr ? text : "");
    writeTerminator(r);
    return r;
}

MeadeResponse makeLongResponse(long value)
{
    MeadeResponse r;
    writeLong(r, value);
    writeTerminator(r);
    return r;
}

MeadeResponse makeBooleanResponse(bool flag)
{
    MeadeResponse r;
    writeChar(r, flag ? '1' : '0');
    writeTerminator(r);
    return r;
}

MeadeResponse makeNumericFloatResponse(float value, int precision)
{
    MeadeResponse r;
    writeFloat(r, value, precision);
    writeTerminator(r);
    return r;
}

MeadeResponse makeIntResponse(int value)
{
    MeadeResponse r;
    writeSignedInt(r, value);
    writeTerminator(r);
    return r;
}

MeadeResponse makeLongPairPipeResponse(long a, long b)
{
    MeadeResponse r;
    writeLong(r, a);
    writeChar(r, '|');
    writeLong(r, b);
    writeTerminator(r);
    return r;
}

MeadeResponse makeDecLimitsPairResponse(float lo, float hi)
{
    MeadeResponse r;
    writeFloat(r, lo, 1);
    writeChar(r, '|');
    writeFloat(r, hi, 1);
    writeTerminator(r);
    return r;
}

MeadeResponse makeHemisphereResponse(bool north)
{
    MeadeResponse r;
    writeChar(r, north ? 'N' : 'S');
    writeTerminator(r);
    return r;
}

MeadeResponse makeCompactHmsResponse(int hours, int minutes, int seconds)
{
    MeadeResponse r;
    writeUnsignedPadded(r, static_cast<unsigned>(hours), 2);
    writeUnsignedPadded(r, static_cast<unsigned>(minutes), 2);
    writeUnsignedPadded(r, static_cast<unsigned>(seconds), 2);
    writeTerminator(r);
    return r;
}

MeadeResponse makeAnglePair4Response(float a, float b)
{
    MeadeResponse r;
    writeFloat(r, a, 4);
    writeChar(r, ',');
    writeFloat(r, b, 4);
    writeTerminator(r);
    return r;
}

MeadeResponse makeLevelUnknownResponse(const char *echoedCmd)
{
    MeadeResponse r;
    writeText(r, "Unknown Level command: X");
    writeText(r, echoedCmd != nullptr ? echoedCmd : "");
    writeTerminator(r);
    return r;
}

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
    char normalized[MeadeResponse::Capacity];
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
    switch (family)
    {
        case 'S': case 'M': case 'G': case 'g': case 'C':
        case 'h': case 'I': case 'Q': case 'R': case 'D':
        case 'X': case 'F':
            result.valid   = true;
            result.family  = family;
            result.payload.assign(normalized + 2);
            return result;
        default:
            return result;
    }
}

// ---------------------------------------------------------------------------
// Unified dispatch — parse + classify + dispatch in one call
// ---------------------------------------------------------------------------
MeadeResponse dispatchMeadeCommand(const char *input, IMeadeHandlers &h)
{
    MeadeParseResult parsed = parseMeadeCommand(input);
    if (!parsed.valid)
    {
        return MeadeResponse {};
    }

    switch (parsed.family)
    {
        case 'S': return handleMeadeSet(parsed.payload.c_str(), h);
        case 'M': return handleMeadeMovement(parsed.payload.c_str(), h);
        case 'G': return handleMeadeGet(parsed.payload.c_str(), h);
        case 'g': return handleMeadeGps(parsed.payload.c_str(), h);
        case 'C': return handleMeadeSyncControl(parsed.payload.c_str(), h);
        case 'h': return handleMeadeHome(parsed.payload.c_str(), h);
        case 'I': return handleMeadeInit(parsed.payload.c_str(), h);
        case 'Q': return handleMeadeQuit(parsed.payload.c_str(), h);
        case 'R': return handleMeadeSetSlewRate(parsed.payload.c_str(), h);
        case 'D': return handleMeadeDistance(parsed.payload.c_str(), h);
        case 'X': return handleMeadeExtra(parsed.payload.c_str(), h);
        case 'F': return handleMeadeFocus(parsed.payload.c_str(), h);
        default:  return MeadeResponse {};
    }
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

// Write a positive int without padding or sign.
void writeInt(MeadeResponse &r, int value)
{
    if (value == 0)
    {
        writeChar(r, '0');
        return;
    }
    char buf[12];
    int n = 0;
    while (value > 0 && n < 11)
    {
        buf[n++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    }
    while (n > 0)
    {
        writeChar(r, buf[--n]);
    }
}

// Write a signed int without padding.
void writeSignedInt(MeadeResponse &r, int value)
{
    if (value < 0)
    {
        writeChar(r, '-');
        value = -value;
    }
    writeInt(r, value);
}

// Write a long (signed) without padding.
void writeLong(MeadeResponse &r, long value)
{
    if (value < 0)
    {
        writeChar(r, '-');
        value = -value;
    }
    if (value == 0)
    {
        writeChar(r, '0');
        return;
    }
    char buf[22];
    int n = 0;
    while (value > 0 && n < 20)
    {
        buf[n++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    }
    while (n > 0)
    {
        writeChar(r, buf[--n]);
    }
}

// Write a float with the given decimal precision (0..9).
// No terminator appended.
void writeFloat(MeadeResponse &r, float value, int precision)
{
    if (precision < 0)
    {
        precision = 0;
    }
    if (precision > 9)
    {
        precision = 9;
    }

    double v = static_cast<double>(value);
    bool negative = v < 0;
    if (negative)
    {
        v = -v;
    }

    // Separate integer and fractional parts.
    int intVal = static_cast<int>(v);
    double frac = v - static_cast<double>(intVal);

    if (negative)
    {
        writeChar(r, '-');
    }
    writeInt(r, intVal);

    if (precision > 0)
    {
        writeChar(r, '.');
        for (int i = 0; i < precision; ++i)
        {
            frac *= 10.0;
            int digit = static_cast<int>(frac);
            if (digit > 9)
            {
                digit = 9;  // guard against floating-point rounding
            }
            writeUnsignedPadded(r, static_cast<unsigned>(digit), 1);
            frac -= static_cast<double>(digit);
        }
    }
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

// Format: "[+-]DD<sep>MM:SS" where sep in {'*', ':'}.
bool readDecCoordinate(Cursor &c, DecCoordinate &out)
{
    int deg;
    unsigned mm, ss;
    if (!c.signed2(deg) || !c.matchIn("*:") || !c.digits(2, mm) || !c.match(':') || !c.digits(2, ss))
    {
        return false;
    }
    out.degrees = static_cast<int16_t>(deg);
    out.minutes = static_cast<uint8_t>(mm);
    out.seconds = static_cast<uint8_t>(ss);
    return true;
}

// Format: "HH:MM:SS".
bool readRaCoordinate(Cursor &c, RaCoordinate &out)
{
    unsigned hh, mm, ss;
    if (!c.digits(2, hh) || !c.match(':') || !c.digits(2, mm) || !c.match(':') || !c.digits(2, ss))
    {
        return false;
    }
    out.hours   = static_cast<uint8_t>(hh);
    out.minutes = static_cast<uint8_t>(mm);
    out.seconds = static_cast<uint8_t>(ss);
    return true;
}

// Format: "[+-]DD<sep>MM" where sep in {'*', ':'}.
bool readLatitude(Cursor &c, MeadeLatitude &out)
{
    int deg;
    unsigned mm;
    if (!c.signed2(deg) || !c.matchIn("*:") || !c.digits(2, mm))
    {
        return false;
    }
    out.degrees = static_cast<int16_t>(deg);
    out.minutes = static_cast<uint8_t>(mm);
    return true;
}

// Format: "[+-]DDD<sep>MM" where sep in {'*', ':'}.
bool readLongitude(Cursor &c, MeadeLongitude &out)
{
    int deg;
    unsigned mm;
    if (!c.signed3(deg) || !c.matchIn("*:") || !c.digits(2, mm))
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

    Cursor c(s + 1);

    switch (s[0])
    {
        case 'd':
            {
                DecCoordinate dec;
                if (!readDecCoordinate(c, dec))
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
                if (!readRaCoordinate(c, ra))
                {
                    writeChar(r, '0');
                    return r;
                }
                writeSetAck(r, h.onSetTargetRa(ra));
                return r;
            }

        case 'H':
            if (c.peek() == 'L')
            {
                // HLhhmmss (8 chars) or HLhhmm (6 chars) — no separators on the wire.
                c.match('L');
                unsigned hh = 0, mm = 0, ss = 0;
                bool ok = false;
                if (c.digits(2, hh) && c.digits(2, mm))
                {
                    if (c.atEnd())
                    {
                        ok = true;
                    }
                    else if (c.digits(2, ss) && c.atEnd())
                    {
                        ok = true;
                    }
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
            if (c.peek() == 'P' && c.match('P') && c.atEnd())
            {
                writeSetAck(r, h.onSetHomePoint());
                return r;
            }
            // Bare H = HourAngle: H<hh><sep><mm>. Separator at s[3] is not validated
            // (legacy behaviour: any single char accepted).
            {
                unsigned hh, mm;
                if (!c.digits(2, hh) || c.peek() == '\0' || !c.match(c.peek()) || !c.digits(2, mm))
                {
                    writeChar(r, '0');
                    return r;
                }
                writeSetAck(r, h.onSetHourAngle(static_cast<uint8_t>(hh), static_cast<uint8_t>(mm)));
                return r;
            }

        case 'Y':
            {
                // Y<dec(9)>.<ra(8)>
                DecCoordinate dec;
                RaCoordinate ra;
                if (!readDecCoordinate(c, dec) || !c.match('.') || !readRaCoordinate(c, ra))
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
                if (!readLatitude(c, lat))
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
                if (!readLongitude(c, lon))
                {
                    writeChar(r, '0');
                    return r;
                }
                writeSetAck(r, h.onSetSiteLongitude(lon));
                return r;
            }

        case 'G':
            {
                // G<sign><DD>
                int hours;
                if (!c.signed2(hours))
                {
                    writeChar(r, '0');
                    return r;
                }
                writeSetAck(r, h.onSetUtcOffset(hours));
                return r;
            }

        case 'L':
            {
                // L<HH>:<MM>:<SS>
                unsigned hh, mm, ss;
                if (!c.digits(2, hh) || !c.match(':') || !c.digits(2, mm) || !c.match(':') || !c.digits(2, ss))
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
                // C<MM>/<DD>/<YY>
                unsigned mo, dd, yy;
                if (!c.digits(2, mo) || !c.match('/') || !c.digits(2, dd) || !c.match('/') || !c.digits(2, yy))
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

// ---------------------------------------------------------------------------
// Init family
// ---------------------------------------------------------------------------

MeadeResponse handleMeadeInit(const char *, IMeadeInitHandlers &h)
{
    h.onEnterSerialControl();
    return MeadeResponse {};
}

// ---------------------------------------------------------------------------
// SyncControl family
// ---------------------------------------------------------------------------

MeadeResponse handleMeadeSyncControl(const char *suffix, IMeadeSyncControlHandlers &h)
{
    MeadeResponse r;
    if (suffix != nullptr && suffix[0] == 'M' && suffix[1] == '\0')
    {
        h.onSyncToTarget();
        writeCString(r, "NONE");
    }
    else
    {
        writeCString(r, "FAIL");
    }
    return r;
}

// ---------------------------------------------------------------------------
// Home family
// ---------------------------------------------------------------------------

MeadeResponse handleMeadeHome(const char *suffix, IMeadeHomeHandlers &h)
{
    MeadeResponse r;
    if (suffix == nullptr || suffix[0] == '\0' || suffix[1] != '\0')
    {
        return r;
    }
    switch (suffix[0])
    {
        case 'P':
            h.onPark();
            break;
        case 'F':
            h.onSlewToHome();
            break;
        case 'U':
            h.onUnpark();
            writeChar(r, '1');
            break;
        case 'Z':
            h.onSetAzAltHome();
            writeChar(r, '1');
            break;
        default:
            break;
    }
    return r;
}

// ---------------------------------------------------------------------------
// SetSlewRate family
// ---------------------------------------------------------------------------

MeadeResponse handleMeadeSetSlewRate(const char *suffix, IMeadeSlewRateHandlers &h)
{
    MeadeResponse r;
    if (suffix == nullptr || suffix[0] == '\0' || suffix[1] != '\0')
    {
        return r;
    }
    switch (suffix[0])
    {
        case 'S':
            h.onSetSlewRate(4);
            break;
        case 'M':
            h.onSetSlewRate(3);
            break;
        case 'C':
            h.onSetSlewRate(2);
            break;
        case 'G':
            h.onSetSlewRate(1);
            break;
        default:
            break;
    }
    return r;
}

// ---------------------------------------------------------------------------
// GPSCommands family
// ---------------------------------------------------------------------------

MeadeResponse handleMeadeGps(const char *suffix, IMeadeGpsHandlers &h)
{
    MeadeResponse r;
    if (suffix == nullptr || suffix[0] != 'T')
    {
        writeChar(r, '0');
        return r;
    }
    const bool acquired = h.onStartGpsAcquisition(suffix + 1);
    writeChar(r, acquired ? '1' : '0');
    return r;
}

// ---------------------------------------------------------------------------
// Focus family
// ---------------------------------------------------------------------------

MeadeResponse handleMeadeFocus(const char *suffix, IMeadeFocusHandlers &h)
{
    MeadeResponse r;
    if (suffix == nullptr || suffix[0] == '\0')
    {
        return r;
    }
    // `:F1#` .. `:F4#` — speed-by-rate digits act as the whole input.
    if ((suffix[0] >= '1') && (suffix[0] <= '4') && suffix[1] == '\0')
    {
        h.onFocusSetSpeedByRate(suffix[0] - '0');
        return r;
    }
    switch (suffix[0])
    {
        case '+':
            h.onFocusContinuousIn();
            break;
        case '-':
            h.onFocusContinuousOut();
            break;
        case 'M':
            h.onFocusMoveBy(strtol(suffix + 1, nullptr, 10));
            break;
        case 'F':
            h.onFocusSetSpeedByRate(4);
            break;
        case 'S':
            h.onFocusSetSpeedByRate(1);
            break;
        case 'p':
            return makeLongResponse(h.onFocusGetPosition());
        case 'P':
            if (h.onFocusIsAvailable())
            {
                h.onFocusSetPosition(strtol(suffix + 1, nullptr, 10));
                return makeSetSuccessResponse(true);
            }
            break;
        case 'B':
            return makeSetSuccessResponse(h.onFocusGetState());
        case 'Q':
            h.onFocusStop();
            break;
        default:
            break;
    }
    return r;
}

// ---------------------------------------------------------------------------
// Movement family — dispatched directly (no separate parse step).
// ---------------------------------------------------------------------------
MeadeResponse handleMeadeMovement(const char *suffix, IMeadeMovementHandlers &h)
{
    MeadeResponse r;
    if (suffix == nullptr || suffix[0] == '\0')
    {
        return r;
    }

    Cursor c(suffix);

    // `:MS#` — exact match only ("S123" is not a slew-to-target).
    if (c.peek() == 'S' && c.match('S') && c.atEnd())
    {
        h.onStartSlewToTarget();
        return makeSetSuccessResponse(false);
    }

    // `:MAA...#` — any input starting with "AA" requests a home move.
    if (c.peek() == 'A' && c.match('A'))
    {
        if (c.peek() == 'A')
        {
            c.match('A');
            h.onMoveAzAltHome();
            return makeSetSuccessResponse(true);
        }
        if (c.peek() == 'Z')
        {
            c.match('Z');
            const float arcMinutes = static_cast<float>(strtod(c.remaining(), nullptr));
            h.onMoveAzimuth(arcMinutes);
            return r;
        }
        if (c.peek() == 'L')
        {
            c.match('L');
            const float arcMinutes = static_cast<float>(strtod(c.remaining(), nullptr));
            h.onMoveAltitude(arcMinutes);
            return r;
        }
        return r;
    }

    // `:MHR<R|L>[distance]#` / `:MHD<U|D>[distance]#` — Hall-sensor auto-home.
    if (c.peek() == 'H' && c.match('H'))
    {
        if (c.peek() == 'R' && c.match('R'))
        {
            int direction = 0;
            char d = c.peek();
            if (d == 'R') direction = -1;
            else if (d == 'L') direction = 1;
            if (direction != 0) c.match(d);
            if (direction == 0) return makeSetSuccessResponse(false);
            return makeSetSuccessResponse(h.onHomeRa(direction, c.remaining()));
        }
        if (c.peek() == 'D' && c.match('D'))
        {
            int direction = 0;
            char d = c.peek();
            if (d == 'U') direction = 1;
            else if (d == 'D') direction = -1;
            if (direction != 0) c.match(d);
            if (direction == 0) return makeSetSuccessResponse(false);
            return makeSetSuccessResponse(h.onHomeDec(direction, c.remaining()));
        }
        return r;
    }

    // `:MT1#` / `:MT0#` — tracking toggle.
    if (c.peek() == 'T' && c.match('T'))
    {
        if (c.peek() == '1') { c.match('1'); h.onTrackingOn(); return makeSetSuccessResponse(true); }
        if (c.peek() == '0') { c.match('0'); h.onTrackingOff(); return makeSetSuccessResponse(true); }
        return makeSetSuccessResponse(false);
    }

    // `:MG<dir><DDDD>#` / `:Mg<dir><DDDD>#` — guide pulse.
    if ((c.peek() == 'G' || c.peek() == 'g') && c.match(c.peek()))
    {
        MoveDirection dir = MoveDirection::East;
        const char dc = static_cast<char>(tolower(static_cast<unsigned char>(c.peek())));
        if (dc == 'n') dir = MoveDirection::North;
        else if (dc == 's') dir = MoveDirection::South;
        else if (dc == 'w') dir = MoveDirection::West;
        c.match(c.peek());
        unsigned d = 0;
        if (c.digits(4, d) && c.atEnd())
        {
            h.onGuidePulse(dir, static_cast<int>(d));
            return makeLiteralResponse("");
        }
        return makeLiteralResponse("0");
    }

    // `:MX<axis><steps>#` — move a single stepper by raw step count.
    if (c.peek() == 'X' && c.match('X'))
    {
        MovementAxis axis;
        switch (c.peek())
        {
            case 'r': axis = MovementAxis::Ra; break;
            case 'd': axis = MovementAxis::Dec; break;
            case 'z': axis = MovementAxis::Azimuth; break;
            case 'l': axis = MovementAxis::Altitude; break;
            case 'f': axis = MovementAxis::Focus; break;
            default: return makeSetSuccessResponse(false);
        }
        c.match(c.peek());
        const long steps = strtol(c.remaining(), nullptr, 10);
        h.onMoveStepper(axis, steps);
        return makeSetSuccessResponse(true);
    }

    // Continuous slew shortcuts — single direction letter, anything after ignored.
    switch (c.peek())
    {
        case 'e': h.onSlewEast(); return r;
        case 'w': h.onSlewWest(); return r;
        case 'n': h.onSlewNorth(); return r;
        case 's': h.onSlewSouth(); return r;
        default:  return r;
    }
}

// ---------------------------------------------------------------------------
// Extra family — `:X...` two-level dispatch.
// ---------------------------------------------------------------------------
namespace
{

MeadeResponse handleExtraGetLeaf(const char *leafInput, IMeadeExtraHandlers &h)
{
    MeadeResponse r;

    if (leafInput == nullptr || leafInput[0] == '\0')
    {
        return r;
    }

    if (isExact(leafInput, "R"))
    {
        return makeNumericFloatResponse(h.onGetRaStepsPerDegree(), 1);
    }
    if (isExact(leafInput, "D"))
    {
        return makeNumericFloatResponse(h.onGetDecStepsPerDegree(), 1);
    }
    if (isExact(leafInput, "DL"))
    {
        ExtraDecLimits lim = h.onGetDecLimits();
        return makeDecLimitsPairResponse(lim.lo, lim.hi);
    }
    if (isExact(leafInput, "DLL"))
    {
        return makeNumericFloatResponse(h.onGetDecLimits().lo, 1);
    }
    if (isExact(leafInput, "DLU"))
    {
        return makeNumericFloatResponse(h.onGetDecLimits().hi, 1);
    }
    if (startsWith(leafInput, "DL"))
    {
        return makeBooleanResponse(false);
    }
    if (isExact(leafInput, "DP"))
    {
        return makeBooleanResponse(false);
    }
    if (isExact(leafInput, "S"))
    {
        return makeNumericFloatResponse(h.onGetTrackingSpeedCalibration(), 5);
    }
    if (isExact(leafInput, "ST"))
    {
        return makeNumericFloatResponse(h.onGetRemainingSafeTime(), 7);
    }
    if (isExact(leafInput, "T"))
    {
        return makeNumericFloatResponse(h.onGetTrackingSpeed(), 7);
    }
    if (isExact(leafInput, "B"))
    {
        return makeIntResponse(h.onGetBacklashSteps());
    }
    if (isExact(leafInput, "A"))
    {
        return makeNumericFloatResponse(h.onGetAltStepsPerDegree(), 1);
    }
    if (isExact(leafInput, "AH"))
    {
        return makeFramedTextResponse(h.onGetAutoHomingStates());
    }
    if (isExact(leafInput, "AA"))
    {
        ExtraAzAltPositions p = h.onGetAzAltPositions();
        return makeLongPairPipeResponse(p.az, p.alt);
    }
    if (isExact(leafInput, "Z"))
    {
        return makeNumericFloatResponse(h.onGetAzStepsPerDegree(), 1);
    }
    if (startsWith(leafInput, "C"))
    {
        // Payload format: "<ra>*<dec>" — float pair separated by '*'.
        const char *payload = leafInput + 1;
        const char *star    = strchr(payload, '*');
        if (star == nullptr || star == payload)
        {
            return r;
        }
        const float raCoord    = static_cast<float>(strtod(payload, nullptr));
        const float decCoord   = static_cast<float>(strtod(star + 1, nullptr));
        ExtraStepperCoords pos = h.onGetTargetCoordinatePositions(raCoord, decCoord);
        return makeLongPairPipeResponse(pos.raPos, pos.decPos);
    }
    if (isExact(leafInput, "MS"))
    {
        return makeFramedTextResponse(h.onGetStepperInfo());
    }
    if (startsWith(leafInput, "M"))
    {
        return makeFramedTextResponse(h.onGetMountHardwareInfo());
    }
    if (isExact(leafInput, "O"))
    {
        return makeLiteralResponse(h.onGetLogBuffer());
    }
    if (isExact(leafInput, "HR"))
    {
        return makeLongResponse(h.onGetRaHomingOffset());
    }
    if (isExact(leafInput, "HD"))
    {
        return makeLongResponse(h.onGetDecHomingOffset());
    }
    if (isExact(leafInput, "HS"))
    {
        return makeHemisphereResponse(h.onGetHemisphere());
    }
    if (isExact(leafInput, "H"))
    {
        ExtraHms t = h.onGetHourAngle();
        return makeCompactHmsResponse(t.hours, t.minutes, t.seconds);
    }
    if (startsWith(leafInput, "H"))
    {
        return makeBooleanResponse(false);
    }
    if (isExact(leafInput, "L"))
    {
        ExtraHms t = h.onGetLocalSiderealTime();
        return makeCompactHmsResponse(t.hours, t.minutes, t.seconds);
    }
    if (isExact(leafInput, "N"))
    {
        return makeFramedTextResponse(h.onGetNetworkStatus());
    }
    return r;
}

MeadeResponse handleExtraSetLeaf(const char *leafInput, IMeadeExtraHandlers &h)
{
    MeadeResponse r;
    if (leafInput == nullptr || leafInput[0] == '\0')
    {
        return r;
    }

    if (isExact(leafInput, "DLl"))
    {
        h.onClearDecLimitLower();
        return r;
    }
    if (isExact(leafInput, "DLu"))
    {
        h.onClearDecLimitUpper();
        return r;
    }
    if (startsWith(leafInput, "DLL"))
    {
        const char *payload    = leafInput + 3;
        const bool havePayload = payload[0] != '\0';
        const float value      = havePayload ? static_cast<float>(strtod(payload, nullptr)) : 0.0f;
        h.onSetDecLimitLower(havePayload, value);
        return r;
    }
    if (startsWith(leafInput, "DLU"))
    {
        const char *payload    = leafInput + 3;
        const bool havePayload = payload[0] != '\0';
        const float value      = havePayload ? static_cast<float>(strtod(payload, nullptr)) : 0.0f;
        h.onSetDecLimitUpper(havePayload, value);
        return r;
    }
    if (startsWith(leafInput, "DP"))
    {
        return r;
    }
    if (startsWith(leafInput, "HR"))
    {
        h.onSetRaHomingOffset(strtol(leafInput + 2, nullptr, 10));
        return r;
    }
    if (startsWith(leafInput, "HD"))
    {
        h.onSetDecHomingOffset(strtol(leafInput + 2, nullptr, 10));
        return r;
    }
    if (startsWith(leafInput, "D") && leafInput[1] != '\0')
    {
        const float v = static_cast<float>(strtod(leafInput + 1, nullptr));
        if (v > 0.0f)
        {
            h.onSetDecStepsPerDegree(v);
        }
        return r;
    }
    if (startsWith(leafInput, "R"))
    {
        h.onSetRaStepsPerDegree(static_cast<float>(strtod(leafInput + 1, nullptr)));
        return r;
    }
    if (startsWith(leafInput, "A"))
    {
        h.onSetAzStepsPerDegree(static_cast<float>(strtod(leafInput + 1, nullptr)));
        return r;
    }
    if (startsWith(leafInput, "L"))
    {
        h.onSetAltStepsPerDegree(static_cast<float>(strtod(leafInput + 1, nullptr)));
        return r;
    }
    if (startsWith(leafInput, "S"))
    {
        h.onSetTrackingSpeedCalibration(static_cast<float>(strtod(leafInput + 1, nullptr)));
        return r;
    }
    if (startsWith(leafInput, "T"))
    {
        h.onSetTrackingStepperPosition(strtol(leafInput + 1, nullptr, 10));
        return r;
    }
    if (startsWith(leafInput, "M"))
    {
        h.onSetManualSlewMode(leafInput[1] == '1');
        return r;
    }
    if (startsWith(leafInput, "X"))
    {
        h.onSetRaManualSpeed(static_cast<float>(strtod(leafInput + 1, nullptr)));
        return r;
    }
    if (startsWith(leafInput, "Y"))
    {
        h.onSetDecManualSpeed(static_cast<float>(strtod(leafInput + 1, nullptr)));
        return r;
    }
    if (startsWith(leafInput, "B"))
    {
        h.onSetBacklashCorrection(static_cast<int>(strtol(leafInput + 1, nullptr, 10)));
        return r;
    }
    return r;
}

MeadeResponse handleExtraLevelLeaf(const char *leafInput, IMeadeExtraHandlers &h)
{
    MeadeResponse r;

    if (!h.onLevelIsAvailable())
    {
        return makeBooleanResponse(false);
    }

    if (leafInput == nullptr || leafInput[0] == '\0')
    {
        return r;
    }

    if (startsWith(leafInput, "GR"))
    {
        ExtraPitchRoll pr = h.onLevelGetReferenceAngles();
        return makeAnglePair4Response(pr.pitch, pr.roll);
    }
    if (startsWith(leafInput, "GC"))
    {
        ExtraPitchRoll pr = h.onLevelGetCurrentAngles();
        return makeAnglePair4Response(pr.pitch, pr.roll);
    }
    if (startsWith(leafInput, "GT"))
    {
        return makeNumericFloatResponse(h.onLevelGetTemperature(), 1);
    }
    if (startsWith(leafInput, "G"))
    {
        return r;
    }
    if (startsWith(leafInput, "SP"))
    {
        h.onLevelSetReferencePitch(static_cast<float>(strtod(leafInput + 2, nullptr)));
        return makeBooleanResponse(true);
    }
    if (startsWith(leafInput, "SR"))
    {
        h.onLevelSetReferenceRoll(static_cast<float>(strtod(leafInput + 2, nullptr)));
        return makeBooleanResponse(true);
    }
    if (startsWith(leafInput, "S"))
    {
        return r;
    }
    if (startsWith(leafInput, "1"))
    {
        h.onLevelStartup();
        return makeBooleanResponse(true);
    }
    if (startsWith(leafInput, "0"))
    {
        h.onLevelShutdown();
        return makeBooleanResponse(true);
    }

    // Echo "L" + the original leaf input, matching legacy behavior.
    char echoed[MeadeResponse::Capacity];
    echoed[0] = 'L';
    size_t i  = 0;
    for (; leafInput[i] != '\0' && (i + 2) < sizeof(echoed); ++i)
    {
        echoed[i + 1] = leafInput[i];
    }
    echoed[i + 1] = '\0';
    return makeLevelUnknownResponse(echoed);
}

}  // namespace

MeadeResponse handleMeadeExtra(const char *suffix, IMeadeExtraHandlers &h)
{
    MeadeResponse r;

    if (suffix == nullptr || suffix[0] == '\0')
    {
        return r;
    }

    if (startsWith(suffix, "FR"))
    {
        h.onFactoryReset();
        return makeBooleanResponse(true);
    }

    if (startsWith(suffix, "D"))
    {
        const int duration = static_cast<int>(strtol(suffix + 1, nullptr, 10)) - 3;
        h.onDriftAlignment(duration);
        return r;
    }

    if (startsWith(suffix, "G"))
    {
        return handleExtraGetLeaf(suffix + 1, h);
    }

    if (startsWith(suffix, "S"))
    {
        return handleExtraSetLeaf(suffix + 1, h);
    }

    if (startsWith(suffix, "L"))
    {
        return handleExtraLevelLeaf(suffix + 1, h);
    }

    return r;
}

}  // namespace meade
}  // namespace core
}  // namespace oat
