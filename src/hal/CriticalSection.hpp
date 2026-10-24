#pragma once

// Mutual exclusion against the step generator.
//
// The status bitfield is a read-modify-write on a `uint16_t`, which is not atomic on AVR: an
// interrupt that runs between the load and the store loses the bits the main loop just set. The
// firmware has always wrapped those writes in `noInterrupts()`/`interrupts()`; this header is
// where that knowledge lives, so no `core/` type and no `adapters/` type has to know which
// platform needs it or how.
//
// The guard saves and restores the interrupt-enable state rather than unconditionally enabling
// interrupts on the way out, so it nests and it is safe inside an ISR — the old
// `noInterrupts(); ...; interrupts();` pair was not.
//
// Platforms with no interrupt-context writer (a FreeRTOS task on ESP32) get a no-op: the lock is
// only needed where a step generator can preempt a write, and pretending otherwise would add
// cost to a path that does not have the hazard.

#if defined(ARDUINO_ARCH_AVR) || defined(__AVR__)
    #include <avr/interrupt.h>

namespace hal
{

class CriticalSection
{
  public:
    CriticalSection() : _sreg(SREG)
    {
        cli();
    }

    ~CriticalSection()
    {
        SREG = _sreg;
    }

    CriticalSection(const CriticalSection &)            = delete;
    CriticalSection &operator=(const CriticalSection &) = delete;

  private:
    uint8_t _sreg;
};

}  // namespace hal

#else

namespace hal
{

/// No interrupt-context writer on this platform: nothing to mask.
///
/// The constructor is written out rather than defaulted so the type is non-trivial: a guard that
/// does nothing is still a guard, and a trivially-constructible unused local would trip
/// `-Wunused-variable` at every call site.
class CriticalSection
{
  public:
    CriticalSection()
    {
    }
};

}  // namespace hal

#endif
