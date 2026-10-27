#pragma once

#include "ports/IMountOptionalAxes.hpp"

namespace fakes
{

/// A focuser + AZ/ALT pair that records what it was asked to do.
///
/// `IMountOptionalAxes` extends `IMountMotion`, so the motion half is here too — no-ops unless a
/// test cares, which keeps the fakes for the screens that use both small.
class FakeOptionalAxes final : public ports::IMountOptionalAxes
{
  public:
    // --- focuser ---
    void focusMoveBy(long steps) override
    {
        lastMoveBy = steps;
    }
    void focusSetStepperPosition(long steps) override
    {
        position = steps;
    }
    long focusGetStepperPosition() override
    {
        return position;
    }
    void focusSetSpeedByRate(int rate) override
    {
        lastRate = rate;
    }
    float getFocusSpeed() const override
    {
        return speed;
    }
    void focusContinuousMove(types::FocuserDirection direction) override
    {
        lastDirection = direction;
        running       = true;
        continuousCalls++;
    }
    void focusStop() override
    {
        running = false;
        stopCalls++;
    }
    bool isRunningFocus() const override
    {
        return running;
    }

    int moveByCalls = 0;

    // --- azimuth / altitude ---
    void moveBy(int direction, float arcMinutes) override
    {
        lastMoveDirection = direction;
        lastArcMinutes    = arcMinutes;
        moveByCalls++;
    }
    void setAZALTHome() override
    {
        azAltHomeSet = true;
    }
    void moveAZALTToHome() override
    {
        azAltHomingCalls++;
    }
    void getAZALTPositions(long &azimuthSteps, long &altitudeSteps) override
    {
        azimuthSteps  = azimuth;
        altitudeSteps = altitude;
    }

    // --- configuration (the optional axes port extends it) ---
    float getStepsPerDegree(StepperAxis) override
    {
        return stepsPerDegree;
    }
    void setStepsPerDegree(StepperAxis, float steps) override
    {
        stepsPerDegree = steps;
    }
    void setSpeed(StepperAxis, float speedDegsPerSec) override
    {
        lastSpeed = speedDegsPerSec;
    }
    float getSpeed(int) override
    {
        return lastSpeed;
    }
    void setSlewRate(int rate) override
    {
        slewRate = rate;
    }
    int getSlewRate() override
    {
        return slewRate;
    }
    float getSpeedCalibration() override
    {
        return speedCalibration;
    }
    void setSpeedCalibration(float value, bool) override
    {
        speedCalibration = value;
    }
    void setDecLimitPosition(bool, float) override
    {
    }
    void clearDecLimitPosition(bool) override
    {
    }
    void getDecLimitPositions(float &lowerLimit, float &upperLimit) override
    {
        lowerLimit = 0.0f;
        upperLimit = 0.0f;
    }
    void setHomingOffset(StepperAxis, long offset) override
    {
        homingOffset = offset;
    }
    long getHomingOffset(StepperAxis) override
    {
        return homingOffset;
    }
    void setBacklashCorrection(int steps) override
    {
        backlash = steps;
    }
    int getBacklashCorrection() override
    {
        return backlash;
    }
    void setManualSlewMode(bool) override
    {
    }
    void clearConfiguration() override
    {
    }

    // --- motion (unused by the focus screen) ---
    void startSlewing(int) override
    {
    }
    void stopSlewing(int) override
    {
    }
    void stopSlewing(StepperAxis) override
    {
    }
    void startSlewingToTarget() override
    {
    }
    void startSlewingToHome() override
    {
    }
    void park() override
    {
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
        return false;
    }
    bool isSlewingTRK() const override
    {
        return false;
    }
    void setHome(bool) override
    {
    }
    void syncPosition(const types::DayTime &, const types::Declination &) override
    {
    }
    void setTrackingStepperPos(long) override
    {
    }
    void startDriftAlignment(int) override
    {
    }
    bool findHomeByHallSensor(StepperAxis, int, int) override
    {
        return false;
    }

    bool running                         = false;
    float speed                          = 1.0f;
    long position                        = 0;
    long lastMoveBy                      = 0;
    int lastRate                         = 0;
    int continuousCalls                  = 0;
    int stopCalls                        = 0;
    int azAltHomingCalls                 = 0;
    long azimuth                         = 0;
    long altitude                        = 0;
    int lastMoveDirection                = 0;
    float lastArcMinutes                 = 0.0f;
    bool azAltHomeSet                    = false;
    float stepsPerDegree                 = 100.0f;
    float lastSpeed                      = 1.0f;
    float speedCalibration               = 1.0f;
    int slewRate                         = 2;
    long homingOffset                    = 0;
    int backlash                         = 0;
    types::FocuserDirection lastDirection = types::FOCUS_FORWARD;
};

}  // namespace fakes
