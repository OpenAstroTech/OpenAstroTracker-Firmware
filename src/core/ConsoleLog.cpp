#include "core/ConsoleLog.hpp"

namespace core
{

ConsoleLog::ConsoleLog() : _count(0)
{
    for (int i = 0; i < kCapacity; i++)
    {
        _lines[i][0] = '\0';
    }
}

int ConsoleLog::add(const char *text)
{
    // Oldest line first, as the panel always scrolled: the caller's line indices keep pointing at
    // the newest lines rather than wrapping around a ring.
    if (_count == kCapacity)
    {
        for (int i = 1; i < kCapacity; i++)
        {
            copyUpper(_lines[i - 1], _lines[i]);
        }
        _count = kCapacity - 1;
    }

    const int index = _count++;
    copyUpper(_lines[index], text);
    return index;
}

void ConsoleLog::update(int line, const char *text)
{
    if (line < 0 || line >= _count)
    {
        return;
    }
    copyUpper(_lines[line], text);
}

int ConsoleLog::firstVisible() const
{
    const int first = _count - kVisible;
    return (first > 0) ? first : 0;
}

int ConsoleLog::visibleCount() const
{
    return (_count < kVisible) ? _count : kVisible;
}

const char *ConsoleLog::line(int index) const
{
    if (index < 0 || index >= _count)
    {
        return "";
    }
    return _lines[index];
}

void ConsoleLog::copyUpper(char *destination, const char *source)
{
    int i = 0;
    for (; source != nullptr && source[i] != '\0' && i < kLineLength; i++)
    {
        const char c   = source[i];
        destination[i] = (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
    }
    destination[i] = '\0';
}

}  // namespace core
