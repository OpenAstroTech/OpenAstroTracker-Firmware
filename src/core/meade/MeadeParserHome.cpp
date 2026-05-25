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

void handleMeadeHome(MeadeResponse &r, const char *suffix, IMeadeHomeHandlers &h)
{
    if (suffix == nullptr || suffix[0] == '\0' || suffix[1] != '\0')
    {
        return;
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
}

}  // namespace meade
}  // namespace core
}  // namespace oat
