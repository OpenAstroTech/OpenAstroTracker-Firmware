/**
 * @file MeadeParser.cpp
 * @brief Top-level entry points for the Meade LX200 command parser.
 *
 * Contains `parseMeadeCommand` (classifier) and `dispatchMeadeCommand`
 * (unified parse + dispatch). Family-specific handlers live in their own
 * files (MeadeParserGet.cpp, MeadeParserSet.cpp, etc.).
 */

#include "MeadeParser.hpp"
#include "MeadeParserHelpers.hpp"

#include <stddef.h>

namespace oat
{
namespace core
{
namespace meade
{

// ---------------------------------------------------------------------------
// Top-level parser
// ---------------------------------------------------------------------------
bool parseMeadeCommand(const char *input, MeadeParseResult &result)
{
    if (input == nullptr || input[0] != ':')
    {
        return false;
    }

    // Copy the input into a stack buffer with whitespace stripped and the
    // optional trailing `#` removed. Using a fixed-capacity char buffer keeps
    // the parser usable on bare AVR builds that lack libstdc++ (`std::string`).
    char normalized[MeadeResponse::Capacity];
    size_t nlen = 0;
    for (const char *cursor = input; *cursor != '\0' && nlen + 1 < sizeof(normalized); ++cursor)
    {
        if (*cursor != ' ')
        {
            normalized[nlen++] = *cursor;
        }
    }
    normalized[nlen] = '\0';

    if (nlen < 2)
    {
        return false;
    }

    if (normalized[nlen - 1] == '#')
    {
        --nlen;
        normalized[nlen] = '\0';
    }

    if (nlen < 2)
    {
        return false;
    }

    const char family = normalized[1];
    switch (family)
    {
        case 'S':
        case 'M':
        case 'G':
        case 'g':
        case 'C':
        case 'h':
        case 'I':
        case 'Q':
        case 'R':
        case 'D':
        case 'X':
        case 'F':
            result.valid  = true;
            result.family = family;
            result.payload.assign(normalized + 2);
            return true;
        default:
            return false;
    }
}

// ---------------------------------------------------------------------------
// Unified dispatch — parse + classify + dispatch in one call
// ---------------------------------------------------------------------------
void dispatchMeadeCommand(MeadeResponse &r, const char *input, IMeadeHandlers &h)
{
    r.clear();
    MeadeParseResult parsed;
    if (!parseMeadeCommand(input, parsed))
    {
        return;
    }

    switch (parsed.family)
    {
        case 'S':
            handleMeadeSet(r, parsed.payload.c_str(), h);
            break;
        case 'M':
            handleMeadeMovement(r, parsed.payload.c_str(), h);
            break;
        case 'G':
            handleMeadeGet(r, parsed.payload.c_str(), h);
            break;
        case 'g':
            handleMeadeGps(r, parsed.payload.c_str(), h);
            break;
        case 'C':
            handleMeadeSyncControl(r, parsed.payload.c_str(), h);
            break;
        case 'h':
            handleMeadeHome(r, parsed.payload.c_str(), h);
            break;
        case 'I':
            handleMeadeInit(r, parsed.payload.c_str(), h);
            break;
        case 'Q':
            handleMeadeQuit(r, parsed.payload.c_str(), h);
            break;
        case 'R':
            handleMeadeSetSlewRate(r, parsed.payload.c_str(), h);
            break;
        case 'D':
            handleMeadeDistance(r, parsed.payload.c_str(), h);
            break;
        case 'X':
            handleMeadeExtra(r, parsed.payload.c_str(), h);
            break;
        case 'F':
            handleMeadeFocus(r, parsed.payload.c_str(), h);
            break;
        default:
            break;
    }
}

}  // namespace meade
}  // namespace core
}  // namespace oat
