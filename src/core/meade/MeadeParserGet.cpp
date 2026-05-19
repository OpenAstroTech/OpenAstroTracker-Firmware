/**
 * @file MeadeParserGet.cpp
 * @brief Get-family (`:G...`) dispatcher for the Meade LX200 parser.
 */

#include "MeadeParser.hpp"
#include "MeadeParserHelpers.hpp"

namespace oat
{
namespace core
{
namespace meade
{

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

}  // namespace meade
}  // namespace core
}  // namespace oat
