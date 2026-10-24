#pragma once

#include "core/MountState.hpp"
#include "ports/IStatusFlags.hpp"

namespace adapters
{

/// `ports::IStatusFlags` over the coordinator's status bitfield.
///
/// `Mount` used to be this port, which meant the homing state machine needed a whole mount to
/// set two bits. The bitfield belongs to `core::MountState`, so this adapter is all that is
/// needed; the interrupt guard is `hal::CriticalSection`, so neither this class nor `core/` knows
/// which platforms need a lock.
class StatusFlags final : public ports::IStatusFlags
{
  public:
    explicit StatusFlags(core::MountState &state);

    void set(uint16_t bits) override;

    void clear(uint16_t bits) override;

  private:
    core::MountState &_state;
};

}  // namespace adapters
