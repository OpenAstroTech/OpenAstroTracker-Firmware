/**
 * @file MeadeParserMovement.cpp
 * @brief Movement-family (`:M...`) dispatcher for the Meade LX200 parser.
 */

#include "core/MeadeParser.hpp"
#include "core/MeadeParserHelpers.hpp"

#include <ctype.h>
#include <stdlib.h>

namespace oat
{
namespace core
{
namespace meade
{

MeadeResponse handleMeadeMovement(const char *suffix, IMeadeMovementHandlers &h)
{
    MeadeResponse r;
    if (suffix == nullptr || suffix[0] == '\0')
    {
        return r;
    }

    Cursor c(suffix);

    // `:MS#` — exact match only ("S123" is not a slew-to-target).
    if (c.peek() == 'S' && c.match('S') && c.atEnd())
    {
        h.onStartSlewToTarget();
        return makeSetSuccessResponse(false);
    }

    // `:MAA...#` — any input starting with "AA" requests a home move.
    if (c.peek() == 'A' && c.match('A'))
    {
        if (c.peek() == 'A')
        {
            c.match('A');
            h.onMoveAzAltHome();
            return makeSetSuccessResponse(true);
        }
        if (c.peek() == 'Z')
        {
            c.match('Z');
            const float arcMinutes = static_cast<float>(strtod(c.remaining(), nullptr));
            h.onMoveAzimuth(arcMinutes);
            return r;
        }
        if (c.peek() == 'L')
        {
            c.match('L');
            const float arcMinutes = static_cast<float>(strtod(c.remaining(), nullptr));
            h.onMoveAltitude(arcMinutes);
            return r;
        }
        return r;
    }

    // `:MHR<R|L>[distance]#` / `:MHD<U|D>[distance]#` — Hall-sensor auto-home.
    if (c.peek() == 'H' && c.match('H'))
    {
        if (c.peek() == 'R' && c.match('R'))
        {
            int direction = 0;
            char d = c.peek();
            if (d == 'R') direction = -1;
            else if (d == 'L') direction = 1;
            if (direction != 0) c.match(d);
            if (direction == 0) return makeSetSuccessResponse(false);
            return makeSetSuccessResponse(h.onHomeRa(direction, c.remaining()));
        }
        if (c.peek() == 'D' && c.match('D'))
        {
            int direction = 0;
            char d = c.peek();
            if (d == 'U') direction = 1;
            else if (d == 'D') direction = -1;
            if (direction != 0) c.match(d);
            if (direction == 0) return makeSetSuccessResponse(false);
            return makeSetSuccessResponse(h.onHomeDec(direction, c.remaining()));
        }
        return r;
    }

    // `:MT1#` / `:MT0#` — tracking toggle.
    if (c.peek() == 'T' && c.match('T'))
    {
        if (c.peek() == '1') { c.match('1'); h.onTrackingOn(); return makeSetSuccessResponse(true); }
        if (c.peek() == '0') { c.match('0'); h.onTrackingOff(); return makeSetSuccessResponse(true); }
        return makeSetSuccessResponse(false);
    }

    // `:MG<dir><DDDD>#` / `:Mg<dir><DDDD>#` — guide pulse.
    if ((c.peek() == 'G' || c.peek() == 'g') && c.match(c.peek()))
    {
        MoveDirection dir = MoveDirection::East;
        const char dc = static_cast<char>(tolower(static_cast<unsigned char>(c.peek())));
        if (dc == 'n') dir = MoveDirection::North;
        else if (dc == 's') dir = MoveDirection::South;
        else if (dc == 'w') dir = MoveDirection::West;
        c.match(c.peek());
        unsigned d = 0;
        if (c.digits(4, d) && c.atEnd())
        {
            h.onGuidePulse(dir, static_cast<int>(d));
            return makeLiteralResponse("");
        }
        return makeLiteralResponse("0");
    }

    // `:MX<axis><steps>#` — move a single stepper by raw step count.
    if (c.peek() == 'X' && c.match('X'))
    {
        MovementAxis axis;
        switch (c.peek())
        {
            case 'r': axis = MovementAxis::Ra; break;
            case 'd': axis = MovementAxis::Dec; break;
            case 'z': axis = MovementAxis::Azimuth; break;
            case 'l': axis = MovementAxis::Altitude; break;
            case 'f': axis = MovementAxis::Focus; break;
            default: return makeSetSuccessResponse(false);
        }
        c.match(c.peek());
        const long steps = strtol(c.remaining(), nullptr, 10);
        h.onMoveStepper(axis, steps);
        return makeSetSuccessResponse(true);
    }

    // Continuous slew shortcuts — single direction letter, anything after ignored.
    switch (c.peek())
    {
        case 'e': h.onSlewEast(); return r;
        case 'w': h.onSlewWest(); return r;
        case 'n': h.onSlewNorth(); return r;
        case 's': h.onSlewSouth(); return r;
        default:  return r;
    }
}

}  // namespace meade
}  // namespace core
}  // namespace oat
