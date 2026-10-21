#pragma once

#include <stdint.h>

namespace core
{

/// What the mount is doing, as the bitfield the firmware has always used.
///
/// The values are the existing `STATUS_*` layout, preserved exactly: the Meade status
/// string, the menus and the displays all decode them. Two entries are masks rather
/// than single bits (`GuidePulseDir` covers the two RA/DEC direction bits,
/// `GuidePulseMask` adds the "guiding" bit).
enum class StatusBit : uint16_t
{
    Parked          = 0x0000,
    Slewing         = 0x0002,
    SlewingToTarget = 0x0004,
    SlewingFree     = 0x0002,
    SlewingManual   = 0x0100,
    Tracking        = 0x0008,
    Parking         = 0x0010,
    ParkingPos      = 0x1000,
    GuidePulse      = 0x0080,
    GuidePulseDir   = 0x0060,
    GuidePulseRa    = 0x0040,
    GuidePulseDec   = 0x0020,
    GuidePulseMask  = 0x00E0,
    FindingHome     = 0x2000,
};

/// The single source of truth for that bitfield.
///
/// Threading contract — the same one the raw field carried, now stated once, here:
/// * Reads are safe from any context. The storage is an aligned `uint16_t` on AVR and
///   on the 32-bit platforms, so a load never returns a torn value.
/// * Read-modify-write (`set`, `clear`) is **main-loop-only**. It is not atomic on AVR,
///   so a concurrent writer would lose bits.
/// * The step generator — `stepperControlTask` on core 0 (ESP32) or the timer ISR
///   (RP2040, and AVR without `NEW_STEPPER_LIB`, where `interruptLoop()` is compiled
///   out) — reads this state to decide which axes to step and does not write it.
/// * One exception, still open: `interruptLoop()` drives the homing state machine,
///   which can stop an axis and change status from that context. See PLAN.md.
class MountState
{
  public:
    void reset(uint16_t bits)
    {
        _bits = bits;
    }

    void set(uint16_t bits)
    {
        _bits |= bits;
    }

    void clear(uint16_t bits)
    {
        // `~bits` promotes to int, so the high bits are set; the narrowing assignment
        // back into the 16-bit field is what keeps the mask to this type's width.
        _bits &= ~bits;
    }

    bool test(uint16_t bits) const
    {
        return (_bits & bits) != 0;
    }

    uint16_t value() const
    {
        return _bits;
    }

    /// Everything clear means parked: `STATUS_PARKED` has always been the zero value.
    bool isParked() const
    {
        return _bits == 0;
    }

  private:
    volatile uint16_t _bits = 0;
};

}  // namespace core
