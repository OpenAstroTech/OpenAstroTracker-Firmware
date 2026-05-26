#include "inc/Globals.hpp"
#include "../Configuration.hpp"
#include "Utility.hpp"
#include "DayTime.hpp"

///////////////////////////////////
// DayTime — Arduino-dependent overlay methods
// Pure arithmetic is in core::DayTime (src/core/types/DayTime.hpp/.cpp)

// Parses the RA or DEC from a string that has an optional sign, a two digit degree, a seperator, a two digit minute, a seperator and a two digit second.
// Does not correct for hemisphere (derived class Declination takes care of that)
// For example:   -45*32:11 or 23:44:22
DayTime DayTime::ParseFromMeade(String const &s)
{
    DayTime result;
    int i    = 0;
    long sgn = 1;
    LOG(DEBUG_MEADE, "[DAYTIME]: Parse Coord from [%s]", s.c_str());
    // Check whether we have a sign. This should be able to parse RA and DEC strings (RA never has a sign, and DEC should always have one).
    if ((s[i] == '-') || (s[i] == '+'))
    {
        sgn = s[i] == '-' ? -1 : +1;
        i++;
    }

    // Degs can be 2 or 3 digits
    long degs = s[i++] - '0';
    LOG(DEBUG_MEADE, "[DAYTIME]: 1st digit [%c] -> degs=%l", s[i - 1], degs);
    degs = degs * 10 + s[i++] - '0';
    LOG(DEBUG_MEADE, "[DAYTIME]: 2nd digit [%c] -> degs=%l", s[i - 1], degs);

    // Third digit?
    if ((s[i] >= '0') && (s[i] <= '9'))
    {
        degs = degs * 10 + s[i++] - '0';
        LOG(DEBUG_MEADE, "[DAYTIME]: 3rd digit [%c] -> degs=%d", s[i - 1], degs);
    }
    i++;  // Skip seperator

    int mins = s.substring(i, i + 2).toInt();
    LOG(DEBUG_MEADE, "[DAYTIME]: Minutes are [%s] -> mins=%d", s.substring(i, i + 2).c_str(), mins);
    int secs = 0;
    if (int(s.length()) > i + 4)
    {
        secs = s.substring(i + 3, i + 5).toInt();
        LOG(DEBUG_MEADE, "[DAYTIME]: Seconds are [%s] -> secs=%d", s.substring(i + 3, i + 5).c_str(), secs);
    }
    else
    {
        LOG(DEBUG_MEADE, "[DAYTIME]: No Seconds. slen %d is not > %d", s.length(), i + 4);
    }
    // Get the signed total seconds specified....
    result.totalSeconds = sgn * (((degs * 60L + mins) * 60L) + secs);

    LOG(DEBUG_MEADE, "[DAYTIME]: TotalSeconds are %l from %lh %dm %ds", result.totalSeconds, degs, mins, secs);

    return result;
}

char achBuf[32];

// Convert to a standard string (like 14:45:06)
const char *DayTime::ToString() const
{
    char *p = achBuf;
    int hours, mins, secs;
    getTime(hours, mins, secs);

    int absHours = abs(hours);
    if (totalSeconds < 0)
    {
        *p++ = '-';
    }
    if (absHours < 10)
    {
        *p++ = '0';
    }
    else
    {
        *p++ = '0' + (absHours / 10);
    }

    *p++ = '0' + (absHours % 10);

    *p++ = ':';
    if (mins < 10)
    {
        *p++ = '0';
    }
    else
    {
        *p++ = '0' + (mins / 10);
    }

    *p++ = '0' + (mins % 10);
    *p++ = ':';
    if (secs < 10)
    {
        *p++ = '0';
    }
    else
    {
        *p++ = '0' + (secs / 10);
    }

    *p++             = '0' + (secs % 10);
    size_t used      = p - achBuf;
    size_t remaining = sizeof(achBuf) - used;
    String floatStr  = String(this->getTotalHours(), 5);
    snprintf(p, remaining, " (%s)", floatStr.c_str());
    return achBuf;
}

// ParseFromMeade, formatString, formatStringImpl, printTwoDigits
// were previously here. formatString/formatStringImpl/printTwoDigits
// are now in core::DayTime (pure).
