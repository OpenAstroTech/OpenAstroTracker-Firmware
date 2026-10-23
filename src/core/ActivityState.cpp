#include "ActivityState.hpp"

namespace core
{

ActivityState classifyActivity(const MountState &state, bool findingHome, bool guiding, bool anySlew, bool trackingAxis)
{
    if (state.isParked())
    {
        return ActivityState::Parked;
    }
    if (state.test(static_cast<uint16_t>(StatusBit::Parking)) || state.test(static_cast<uint16_t>(StatusBit::ParkingPos)))
    {
        return ActivityState::Parking;
    }
    if (findingHome)
    {
        return ActivityState::Homing;
    }
    if (guiding)
    {
        return ActivityState::Guiding;
    }

    if (anySlew || trackingAxis)
    {
        if (state.test(static_cast<uint16_t>(StatusBit::SlewingToTarget)))
        {
            return ActivityState::SlewToTarget;
        }
        if (state.test(static_cast<uint16_t>(StatusBit::SlewingFree)))
        {
            return ActivityState::FreeSlew;
        }
        if (state.test(static_cast<uint16_t>(StatusBit::SlewingManual)))
        {
            return ActivityState::ManualSlew;
        }
        if (trackingAxis)
        {
            return ActivityState::Tracking;
        }
        return ActivityState::Unknown;
    }

    return ActivityState::Idle;
}

}  // namespace core
