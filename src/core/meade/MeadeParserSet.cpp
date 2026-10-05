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

// The readers below have always required an explicit sign, and this keeps
// that grammar byte-for-byte unchanged. It is a description of the parser as
// it stands, not of the protocol: MeadeProtocol.hpp documents the sign as
// optional for `:Sg`, where an unsigned value means 0..360 going westward.
// That form is rejected here, exactly as it was before this change.
bool readMandatorySign(Cursor &c, int &sign)
{
    const char first = c.peek();
    if ((first != '+') && (first != '-'))
    {
        return false;
    }
    return c.optionalSign(sign);
}

// Format: "[+-]DD<sep>MM:SS" where sep in {'*', ':'}.
bool readDecCoordinate(Cursor &c, DecCoordinate &out)
{
    int sign;
    unsigned dd, mm, ss;
    if (!readMandatorySign(c, sign) || !c.digits(2, dd) || !c.matchIn("*:") || !c.digits(2, mm) || !c.match(':') || !c.digits(2, ss))
    {
        return false;
    }
    out.degrees  = static_cast<uint16_t>(dd);
    out.minutes  = static_cast<uint8_t>(mm);
    out.seconds  = static_cast<uint8_t>(ss);
    out.negative = (sign < 0);
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
    int sign;
    unsigned dd, mm;
    if (!readMandatorySign(c, sign) || !c.digits(2, dd) || !c.matchIn("*:") || !c.digits(2, mm))
    {
        return false;
    }
    out.degrees  = static_cast<uint16_t>(dd);
    out.minutes  = static_cast<uint8_t>(mm);
    out.negative = (sign < 0);
    return true;
}

// Unsigned :Sg is the legacy 0..360 count running WESTWARD from Greenwich.
// East-positive is what the mount stores, so negate modulo a full circle (which is
// what `fullCircle - arcminutes` is) and wrap into (-180, 180].
// The tempting mistake is the other reflection, the one that lands Greenwich on 180
// — `fullCircle / 2 - arcminutes`, which is what Longitude::ParseFromMeade computes.
// It turns a 121d53' west site into 58d07' east, exactly 180 degrees (12 hours of
// local sidereal time) from where it should be.
// A signed wire value negates the same way, so this is the single mapping for both
// forms: `arcminutes` is the westward count, positive or negative, and never more
// than one full circle from zero.
long westwardToEastPositiveArcminutes(long arcminutes)
{
    const long fullCircle = 360L * 60L;
    long east             = fullCircle - arcminutes;
    while (east > fullCircle / 2)
    {
        east -= fullCircle;
    }
    return east;
}

// Format: "[+-]?DDD<sep>MM" where sep in {'*', ':'}.
//
// The sign is optional. INDI omits it — ":Sg121*53#" goes on the wire for a site
// 121d53' WEST — so demanding one answers INDI's site push with "0" and the mount
// silently keeps whatever longitude it already had.
//
// Only the unsigned form is interpreted here. A signed value is passed through
// unchanged; which hemisphere its sign denotes is a separate question that this
// function deliberately does not answer.
bool readLongitude(Cursor &c, MeadeLongitude &out)
{
    // The sign is optional, and both forms mean the same thing. MeadeProtocol.hpp
    // documents :Sg/:Gg as east-negative, so the legacy unsigned 0..360 westward
    // count that INDI sends is simply the sign == +1 case of the signed form:
    // east = wrap(-value) either way, with no branch between them.
    int sign;
    c.optionalSign(sign);

    unsigned ddd, mm;
    if (!c.digits(3, ddd) || !c.matchIn("*:") || !c.digits(2, mm))
    {
        return false;
    }

    // Reject anything outside one full circle, which nothing downstream does:
    // core::Longitude(int, int, int) never calls checkHours(), and
    // EEPROMStore::storeLongitude clamps degrees*100 into an int16, which destroys
    // the mod-360 equivalence and persists a genuinely wrong site across reboots.
    if ((ddd >= 360) || (mm >= 60))
    {
        return false;
    }

    const long westward  = sign * ((static_cast<long>(ddd) * 60L) + static_cast<long>(mm));
    const long east      = westwardToEastPositiveArcminutes(westward);
    const bool isWest    = (east < 0);
    const long magnitude = isWest ? -east : east;

    out.degrees  = static_cast<uint16_t>(magnitude / 60);
    out.minutes  = static_cast<uint8_t>(magnitude % 60);
    out.negative = isWest;
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

void handleMeadeSet(MeadeResponse &r, const char *s, IMeadeSetHandlers &h)
{
    if (!s || s[0] == '\0')
    {
        writeChar(r, '0');
        return;
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
                    return;
                }
                writeSetAck(r, h.onSetTargetDec(dec));
                return;
            }

        case 'r':
            {
                RaCoordinate ra;
                if (!readRaCoordinate(c, ra))
                {
                    writeChar(r, '0');
                    return;
                }
                writeSetAck(r, h.onSetTargetRa(ra));
                return;
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
                    return;
                }
                MeadeLocalTime t {static_cast<uint8_t>(hh), static_cast<uint8_t>(mm), static_cast<uint8_t>(ss)};
                writeSetAck(r, h.onSetLocalSiderealTime(t));
                return;
            }
            if (c.peek() == 'P' && c.match('P') && c.atEnd())
            {
                writeSetAck(r, h.onSetHomePoint());
                return;
            }
            // Bare H = HourAngle: H<hh><sep><mm>. Separator at s[3] is not validated
            // (legacy behaviour: any single char accepted).
            {
                unsigned hh, mm;
                if (!c.digits(2, hh) || c.peek() == '\0' || !c.match(c.peek()) || !c.digits(2, mm) || !c.atEnd())
                {
                    writeChar(r, '0');
                    return;
                }
                writeSetAck(r, h.onSetHourAngle(static_cast<uint8_t>(hh), static_cast<uint8_t>(mm)));
                return;
            }

        case 'Y':
            {
                // Y<dec(9)>.<ra(8)>
                DecCoordinate dec;
                RaCoordinate ra;
                if (!readDecCoordinate(c, dec) || !c.match('.') || !readRaCoordinate(c, ra))
                {
                    writeChar(r, '0');
                    return;
                }
                writeSetAck(r, h.onSyncCoordinates(dec, ra));
                return;
            }

        case 't':
            {
                MeadeLatitude lat;
                if (!readLatitude(c, lat))
                {
                    writeChar(r, '0');
                    return;
                }
                writeSetAck(r, h.onSetSiteLatitude(lat));
                return;
            }

        case 'g':
            {
                MeadeLongitude lon;
                if (!readLongitude(c, lon))
                {
                    writeChar(r, '0');
                    return;
                }
                writeSetAck(r, h.onSetSiteLongitude(lon));
                return;
            }

        case 'G':
            {
                // G<sign><DD>
                int sign;
                unsigned hours;
                if (!readMandatorySign(c, sign) || !c.digits(2, hours))
                {
                    writeChar(r, '0');
                    return;
                }
                writeSetAck(r, h.onSetUtcOffset(sign * static_cast<int>(hours)));
                return;
            }

        case 'L':
            {
                // L<HH>:<MM>:<SS>
                unsigned hh, mm, ss;
                if (!c.digits(2, hh) || !c.match(':') || !c.digits(2, mm) || !c.match(':') || !c.digits(2, ss))
                {
                    writeChar(r, '0');
                    return;
                }
                MeadeLocalTime t {static_cast<uint8_t>(hh), static_cast<uint8_t>(mm), static_cast<uint8_t>(ss)};
                writeSetAck(r, h.onSetLocalTime(t));
                return;
            }

        case 'C':
            {
                // C<MM>/<DD>/<YY>
                unsigned mo, dd, yy;
                if (!c.digits(2, mo) || !c.match('/') || !c.digits(2, dd) || !c.match('/') || !c.digits(2, yy))
                {
                    writeChar(r, '0');
                    return;
                }
                MeadeLocalDate d;
                d.month = static_cast<uint8_t>(mo);
                d.day   = static_cast<uint8_t>(dd);
                d.year  = static_cast<uint16_t>(2000 + yy);
                writeSetLocalDateAck(r, h.onSetLocalDate(d));
                return;
            }

        default:
            writeChar(r, '0');
            return;
    }
}

}  // namespace meade
}  // namespace core
}  // namespace oat
