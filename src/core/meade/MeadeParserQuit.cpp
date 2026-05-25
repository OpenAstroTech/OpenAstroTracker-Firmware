/**
 * @file MeadeParserQuit.cpp
 * @brief Quit-family (`:Q...`) dispatcher for the Meade LX200 parser.
 */

#include "MeadeParser.hpp"

namespace oat
{
namespace core
{
namespace meade
{

void handleMeadeQuit(MeadeResponse &r, const char *suffix, IMeadeQuitHandlers &h)
{
    (void) r;  // response stays empty
    if (suffix == nullptr)
    {
        return;
    }

    // Empty suffix == :Q# == StopAll.
    if (suffix[0] == '\0')
    {
        h.onStopAll();
        return;
    }

    // All remaining variants are a single character.
    if (suffix[1] != '\0')
    {
        return;
    }

    switch (suffix[0])
    {
        case 'a':
            h.onStopDirectionalAll();
            break;
        case 'e':
            h.onStopEast();
            break;
        case 'w':
            h.onStopWest();
            break;
        case 'n':
            h.onStopNorth();
            break;
        case 's':
            h.onStopSouth();
            break;
        case 'q':
            h.onQuitControlMode();
            break;
        default:
            break;
    }
}

}  // namespace meade
}  // namespace core
}  // namespace oat
