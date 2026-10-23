#include "CoordinateText.hpp"

namespace core
{

namespace
{
/// Indexed by `CoordinateFormat`; index 0 is the "" the callers use for "no format".
const char *const kRaFormats[] = {
    "",
    " %02dh %02dm %02ds",  // LcdMenu
    "%02d:%02d:%02d#",     // Meade
    "%02dh %02dm %02ds",   // Print
    "%02dh%02dm%02ds",     // Lcd
    "%02d%02d%02d",        // Compact
};

const char *const kDecFormats[] = {
    "",
    " {d}@ {m}' {s}\"",  // LcdMenu
    "{d}*{m}'{s}#",      // Meade
    "{d} {m}'{s}\"",     // Print
    "{d}@{m}'{s}\"",     // Lcd
    "{d}{m}{s}",         // Compact
};
}  // namespace

const char *raFormat(CoordinateFormat format)
{
    const uint8_t index = static_cast<uint8_t>(format);
    if (index >= (sizeof(kRaFormats) / sizeof(kRaFormats[0])))
    {
        return kRaFormats[0];
    }
    return kRaFormats[index];
}

const char *decFormat(CoordinateFormat format)
{
    const uint8_t index = static_cast<uint8_t>(format);
    if (index >= (sizeof(kDecFormats) / sizeof(kDecFormats[0])))
    {
        return kDecFormats[0];
    }
    return kDecFormats[index];
}

void insertLcdMenuCursor(char *buffer, uint8_t active, bool hasSignColumn)
{
    if (buffer == nullptr)
    {
        return;
    }

    const uint8_t offset          = hasSignColumn && active > 0 ? 1 : 0;
    buffer[(active * 4) + offset] = '>';
}

}  // namespace core
