/**
 * @file MeadeParserFocus.cpp
 * @brief Focus-family (`:F...`) dispatcher for the Meade LX200 parser.
 */

#include "MeadeParser.hpp"
#include "MeadeParserHelpers.hpp"

#include <stdlib.h>

namespace oat
{
namespace core
{
namespace meade
{

void handleMeadeFocus(MeadeResponse &r, const char *suffix, IMeadeFocusHandlers &h)
{
    if (suffix == nullptr || suffix[0] == '\0')
    {
        return;
    }

    Cursor c(suffix);

    // `:F1#` .. `:F4#` — speed-by-rate digits act as the whole input.
    if (c.peek() >= '1' && c.peek() <= '4')
    {
        c.match(c.peek());
        if (c.atEnd())
        {
            h.onFocusSetSpeedByRate(suffix[0] - '0');
            return;
        }
    }
    switch (c.peek())
    {
        case '+':
            h.onFocusContinuousIn();
            break;
        case '-':
            h.onFocusContinuousOut();
            break;
        case 'M':
            c.match('M');
            h.onFocusMoveBy(strtol(c.remaining(), nullptr, 10));
            break;
        case 'F':
            h.onFocusSetSpeedByRate(4);
            break;
        case 'S':
            h.onFocusSetSpeedByRate(1);
            break;
        case 'p':
            fillLongResponse(r, h.onFocusGetPosition());
            return;
        case 'P':
            if (h.onFocusIsAvailable())
            {
                c.match('P');
                h.onFocusSetPosition(strtol(c.remaining(), nullptr, 10));
                fillSetSuccessResponse(r, true);
                return;
            }
            break;
        case 'B':
            fillSetSuccessResponse(r, h.onFocusGetState());
            return;
        case 'Q':
            h.onFocusStop();
            break;
        default:
            break;
    }
}

}  // namespace meade
}  // namespace core
}  // namespace oat
