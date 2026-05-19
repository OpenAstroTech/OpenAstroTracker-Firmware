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

MeadeResponse handleMeadeGps(const char *suffix, IMeadeGpsHandlers &h)
{
    MeadeResponse r;
    if (suffix == nullptr || suffix[0] != 'T')
    {
        writeChar(r, '0');
        return r;
    }
    const bool acquired = h.onStartGpsAcquisition(suffix + 1);
    writeChar(r, acquired ? '1' : '0');
    return r;
}

}  // namespace meade
}  // namespace core
}  // namespace oat
