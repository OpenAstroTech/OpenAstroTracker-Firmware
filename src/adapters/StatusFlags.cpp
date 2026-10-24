#include "adapters/StatusFlags.hpp"

#include "hal/CriticalSection.hpp"

namespace adapters
{

StatusFlags::StatusFlags(core::MountState &state) : _state(state)
{
}

void StatusFlags::set(uint16_t bits)
{
    const hal::CriticalSection lock;
    _state.set(bits);
}

void StatusFlags::clear(uint16_t bits)
{
    const hal::CriticalSection lock;
    _state.clear(bits);
}

}  // namespace adapters
