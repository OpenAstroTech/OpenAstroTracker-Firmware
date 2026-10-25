#include "core/menu/MenuLine.hpp"

#include <string.h>

namespace
{

/// Length of one item as it appears in the concatenated menu string, markers included.
size_t segmentLength(const core::MenuEntry &entry)
{
    return 1 + strlen(entry.display) + 1;
}

/// The character at `index` in the concatenated menu string, markers included.
char menuCharAt(const core::MenuModel &menu, size_t index)
{
    size_t base = 0;
    for (uint8_t i = 0; i < menu.count(); i++)
    {
        const core::MenuEntry *entry = menu.at(i);
        const size_t length          = segmentLength(*entry);
        if (index < base + length)
        {
            const size_t local = index - base;
            const bool active  = (i == menu.activeIndex());
            if (local == 0)
            {
                return active ? '>' : ' ';
            }
            if (local == length - 1)
            {
                return active ? '<' : ' ';
            }
            return entry->display[local - 1];
        }
        base += length;
    }
    return ' ';
}

}  // namespace

namespace core
{

void composeMenuRow(char *out, size_t capacity, const MenuModel &menu, uint8_t columns)
{
    if (out == nullptr || capacity == 0)
    {
        return;
    }
    if (columns == 0)
    {
        out[0] = '\0';
        return;
    }

    // The last column is left empty: the tracking indicator is painted there.
    const size_t usable = (columns > 1) ? static_cast<size_t>(columns - 1) : 0;

    size_t total        = 0;
    size_t activeOffset = 0;
    for (uint8_t i = 0; i < menu.count(); i++)
    {
        if (i == menu.activeIndex())
        {
            activeOffset = total;
        }
        total += segmentLength(*menu.at(i));
    }

    const int margin = (static_cast<int>(usable) - static_cast<int>(menu.longestDisplay())) / 2;
    int source       = static_cast<int>(activeOffset) - margin;

    size_t written     = 0;
    const size_t limit = (capacity - 1 < static_cast<size_t>(columns)) ? capacity - 1 : static_cast<size_t>(columns);

    while (source < 0 && written < limit)
    {
        out[written++] = ' ';
        source++;
    }

    while (written < usable && written < limit && static_cast<size_t>(source) < total)
    {
        out[written++] = menuCharAt(menu, static_cast<size_t>(source));
        source++;
    }

    while (written < limit)
    {
        out[written++] = ' ';
    }
    out[written] = '\0';
}

}  // namespace core
