/**
 * @file MeadeParserSyncControl.cpp
 * @brief SyncControl-family (`:C...`) dispatcher for the Meade LX200 parser.
 */

#include "MeadeParser.hpp"
#include "MeadeParserHelpers.hpp"

namespace oat
{
namespace core
{
namespace meade
{

MeadeResponse handleMeadeSyncControl(const char *suffix, IMeadeSyncControlHandlers &h)
{
    MeadeResponse r;
    if (suffix != nullptr && suffix[0] == 'M' && suffix[1] == '\0')
    {
        h.onSyncToTarget();
        writeCString(r, "NONE");
    }
    else
    {
        writeCString(r, "FAIL");
    }
    return r;
}

}  // namespace meade
}  // namespace core
}  // namespace oat
