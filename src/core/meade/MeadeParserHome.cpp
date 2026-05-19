/**
 * @file MeadeParserHome.cpp
 * @brief Home-family (`:h...`) dispatcher for the Meade LX200 parser.
 */

#include "MeadeParser.hpp"
#include "MeadeParserHelpers.hpp"

namespace oat
{
namespace core
{
namespace meade
{

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

}  // namespace meade
}  // namespace core
}  // namespace oat
