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

void handleMeadeDistance(MeadeResponse &r, const char *, IMeadeDistanceHandlers &h)
{
    writeChar(r, h.onIsSlewingRaOrDec() ? '|' : ' ');
    writeTerminator(r);
}

}  // namespace meade
}  // namespace core
}  // namespace oat
