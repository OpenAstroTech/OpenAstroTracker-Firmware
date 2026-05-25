/**
 * @file MeadeParserGps.cpp
 * @brief GPS-family (`:g...`) dispatcher for the Meade LX200 parser.
 */

#include "MeadeParser.hpp"
#include "MeadeParserHelpers.hpp"

namespace oat
{
namespace core
{
namespace meade
{

void handleMeadeGps(MeadeResponse &r, const char *suffix, IMeadeGpsHandlers &h)
{
    if (suffix == nullptr || suffix[0] != 'T')
    {
        writeChar(r, '0');
        return;
    }
    const bool acquired = h.onStartGpsAcquisition(suffix + 1);
    writeChar(r, acquired ? '1' : '0');
}

}  // namespace meade
}  // namespace core
}  // namespace oat
