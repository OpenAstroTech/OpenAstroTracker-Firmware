/**
 * @file MeadeParserDistance.cpp
 * @brief Distance-family (`:D...`) dispatcher for the Meade LX200 parser.
 */

#include "MeadeParser.hpp"
#include "MeadeParserHelpers.hpp"

namespace oat
{
namespace core
{
namespace meade
{

MeadeResponse handleMeadeDistance(const char *, IMeadeDistanceHandlers &h)
{
    MeadeResponse r;
    writeChar(r, h.onIsSlewingRaOrDec() ? '|' : ' ');
    writeTerminator(r);
    return r;
}

}  // namespace meade
}  // namespace core
}  // namespace oat
