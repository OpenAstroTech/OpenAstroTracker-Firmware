/**
 * @file MeadeParserSlewRate.cpp
 * @brief SetSlewRate-family (`:R...`) dispatcher for the Meade LX200 parser.
 */

#include "MeadeParser.hpp"

namespace oat
{
namespace core
{
namespace meade
{

MeadeResponse handleMeadeSetSlewRate(const char *suffix, IMeadeSlewRateHandlers &h)
{
    MeadeResponse r;
    if (suffix == nullptr || suffix[0] == '\0' || suffix[1] != '\0')
    {
        return r;
    }
    switch (suffix[0])
    {
        case 'S':
            h.onSetSlewRate(4);
            break;
        case 'M':
            h.onSetSlewRate(3);
            break;
        case 'C':
            h.onSetSlewRate(2);
            break;
        case 'G':
            h.onSetSlewRate(1);
            break;
        default:
            break;
    }
    return r;
}

}  // namespace meade
}  // namespace core
}  // namespace oat
