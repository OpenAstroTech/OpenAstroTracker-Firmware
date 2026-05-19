/**
 * @file MeadeParser.cpp
 * @brief Top-level entry points for the Meade LX200 command parser.
 *
 * Contains `parseMeadeCommand` (classifier) and `dispatchMeadeCommand`
 * (unified parse + dispatch). Family-specific handlers live in their own
 * files (MeadeParserGet.cpp, MeadeParserSet.cpp, etc.).
 */

#include "core/MeadeParser.hpp"
#include "core/MeadeParserHelpers.hpp"

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
MeadeParseResult parseMeadeCommand(const char *input)
{
    MeadeParseResult result;
    if (input == nullptr || input[0] != ':')
    {
        return result;
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
        return result;
    }

    if (normalized[nlen - 1] == '#')
    {
        --nlen;
        normalized[nlen] = '\0';
    }

    if (nlen < 2)
    {
        return result;
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
            return result;
        default:
            return result;
    }
}

// ---------------------------------------------------------------------------
// Unified dispatch — parse + classify + dispatch in one call
// ---------------------------------------------------------------------------
MeadeResponse dispatchMeadeCommand(const char *input, IMeadeHandlers &h)
{
    MeadeParseResult parsed = parseMeadeCommand(input);
    if (!parsed.valid)
    {
        return MeadeResponse {};
    }

    switch (parsed.family)
    {
        case 'S':
            return handleMeadeSet(parsed.payload.c_str(), h);
        case 'M':
            return handleMeadeMovement(parsed.payload.c_str(), h);
        case 'G':
            return handleMeadeGet(parsed.payload.c_str(), h);
        case 'g':
            return handleMeadeGps(parsed.payload.c_str(), h);
        case 'C':
            return handleMeadeSyncControl(parsed.payload.c_str(), h);
        case 'h':
            return handleMeadeHome(parsed.payload.c_str(), h);
        case 'I':
            return handleMeadeInit(parsed.payload.c_str(), h);
        case 'Q':
            return handleMeadeQuit(parsed.payload.c_str(), h);
        case 'R':
            return handleMeadeSetSlewRate(parsed.payload.c_str(), h);
        case 'D':
            return handleMeadeDistance(parsed.payload.c_str(), h);
        case 'X':
            return handleMeadeExtra(parsed.payload.c_str(), h);
        case 'F':
            return handleMeadeFocus(parsed.payload.c_str(), h);
        default:
            return MeadeResponse {};
    }
}

}  // namespace meade
}  // namespace core
}  // namespace oat
