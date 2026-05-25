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

void handleMeadeGet(MeadeResponse &r, const char *s, IMeadeGetHandlers &h)
{
    if (!s || s[0] == '\0')
    {
        return;
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
                    return;
                case 'P':
                    writeCString(r, h.onProductName());
                    return;
                default:
                    return;
            }
        }
        if (s[0] == 'I')
        {
            switch (s[1])
            {
                case 'S':
                    writeBool01(r, h.onIsSlewing());
                    return;
                case 'T':
                    writeBool01(r, h.onIsTracking());
                    return;
                case 'G':
                    writeBool01(r, h.onIsGuiding());
                    return;
                default:
                    return;
            }
        }
        return;
    }

    // Single-character commands.
    if (s[1] != '\0')
    {
        return;
    }

    switch (s[0])
    {
        case 'R':
            writeRa(r, h.onCurrentRa());
            return;
        case 'r':
            writeRa(r, h.onTargetRa());
            return;
        case 'D':
            writeDec(r, h.onCurrentDec());
            return;
        case 'd':
            writeDec(r, h.onTargetDec());
            return;
        case 'X':
            writeCString(r, h.onMountStatus());
            return;
        case 't':
            writeLatitude(r, h.onSiteLatitude());
            return;
        case 'g':
            writeLongitude(r, h.onSiteLongitude());
            return;
        case 'G':
            writeUtcOffset(r, h.onUtcOffset());
            return;
        case 'a':
            writeTime12h(r, h.onLocalTime());
            return;
        case 'L':
            writeTime24h(r, h.onLocalTime());
            return;
        case 'C':
            writeLocalDate(r, h.onLocalDate());
            return;
        case 'c':
            writeClockFormat(r, h.onClockFormat());
            return;
        case 'T':
            writeTrackingRate(r, h.onTrackingRate());
            return;
        case 'M':
            writeCString(r, h.onSiteName(1));
            return;
        case 'N':
            writeCString(r, h.onSiteName(2));
            return;
        case 'O':
            writeCString(r, h.onSiteName(3));
            return;
        case 'P':
            writeCString(r, h.onSiteName(4));
            return;
        default:
            return;
    }
}

}  // namespace meade
}  // namespace core
}  // namespace oat
