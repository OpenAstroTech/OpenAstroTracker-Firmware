#pragma once

#include <stdint.h>

namespace core
{

/// One row of the menu: the text a screen shows and the id of the screen it opens.
struct MenuEntry {
    const char *display;
    uint8_t id;
};

/// The menu's item list and selection.
///
/// The table is caller-owned and fixed-capacity: the menu used to allocate a pointer array plus one
/// `MenuItem` per entry on the heap, which was the only heap use in the UI path on an 8 KB part.
class MenuModel
{
  public:
    MenuModel(MenuEntry *entries, uint8_t capacity);

    /// Appends an item. Returns false when the caller's table is full.
    bool add(const char *display, uint8_t id);

    /// The item with this id, or nullptr when the menu has no such item.
    const MenuEntry *findById(uint8_t id) const;

    /// The id of the active item, or 0 when the menu is empty.
    uint8_t activeId() const;

    /// The index of the active item within the table.
    uint8_t activeIndex() const;

    /// Makes the item with this id active. Returns false when the id is not in the menu.
    bool setActive(uint8_t id);

    /// Advances to the next item, wrapping past the last one. A no-op below two items.
    void next();

    /// Empties the table, keeping the caller's storage.
    void clear();

    uint8_t count() const;

    /// The longest display text in the table, without the selector markers.
    uint8_t longestDisplay() const;

    /// The item at this index, or nullptr when the index is past the end.
    const MenuEntry *at(uint8_t index) const;

  private:
    MenuEntry *_entries;
    uint8_t _capacity;
    uint8_t _count;
    uint8_t _activeIndex;
    uint8_t _longest;
};

}  // namespace core
