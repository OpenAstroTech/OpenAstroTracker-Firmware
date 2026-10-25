#pragma once

#include <stddef.h>
#include <stdint.h>

namespace core
{

/// The boot console's fixed-capacity line ring.
///
/// The panel shows the last few bring-up lines while the mount initialises; the ring holds the
/// recent history so the window can scroll. Text is stored uppercased because the small OLED font
/// has no lowercase glyphs, and lines are fixed-size so nothing allocates during boot.
class ConsoleLog
{
  public:
    static constexpr int kCapacity   = 16;
    static constexpr int kVisible    = 6;
    static constexpr int kLineLength = 24;

    ConsoleLog();

    /// Appends a line and returns its index, discarding the oldest line when the ring is full.
    int add(const char *text);

    /// Replaces one line in place. Out-of-range indices are ignored.
    void update(int line, const char *text);

    /// The first line of the visible window, given how many lines have been added.
    int firstVisible() const;

    /// The lines that fit on screen, oldest first.
    int visibleCount() const;

    /// The text of one line, or an empty string when the index is out of range.
    const char *line(int index) const;

  private:
    static void copyUpper(char *destination, const char *source);

    char _lines[kCapacity][kLineLength + 1];
    int _count;
};

}  // namespace core
