#include "adapters/DebouncedButtons.hpp"

namespace
{

static_assert(static_cast<uint8_t>(hal::Button::None) == static_cast<uint8_t>(types::Button::None), "the two key sets must agree");
static_assert(static_cast<uint8_t>(hal::Button::Right) == static_cast<uint8_t>(types::Button::Right), "the two key sets must agree");
static_assert(static_cast<uint8_t>(hal::Button::Up) == static_cast<uint8_t>(types::Button::Up), "the two key sets must agree");
static_assert(static_cast<uint8_t>(hal::Button::Down) == static_cast<uint8_t>(types::Button::Down), "the two key sets must agree");
static_assert(static_cast<uint8_t>(hal::Button::Left) == static_cast<uint8_t>(types::Button::Left), "the two key sets must agree");
static_assert(static_cast<uint8_t>(hal::Button::Select) == static_cast<uint8_t>(types::Button::Select), "the two key sets must agree");

}  // namespace

namespace adapters
{

DebouncedButtons::DebouncedButtons(hal::IButtonMatrix &matrix, ports::IClock &clock) : _matrix(matrix), _clock(clock)
{
}

types::Button DebouncedButtons::pressed()
{
    poll();
    return _debounce.committed();
}

bool DebouncedButtons::keyChanged(types::Button *out)
{
    poll();
    return _debounce.changed(out);
}

uint16_t DebouncedButtons::rawValue()
{
    return _matrix.rawValue();
}

void DebouncedButtons::poll()
{
    const hal::Button raw = _matrix.pressed();
    _debounce.update(static_cast<types::Button>(raw), _clock.now());
}

}  // namespace adapters
