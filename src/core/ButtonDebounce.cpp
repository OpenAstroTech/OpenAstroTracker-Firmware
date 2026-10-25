#include "core/ButtonDebounce.hpp"

namespace core
{

ButtonDebounce::ButtonDebounce() : _raw(Button::Invalid), _committed(Button::None), _reported(Button::None), _rawSince(0)
{
}

void ButtonDebounce::update(Button raw, uint32_t nowMs)
{
    if (raw != _raw)
    {
        _raw      = raw;
        _rawSince = nowMs;
        return;
    }

    // The wrap is safe because both sides are unsigned and the interval is far below 2^32 ms.
    if ((nowMs - _rawSince) > kCommitMs)
    {
        _committed = raw;
    }
}

Button ButtonDebounce::committed() const
{
    return _committed;
}

bool ButtonDebounce::changed(Button *out)
{
    if (_committed == _reported)
    {
        return false;
    }

    _reported = _committed;
    if (out != nullptr)
    {
        *out = _committed;
    }
    return true;
}

}  // namespace core
