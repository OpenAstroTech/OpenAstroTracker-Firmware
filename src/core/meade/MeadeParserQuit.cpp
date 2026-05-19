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

MeadeResponse handleMeadeQuit(const char *suffix, IMeadeQuitHandlers &h)
{
    MeadeResponse r;
    if (suffix == nullptr)
    {
        return r;
    }

    // Empty suffix == :Q# == StopAll.
    if (suffix[0] == '\0')
    {
        h.onStopAll();
        return r;
    }

    // All remaining variants are a single character.
    if (suffix[1] != '\0')
    {
        return r;
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
    return r;
}

}  // namespace meade
}  // namespace core
}  // namespace oat
