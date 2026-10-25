#include "core/menu/MenuModel.hpp"

#include <string.h>

namespace core
{

MenuModel::MenuModel(MenuEntry *entries, uint8_t capacity) : _entries(entries), _capacity(capacity), _count(0), _activeIndex(0), _longest(0)
{
}

bool MenuModel::add(const char *display, uint8_t id)
{
    if (_entries == nullptr || _count >= _capacity)
    {
        return false;
    }

    _entries[_count].display = display;
    _entries[_count].id      = id;
    _count++;

    const size_t length = strlen(display);
    if (length > _longest)
    {
        _longest = static_cast<uint8_t>(length);
    }

    return true;
}

const MenuEntry *MenuModel::findById(uint8_t id) const
{
    for (uint8_t i = 0; i < _count; i++)
    {
        if (_entries[i].id == id)
        {
            return &_entries[i];
        }
    }
    return nullptr;
}

uint8_t MenuModel::activeId() const
{
    return (_count == 0) ? 0 : _entries[_activeIndex].id;
}

uint8_t MenuModel::activeIndex() const
{
    return _activeIndex;
}

bool MenuModel::setActive(uint8_t id)
{
    for (uint8_t i = 0; i < _count; i++)
    {
        if (_entries[i].id == id)
        {
            _activeIndex = i;
            return true;
        }
    }
    return false;
}

void MenuModel::next()
{
    if (_count < 2)
    {
        return;
    }
    _activeIndex = static_cast<uint8_t>((_activeIndex + 1) % _count);
}

void MenuModel::clear()
{
    _count       = 0;
    _activeIndex = 0;
    _longest     = 0;
}

uint8_t MenuModel::count() const
{
    return _count;
}

uint8_t MenuModel::longestDisplay() const
{
    return _longest;
}

const MenuEntry *MenuModel::at(uint8_t index) const
{
    return (index < _count) ? &_entries[index] : nullptr;
}

}  // namespace core
