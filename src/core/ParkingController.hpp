#pragma once

#include <stdint.h>

#include "MountState.hpp"

namespace core
{

/// Where a park sequence is.
enum class ParkingPhase : uint8_t
{
    NotParking,        ///< not requested, or finished and acknowledged
    Homing,            ///< first leg: slewing to home so the home position is known
    MovingToPosition,  ///< second leg: moving from home to the configured park position
    Parked,            ///< both legs finished
};

/// The park sequence as a state machine.
///
/// Parking is two legs, not one: the mount first homes so it knows where it is, then
/// moves to the configured park position. That is why "is the mount parking?" and "is it
/// on the second leg?" used to be two status bits plus an extra flag on the slew object,
/// kept in step by hand at four sites.
///
/// The controller owns the phase; `Mount` renders it into `STATUS_PARKING` /
/// `STATUS_PARKING_POS`, because those bits are the wire format the Meade status string
/// and the displays decode. Deriving them from one phase is what stops the two views
/// disagreeing.
///
/// Threading: main-loop-only.
class ParkingController
{
  public:
    /// `park()` was called. Re-requesting restarts the sequence from homing, which is
    /// what the firmware has always done.
    void beginHoming()
    {
        _phase = ParkingPhase::Homing;
    }

    /// The first leg finished: set home and start moving to the park position.
    void beginPositionLeg()
    {
        _phase = ParkingPhase::MovingToPosition;
    }

    void markParked()
    {
        _phase = ParkingPhase::Parked;
    }

    /// Nothing is parking: either never requested, or the mount was moved home by hand.
    void clear()
    {
        _phase = ParkingPhase::NotParking;
    }

    ParkingPhase phase() const
    {
        return _phase;
    }

    /// True while a park owns the axes. A finished park does not: the mount is parked but
    /// free to be commanded, which is how unparking works.
    bool isParking() const
    {
        return (_phase == ParkingPhase::Homing) || (_phase == ParkingPhase::MovingToPosition);
    }

    bool headingToPosition() const
    {
        return _phase == ParkingPhase::MovingToPosition;
    }

    bool isParked() const
    {
        return _phase == ParkingPhase::Parked;
    }

    /// The status bit that represents this phase, or 0 for the phases that set neither.
    /// The homing leg is `STATUS_PARKING`, the park-position leg `STATUS_PARKING_POS`.
    static uint16_t statusBitFor(ParkingPhase phase)
    {
        switch (phase)
        {
            case ParkingPhase::Homing:
                return static_cast<uint16_t>(StatusBit::Parking);
            case ParkingPhase::MovingToPosition:
                return static_cast<uint16_t>(StatusBit::ParkingPos);
            default:
                return 0;
        }
    }

  private:
    ParkingPhase _phase = ParkingPhase::NotParking;
};

}  // namespace core
