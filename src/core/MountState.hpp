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

// The names the firmware has always used for those bits. They were `#define`s in the mount's
// header; they alias `StatusBit`, so the two cannot drift.
#define STATUS_PARKED            ((uint16_t)::core::StatusBit::Parked)
#define STATUS_SLEWING           ((uint16_t)::core::StatusBit::Slewing)
#define STATUS_SLEWING_TO_TARGET ((uint16_t)::core::StatusBit::SlewingToTarget)
#define STATUS_SLEWING_FREE      ((uint16_t)::core::StatusBit::SlewingFree)
#define STATUS_SLEWING_MANUAL    ((uint16_t)::core::StatusBit::SlewingManual)
#define STATUS_TRACKING          ((uint16_t)::core::StatusBit::Tracking)
#define STATUS_PARKING           ((uint16_t)::core::StatusBit::Parking)
#define STATUS_PARKING_POS       ((uint16_t)::core::StatusBit::ParkingPos)
#define STATUS_GUIDE_PULSE       ((uint16_t)::core::StatusBit::GuidePulse)
#define STATUS_GUIDE_PULSE_DIR   ((uint16_t)::core::StatusBit::GuidePulseDir)
#define STATUS_GUIDE_PULSE_RA    ((uint16_t)::core::StatusBit::GuidePulseRa)
#define STATUS_GUIDE_PULSE_DEC   ((uint16_t)::core::StatusBit::GuidePulseDec)
#define STATUS_GUIDE_PULSE_MASK  ((uint16_t)::core::StatusBit::GuidePulseMask)
#define STATUS_FINDING_HOME      ((uint16_t)::core::StatusBit::FindingHome)

/// The single source of truth for that bitfield.
///
/// Threading contract — the same one the raw field carried, now stated once, here:
/// * Reads are safe from any context. The storage is an aligned `uint16_t` on AVR and
///   on the 32-bit platforms, so a load never returns a torn value.
/// * Read-modify-write (`set`, `clear`) is **main-loop-only**. It is not atomic on AVR,
///   so a concurrent writer would lose bits.
/// * A step generator owned by a `hal/` backend may read this state from another
///   context to decide which axes to step; it does not write it. Which backends do
///   that is a platform detail (the ESP32 task and the AVR timer ISR do today; a driver
///   generating its own steps, such as a TMC2209 in `VACTUAL` mode, runs no MCU code at
///   all). The read-side guarantee below is what makes those backends possible.
/// * One exception, still open: the step pump (`app::StepPump`) drives the homing state
///   machine, which can stop an axis and change status from that context. See PLAN.md.
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
