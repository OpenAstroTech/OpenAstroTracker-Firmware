/**
 * @file MeadeParserMovement.cpp
 * @brief Movement-family (`:M...`) dispatcher for the Meade LX200 parser.
 */

#include "MeadeParser.hpp"
#include "MeadeParserHelpers.hpp"

#include <ctype.h>
#include <stdlib.h>

namespace oat
{
namespace core
{
namespace meade
{

void handleMeadeMovement(MeadeResponse &r, const char *suffix, IMeadeMovementHandlers &h)
{
    if (suffix == nullptr || suffix[0] == '\0')
    {
        return;
    }

    Cursor c(suffix);

    // `:MS#` — exact match only ("S123" is not a slew-to-target).
    if (c.peek() == 'S' && c.match('S') && c.atEnd())
    {
        h.onStartSlewToTarget();
        fillSetSuccessResponse(r, false);
        return;
    }

    // `:MAA...#` — any input starting with "AA" requests a home move.
    if (c.peek() == 'A' && c.match('A'))
    {
        if (c.peek() == 'A')
        {
            c.match('A');
            h.onMoveAzAltHome();
            fillSetSuccessResponse(r, true);
            return;
        }
        if (c.peek() == 'Z')
        {
            c.match('Z');
            const float arcMinutes = static_cast<float>(strtod(c.remaining(), nullptr));
            h.onMoveAzimuth(arcMinutes);
            return;
        }
        if (c.peek() == 'L')
        {
            c.match('L');
            const float arcMinutes = static_cast<float>(strtod(c.remaining(), nullptr));
            h.onMoveAltitude(arcMinutes);
            return;
        }
        return;
    }

    // `:MHR<R|L>[distance]#` / `:MHD<U|D>[distance]#` — Hall-sensor auto-home.
    if (c.peek() == 'H' && c.match('H'))
    {
        if (c.peek() == 'R' && c.match('R'))
        {
            int direction = 0;
            char d        = c.peek();
            if (d == 'R')
                direction = -1;
            else if (d == 'L')
                direction = 1;
            if (direction != 0)
                c.match(d);
            if (direction == 0)
            {
                fillSetSuccessResponse(r, false);
                return;
            }
            fillSetSuccessResponse(r, h.onHomeRa(direction, c.remaining()));
            return;
        }
        if (c.peek() == 'D' && c.match('D'))
        {
            int direction = 0;
            char d        = c.peek();
            if (d == 'U')
                direction = 1;
            else if (d == 'D')
                direction = -1;
            if (direction != 0)
                c.match(d);
            if (direction == 0)
            {
                fillSetSuccessResponse(r, false);
                return;
            }
            fillSetSuccessResponse(r, h.onHomeDec(direction, c.remaining()));
            return;
        }
        return;
    }

    // `:MT1#` / `:MT0#` — tracking toggle.
    if (c.peek() == 'T' && c.match('T'))
    {
        if (c.peek() == '1')
        {
            c.match('1');
            h.onTrackingOn();
            fillSetSuccessResponse(r, true);
            return;
        }
        if (c.peek() == '0')
        {
            c.match('0');
            h.onTrackingOff();
            fillSetSuccessResponse(r, true);
            return;
        }
        fillSetSuccessResponse(r, false);
        return;
    }

    // `:MG<dir><DDDD>#` / `:Mg<dir><DDDD>#` — guide pulse.
    if ((c.peek() == 'G' || c.peek() == 'g') && c.match(c.peek()))
    {
        MoveDirection dir = MoveDirection::East;
        const char dc     = static_cast<char>(tolower(static_cast<unsigned char>(c.peek())));
        if (dc == 'n')
            dir = MoveDirection::North;
        else if (dc == 's')
            dir = MoveDirection::South;
        else if (dc == 'w')
            dir = MoveDirection::West;
        c.match(c.peek());
        unsigned d = 0;
        if (c.digits(4, d) && c.atEnd())
        {
            h.onGuidePulse(dir, static_cast<int>(d));
            fillLiteralResponse(r, "");
            return;
        }
        fillLiteralResponse(r, "0");
        return;
    }

    // `:MX<axis><steps>#` — move a single stepper by raw step count.
    if (c.peek() == 'X' && c.match('X'))
    {
        MovementAxis axis;
        switch (c.peek())
        {
            case 'r':
                axis = MovementAxis::Ra;
                break;
            case 'd':
                axis = MovementAxis::Dec;
                break;
            case 'z':
                axis = MovementAxis::Azimuth;
                break;
            case 'l':
                axis = MovementAxis::Altitude;
                break;
            case 'f':
                axis = MovementAxis::Focus;
                break;
            default:
                fillSetSuccessResponse(r, false);
                return;
        }
        c.match(c.peek());
        const long steps = strtol(c.remaining(), nullptr, 10);
        h.onMoveStepper(axis, steps);
        fillSetSuccessResponse(r, true);
        return;
    }

    // Continuous slew shortcuts — single direction letter, anything after ignored.
    switch (c.peek())
    {
        case 'e':
            h.onSlewEast();
            return;
        case 'w':
            h.onSlewWest();
            return;
        case 'n':
            h.onSlewNorth();
            return;
        case 's':
            h.onSlewSouth();
            return;
        default:
            return;
    }
}

}  // namespace meade
}  // namespace core
}  // namespace oat
