#include "core/ButtonDebounce.hpp"

namespace core
{

ButtonDebounce::ButtonDebounce() : _raw(types::Button::Invalid), _committed(types::Button::None), _reported(types::Button::None), _rawSince(0)
{
}

void ButtonDebounce::update(types::Button raw, uint32_t nowMs)
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

types::Button ButtonDebounce::committed() const
{
    return _committed;
}

bool ButtonDebounce::changed(types::Button *out)
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
