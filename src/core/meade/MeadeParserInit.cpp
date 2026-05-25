/**
 * @file MeadeParserInit.cpp
 * @brief Init-family (`:I...`) dispatcher for the Meade LX200 parser.
 */

#include "MeadeParser.hpp"

namespace oat
{
namespace core
{
namespace meade
{

void handleMeadeInit(MeadeResponse &r, const char *, IMeadeInitHandlers &h)
{
    h.onEnterSerialControl();
    (void) r;  // response stays empty
}

}  // namespace meade
}  // namespace core
}  // namespace oat
