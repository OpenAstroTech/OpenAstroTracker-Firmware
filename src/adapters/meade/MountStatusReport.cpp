#include "MountStatusReport.hpp"

#include <stdio.h>

namespace adapters
{
namespace meade
{

bool formatMountStatus(char *buffer, size_t size, const MountStatusFacts &facts)
{
    // Position 0: RA, 1: DEC, 2: tracking, 3: AZ, 4: ALT, 5: focus. Upper case means the axis is
    // running towards a lower position, lower case towards a higher one; '-' means idle.
    char glyphs[7] = "------";
    if (facts.raRunning)
    {
        glyphs[0] = facts.raTargetBelowCurrent ? 'R' : 'r';
    }
    if (facts.decRunning)
    {
        glyphs[1] = facts.decTargetBelowCurrent ? 'D' : 'd';
    }
    if (facts.trackingAxisRunning || facts.trackingOnly)
    {
        glyphs[2] = 'T';
    }
    if (facts.azRunning)
    {
        glyphs[3] = facts.azTargetBelowCurrent ? 'Z' : 'z';
    }
    if (facts.altRunning)
    {
        glyphs[4] = facts.altTargetBelowCurrent ? 'A' : 'a';
    }
    if (facts.focusRunning)
    {
        glyphs[5] = facts.focusTargetBelowCurrent ? 'F' : 'f';
    }

    char focus[16] = "";
    if (facts.hasFocus)
    {
        snprintf(focus, sizeof(focus), "%ld", facts.focusSteps);
    }

    const int written = snprintf(buffer,
                                 size,
                                 "%s,%s,%ld,%ld,%ld,%s,%s,%s,",
                                 facts.activity,
                                 glyphs,
                                 facts.raSteps,
                                 facts.decSteps,
                                 facts.trkSteps,
                                 facts.raText,
                                 facts.decText,
                                 focus);
    return written > 0 && static_cast<size_t>(written) < size;
}

}  // namespace meade
}  // namespace adapters
