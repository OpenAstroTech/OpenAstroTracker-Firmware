#include "./inc/Globals.hpp"
#include "../Configuration.hpp"
#include "Utility.hpp"
#include "RightAscension.hpp"

//////////////////////////////////////////////////////////////////////////////////////
//
// RightAscension — Arduino-dependent overlay methods
// Pure arithmetic is in core::RightAscension (src/core/types/RightAscension.hpp/.cpp)
//
// RA is 0–24h, always non-negative, wrapping at 24h.

char achBufRA[32];

const char *RightAscension::ToString() const
{
    char *p = achBufRA;
    int hours, mins, secs;
    getTime(hours, mins, secs);

    // RA is always non-negative
    int absHours = abs(hours);
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
    *p++ = '0' + (secs % 10);
    *p   = '\0';

    size_t used      = strlen(achBufRA);
    size_t remaining = sizeof(achBufRA) - used;
    if (remaining > 8)
    {
        String floatStr = String(this->getTotalHours(), 5);
        snprintf(achBufRA + used, remaining, " (%s)", floatStr.c_str());
    }
    return achBufRA;
}

RightAscension RightAscension::ParseFromMeade(String const &s)
{
    RightAscension result;
    int i = 0;
    LOG(DEBUG_MEADE, "[RA]: Parse Coord from [%s]", s.c_str());

    // RA never has a sign in Meade protocol
    // Degs can be 2 or 3 digits
    long degs = s[i++] - '0';
    degs      = degs * 10 + s[i++] - '0';

    // Third digit?
    if ((s[i] >= '0') && (s[i] <= '9'))
    {
        degs = degs * 10 + s[i++] - '0';
    }
    i++;  // Skip separator

    int mins = s.substring(i, i + 2).toInt();
    int secs = 0;
    if (int(s.length()) > i + 4)
    {
        secs = s.substring(i + 3, i + 5).toInt();
    }

    result.totalSeconds = ((degs * 60L + mins) * 60L) + secs;
    result.checkHours();

    LOG(DEBUG_MEADE, "[RA]: TotalSeconds are %l from %lh %dm %ds", result.totalSeconds, degs, mins, secs);
    return result;
}
