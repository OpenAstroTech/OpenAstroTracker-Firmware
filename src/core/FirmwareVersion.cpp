#include "FirmwareVersion.hpp"

namespace core
{

namespace
{
/// Reads the next run of decimal digits, leaving the cursor on the first non-digit.
int parseComponent(const char *&cursor)
{
    int value = 0;
    while (*cursor >= '0' && *cursor <= '9')
    {
        value = (value * 10) + (*cursor - '0');
        ++cursor;
    }
    return value;
}

/// Skips a separator if one is there, so "1.13.22" and "1-13-22" both parse.
void skipSeparator(const char *&cursor)
{
    if (*cursor == '.' || *cursor == '-')
    {
        ++cursor;
    }
}
}  // namespace

int16_t firmwareVersionCode(const char *version)
{
    if (version == nullptr)
    {
        return 0;
    }

    const char *cursor = version;
    if (*cursor == 'v' || *cursor == 'V')
    {
        ++cursor;
    }

    const int major = parseComponent(cursor);
    skipSeparator(cursor);

    const int minor = parseComponent(cursor);
    skipSeparator(cursor);

    const int patch = parseComponent(cursor);

    return static_cast<int16_t>((major * 10000) + (minor * 100) + patch);
}

}  // namespace core
