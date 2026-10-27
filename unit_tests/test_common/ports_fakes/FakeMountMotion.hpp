#pragma once

#include "ports/IMountMotion.hpp"

namespace fakes
{

/// A `ports::IMountMotion` that records the commands it was given.
///
/// The screens only ever decide *which* command to send, so counting the calls is enough to pin the
/// behaviour down without a stepper anywhere near the test.
class FakeMountMotion final : public ports::IMountMotion
{
  public:
    void startSlewing(int direction) override
    {
        lastSlewingDirection = direction;
        startSlewingCalls++;
    }

    void stopSlewing(int) override
    {
        stopSlewingAnyCalls++;
    }

    void stopSlewing(StepperAxis axis) override
    {
        lastStoppedAxis = axis;
    }

    void startSlewingToTarget() override
    {
        slewToTargetCalls++;
    }

    void startSlewingToHome() override
    {
        slewToHomeCalls++;
    }

    void park() override
    {
        parkCalls++;
    }

    void guidePulse(uint8_t, int) override
    {
    }

    void moveStepperBy(StepperAxis, long) override
    {
    }

    void moveStepperTo(StepperAxis, long) override
    {
    }

    bool isSlewingRAorDEC() const override
    {
        return slewing;
    }

    bool isSlewingTRK() const override
    {
        return tracking;
    }

    void setHome(bool clearZeroPos) override
    {
        setHomeCalls++;
        lastSetHomeClearsZeroPos = clearZeroPos;
    }

    void syncPosition(const types::DayTime &, const types::Declination &) override
    {
        syncCalls++;
    }

    void setTrackingStepperPos(long) override
    {
    }

    void startDriftAlignment(int duration) override
    {
        driftAlignmentCalls++;
        lastDriftDuration = duration;
    }

    bool findHomeByHallSensor(StepperAxis, int, int) override
    {
        return false;
    }

    int lastSlewingDirection      = 0;
    int startSlewingCalls         = 0;
    int stopSlewingAnyCalls       = 0;
    StepperAxis lastStoppedAxis   = RA_STEPS;
    int slewToTargetCalls         = 0;
    int slewToHomeCalls           = 0;
    int parkCalls                 = 0;
    int setHomeCalls              = 0;
    int syncCalls                 = 0;
    int driftAlignmentCalls       = 0;
    int lastDriftDuration         = 0;
    bool lastSetHomeClearsZeroPos = false;
    bool slewing                  = false;
    bool tracking                 = false;
};

}  // namespace fakes
