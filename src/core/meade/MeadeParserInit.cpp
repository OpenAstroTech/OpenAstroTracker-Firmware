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

MeadeResponse handleMeadeInit(const char *, IMeadeInitHandlers &h)
{
    h.onEnterSerialControl();
    return MeadeResponse {};
}

}  // namespace meade
}  // namespace core
}  // namespace oat
