/**
 * @file MeadeResponse.cpp
 * @brief Implementations of `makeResponse(tag::*, ...)` overloads.
 *
 * Each overload owns the wire formatting for one response shape and writes
 * directly into the inline buffer of a fresh `MeadeResponse`. Framing (the
 * trailing `#` byte) is appended via `appendTerminator`; shapes that emit
 * unframed bytes (Empty, Literal, SetSuccess, LevelUnknown) deliberately
 * skip that step.
 */

#include "core/MeadeResponse.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace oat
{
namespace core
{
namespace meade
{

MeadeResponse::MeadeResponse() : _length(0)
{
    _data[0] = '\0';
}

namespace response
{

namespace
{

// Write `text` (NUL-terminated) into `r`'s buffer, clamping to capacity.
void writeText(MeadeResponse &r, const char *text)
{
    if (text == nullptr)
    {
        r.buffer()[0] = '\0';
        r.setLength(0);
        return;
    }
    const std::size_t cap = MeadeResponse::capacity();
    std::size_t i         = 0;
    while (i + 1 < cap && text[i] != '\0')
    {
        r.buffer()[i] = text[i];
        ++i;
    }
    r.buffer()[i] = '\0';
    r.setLength(i);
}

// Write a printf-style format into `r`, clamping to capacity.
void writeFormatted(MeadeResponse &r, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int n = std::vsnprintf(r.buffer(), MeadeResponse::capacity(), fmt, args);
    va_end(args);
    if (n < 0)
    {
        r.buffer()[0] = '\0';
        r.setLength(0);
        return;
    }
    const std::size_t cap = MeadeResponse::capacity();
    std::size_t len       = static_cast<std::size_t>(n);
    if (len >= cap)
    {
        len = cap - 1;
    }
    r.setLength(len);
}

// Framing terminator for Meade responses. Kept in one place so individual
// shape formatters stay focused on payload bytes only.
constexpr char kResponseTerminator = '#';

// Append the framing terminator to `r`, clamping to capacity.
void appendTerminator(MeadeResponse &r)
{
    const std::size_t cap = MeadeResponse::capacity();
    std::size_t len       = r.length();
    if (len + 1 >= cap)
    {
        return;
    }
    r.buffer()[len]     = kResponseTerminator;
    r.buffer()[len + 1] = '\0';
    r.setLength(len + 1);
}

}  // namespace

MeadeResponse makeResponse(tag::Empty)
{
    return MeadeResponse {};
}

MeadeResponse makeResponse(tag::Literal, const char *text)
{
    MeadeResponse r;
    writeText(r, text);
    return r;
}

MeadeResponse makeResponse(tag::Text, const char *body)
{
    MeadeResponse r;
    writeText(r, body != nullptr ? body : "");
    appendTerminator(r);
    return r;
}

MeadeResponse makeResponse(tag::Boolean, bool flag)
{
    MeadeResponse r;
    writeText(r, flag ? "1" : "0");
    appendTerminator(r);
    return r;
}

MeadeResponse makeResponse(tag::SetSuccess, bool ok)
{
    MeadeResponse r;
    writeText(r, ok ? "1" : "0");
    return r;
}

MeadeResponse makeResponse(tag::NumericFloat, float value, int precision)
{
    MeadeResponse r;
    if (precision < 0)
    {
        precision = 0;
    }
    if (precision > 9)
    {
        precision = 9;
    }
    writeFormatted(r, "%.*f", precision, static_cast<double>(value));
    appendTerminator(r);
    return r;
}

MeadeResponse makeResponse(tag::ClockFormat24)
{
    MeadeResponse r;
    writeText(r, "24");
    appendTerminator(r);
    return r;
}

MeadeResponse makeResponse(tag::TrackingRate)
{
    MeadeResponse r;
    writeText(r, "60.0");
    appendTerminator(r);
    return r;
}

MeadeResponse makeResponse(tag::UtcOffset, int hours)
{
    MeadeResponse r;
    writeFormatted(r, "%+03d", hours);
    appendTerminator(r);
    return r;
}

MeadeResponse makeResponse(tag::LocalDate, int month, int day, int year)
{
    MeadeResponse r;
    int yy = year % 100;
    if (yy < 0)
    {
        yy += 100;
    }
    writeFormatted(r, "%02d/%02d/%02d", month, day, yy);
    appendTerminator(r);
    return r;
}

MeadeResponse makeResponse(tag::SiteNameSlot, int slot)
{
    MeadeResponse r;
    writeFormatted(r, "OAT%d", slot);
    appendTerminator(r);
    return r;
}

MeadeResponse makeResponse(tag::RaCoordinate, int hours, int minutes, int seconds)
{
    MeadeResponse r;
    writeFormatted(r, "%02d:%02d:%02d", hours, minutes, seconds);
    appendTerminator(r);
    return r;
}

MeadeResponse makeResponse(tag::RaCoordinate, const char *preformatted)
{
    MeadeResponse r;
    writeText(r, preformatted);
    appendTerminator(r);
    return r;
}

MeadeResponse makeResponse(tag::DecCoordinate, char sign, int degrees, int minutes, int seconds)
{
    MeadeResponse r;
    const char s = (sign == '-') ? '-' : '+';
    writeFormatted(r, "%c%02d*%02d'%02d", s, std::abs(degrees), std::abs(minutes), std::abs(seconds));
    appendTerminator(r);
    return r;
}

MeadeResponse makeResponse(tag::DecCoordinate, const char *preformatted)
{
    MeadeResponse r;
    writeText(r, preformatted);
    appendTerminator(r);
    return r;
}

MeadeResponse makeResponse(tag::SiteLatitude, char sign, int degrees, int minutes)
{
    MeadeResponse r;
    const char s = (sign == '-') ? '-' : '+';
    writeFormatted(r, "%c%02d*%02d", s, std::abs(degrees), std::abs(minutes));
    appendTerminator(r);
    return r;
}

MeadeResponse makeResponse(tag::SiteLatitude, const char *preformatted)
{
    MeadeResponse r;
    writeText(r, preformatted);
    appendTerminator(r);
    return r;
}

MeadeResponse makeResponse(tag::SiteLongitude, char sign, int degrees, int minutes)
{
    MeadeResponse r;
    const char s = (sign == '-') ? '-' : '+';
    writeFormatted(r, "%c%03d*%02d", s, std::abs(degrees), std::abs(minutes));
    appendTerminator(r);
    return r;
}

MeadeResponse makeResponse(tag::SiteLongitude, const char *preformatted)
{
    MeadeResponse r;
    writeText(r, preformatted);
    appendTerminator(r);
    return r;
}

MeadeResponse makeResponse(tag::LocalTime, int hours, int minutes, int seconds)
{
    MeadeResponse r;
    writeFormatted(r, "%02d:%02d:%02d", hours, minutes, seconds);
    appendTerminator(r);
    return r;
}

MeadeResponse makeResponse(tag::LocalTime, const char *preformatted)
{
    MeadeResponse r;
    writeText(r, preformatted);
    appendTerminator(r);
    return r;
}

MeadeResponse makeResponse(tag::DecLimitsPair, float lo, float hi)
{
    MeadeResponse r;
    writeFormatted(r, "%.1f|%.1f", static_cast<double>(lo), static_cast<double>(hi));
    appendTerminator(r);
    return r;
}

MeadeResponse makeResponse(tag::AnglePair, float a, float b)
{
    MeadeResponse r;
    writeFormatted(r, "%.2f,%.2f", static_cast<double>(a), static_cast<double>(b));
    appendTerminator(r);
    return r;
}

MeadeResponse makeResponse(tag::AnglePair4, float a, float b)
{
    MeadeResponse r;
    writeFormatted(r, "%.4f,%.4f", static_cast<double>(a), static_cast<double>(b));
    appendTerminator(r);
    return r;
}

MeadeResponse makeResponse(tag::Hemisphere, bool north)
{
    MeadeResponse r;
    writeText(r, north ? "N" : "S");
    appendTerminator(r);
    return r;
}

MeadeResponse makeResponse(tag::SetLocalDateAck, bool ok)
{
    MeadeResponse r;
    if (ok)
    {
        // Two framed records concatenated: "1Updating Planetary Data" + '#'
        // and 30 spaces + '#'.
        writeText(r, "1Updating Planetary Data");
        appendTerminator(r);
        // Append 30 spaces and a framing terminator.
        const char *padding   = "                              ";  // 30 spaces
        const std::size_t cap = MeadeResponse::capacity();
        std::size_t len       = r.length();
        std::size_t i         = 0;
        while (padding[i] != '\0' && len + 1 < cap)
        {
            r.buffer()[len++] = padding[i++];
        }
        r.buffer()[len] = '\0';
        r.setLength(len);
        appendTerminator(r);
    }
    else
    {
        writeText(r, "0");
    }
    return r;
}

MeadeResponse makeResponse(tag::LevelUnknown, const char *echoedCmd)
{
    MeadeResponse r;
    writeFormatted(r, "Unknown Level command: X%s", echoedCmd != nullptr ? echoedCmd : "");
    return r;
}

MeadeResponse makeResponse(tag::Int, int value)
{
    MeadeResponse r;
    writeFormatted(r, "%d", value);
    appendTerminator(r);
    return r;
}

MeadeResponse makeResponse(tag::Long, long value)
{
    MeadeResponse r;
    writeFormatted(r, "%ld", value);
    appendTerminator(r);
    return r;
}

MeadeResponse makeResponse(tag::LongPairPipe, long a, long b)
{
    MeadeResponse r;
    writeFormatted(r, "%ld|%ld", a, b);
    appendTerminator(r);
    return r;
}

MeadeResponse makeResponse(tag::CompactHms, int hours, int minutes, int seconds)
{
    MeadeResponse r;
    writeFormatted(r, "%02d%02d%02d", hours, minutes, seconds);
    appendTerminator(r);
    return r;
}

}  // namespace response

}  // namespace meade
}  // namespace core
}  // namespace oat
