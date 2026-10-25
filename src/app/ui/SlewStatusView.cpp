#include "app/ui/SlewStatusView.hpp"

#include <math.h>

#include "ports/LogFormat.hpp"

namespace
{

/// Same cadence the legacy view used (`DISPLAY_UPDATE_TIME`); spelled out so `app/ui` needs no board
/// configuration on the host.
constexpr unsigned long kUpdateInterval = 200;

/// Room for "R 12:34:56 100%": the display truncates to its own width.
constexpr uint8_t kRowCapacity = 40;

/// A move counts as planned once it is longer than a thousandth of a step — the legacy threshold.
constexpr float kPlannedMoveEpsilon = 0.001F;

bool hasRaMove(ports::IMountReadModel &read)
{
    return fabsf(read.plannedRaSteps()) > kPlannedMoveEpsilon;
}

bool hasDecMove(ports::IMountReadModel &read)
{
    return fabsf(read.plannedDecSteps()) > kPlannedMoveEpsilon;
}

}  // namespace

namespace app
{

void SlewStatusView::render(ports::IMountReadModel &read, bool serialActive, ports::IDisplay &display)
{
    // The percentages the mount already computes for the progress bar; asking twice would be two
    // more axis reads for the same answer.
    int raPercent  = 100;
    int decPercent = 100;
    read.getStepperProgress(raPercent, decPercent);

    char row[kRowCapacity];

    if (hasRaMove(read) && hasDecMove(read))
    {
        OAT_SNPRINTF(row, sizeof(row), "R %s %d%%", read.raText(LCD_STRING | CURRENT_STRING), raPercent);
        display.writeLine(0, row);
        OAT_SNPRINTF(row, sizeof(row), "D%s %d%%", read.decText(LCD_STRING | CURRENT_STRING), decPercent);
        display.writeLine(1, row);
        return;
    }

    if (hasDecMove(read))
    {
        OAT_SNPRINTF(row, sizeof(row), "D%s %d%%", read.decText(LCD_STRING | CURRENT_STRING), decPercent);
        display.writeLine(1, row);
        return;
    }

    if (hasRaMove(read))
    {
        // A hand-driven move draws over the menu unless a serial host has taken the display.
        OAT_SNPRINTF(row, sizeof(row), "R %s %d%%", read.raText(LCD_STRING | CURRENT_STRING), raPercent);
        display.writeLine(serialActive ? 0 : 1, row);
        return;
    }

    if (serialActive)
    {
        OAT_SNPRINTF(row, sizeof(row), " RA: %s", read.raText(LCD_STRING | CURRENT_STRING));
        display.writeLine(0, row);
        OAT_SNPRINTF(row, sizeof(row), "DEC: %s", read.decText(LCD_STRING | CURRENT_STRING));
        display.writeLine(1, row);
        return;
    }

    OAT_SNPRINTF(row, sizeof(row), "R%s D%s", read.raText(COMPACT_STRING | CURRENT_STRING), read.decText(COMPACT_STRING | CURRENT_STRING));
    display.writeLine(1, row);
}

void SlewStatusView::renderThrottled(ports::IMountReadModel &read, bool serialActive, unsigned long now, ports::IDisplay &display)
{
    if ((now - _lastUpdate) > kUpdateInterval)
    {
        render(read, serialActive, display);
        _lastUpdate = now;
    }
}

}  // namespace app
