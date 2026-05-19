/**
 * @file MeadeParserSet.cpp
 * @brief Set-family (`:S...`) dispatcher for the Meade LX200 parser.
 */

#include "MeadeParser.hpp"
#include "MeadeParserHelpers.hpp"

#include <stddef.h>
#include <stdint.h>

namespace oat
{
namespace core
{
namespace meade
{

namespace
{

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
                if (!c.digits(2, hh) || c.peek() == '\0' || !c.match(c.peek()) || !c.digits(2, mm) || !c.atEnd())
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

}  // namespace meade
}  // namespace core
}  // namespace oat
