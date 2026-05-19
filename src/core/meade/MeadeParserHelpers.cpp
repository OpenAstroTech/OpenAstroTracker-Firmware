/**
 * @file MeadeParserHelpers.cpp
 * @brief Shared helper implementations for the Meade LX200 parser.
 *
 * All symbols live in oat::core::meade. This file is internal — not part of
 * the public API (see MeadeParserHelpers.hpp for declarations).
 */

#include "MeadeParserHelpers.hpp"

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

// ---------------------------------------------------------------------------
// Cursor
// ---------------------------------------------------------------------------

Cursor::Cursor(const char *p) : _p(p ? p : "")
{
}

bool Cursor::atEnd() const
{
    return *_p == '\0';
}
char Cursor::peek() const
{
    return *_p;
}
const char *Cursor::remaining() const
{
    return _p;
}

bool Cursor::match(char c)
{
    if (*_p != c)
        return false;
    ++_p;
    return true;
}

bool Cursor::matchIn(const char *set)
{
    if (*_p == '\0')
        return false;
    for (const char *s = set; *s; ++s)
    {
        if (*_p == *s)
        {
            ++_p;
            return true;
        }
    }
    return false;
}

bool Cursor::digits(int n, unsigned &out)
{
    unsigned v = 0;
    for (int i = 0; i < n; ++i)
    {
        char c = *_p;
        if (c < '0' || c > '9')
            return false;
        v = v * 10 + static_cast<unsigned>(c - '0');
        ++_p;
    }
    out = v;
    return true;
}

bool Cursor::signed2(int &out)
{
    char sign = peek();
    if (sign != '+' && sign != '-')
        return false;
    ++_p;
    unsigned v = 0;
    if (!digits(2, v))
        return false;
    out = (sign == '-') ? -static_cast<int>(v) : static_cast<int>(v);
    return true;
}

bool Cursor::signed3(int &out)
{
    char sign = peek();
    if (sign != '+' && sign != '-')
        return false;
    ++_p;
    unsigned v = 0;
    if (!digits(3, v))
        return false;
    out = (sign == '-') ? -static_cast<int>(v) : static_cast<int>(v);
    return true;
}

// ---------------------------------------------------------------------------
// String helpers
// ---------------------------------------------------------------------------

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
// Low-level write primitives
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Format-specific writers
// ---------------------------------------------------------------------------

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

    double v      = static_cast<double>(value);
    bool negative = v < 0;
    if (negative)
    {
        v = -v;
    }

    // Separate integer and fractional parts.
    int intVal  = static_cast<int>(v);
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

// ---------------------------------------------------------------------------
// High-level make*Response wrappers
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

}  // namespace meade
}  // namespace core
}  // namespace oat
