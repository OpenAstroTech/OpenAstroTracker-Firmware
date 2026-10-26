#include "MountExecutor.hpp"

#include "ports/ILogger.hpp"

namespace app
{

void MountExecutor::getAZALTPositions(long &azPos, long &altPos)
{
    azPos  = kHasAzStepper ? _axes.az()->currentPosition() : 0;
    altPos = kHasAltStepper ? _axes.alt()->currentPosition() : 0;
}

void MountExecutor::moveAZALTToHome()
{
    if (kHasAzStepper)
    {
        enableAzMotor();
        _axes.az()->moveTo(0);
    }
    if (kHasAltStepper)
    {
        enableAltMotor();
        _axes.alt()->moveTo(0);
    }
}

void MountExecutor::setAZALTHome()
{
    if (kHasAzStepper)
    {
        _axes.az()->setCurrentPosition(0);
        _deps.settings->storeAZPosition(0);
    }
    if (kHasAltStepper)
    {
        _axes.alt()->setCurrentPosition(0);
        _deps.settings->storeALTPosition(0);
    }
}

/////////////////////////////////
void MountExecutor::park()
{
    stopGuiding();
    stopSlewing(ALL_DIRECTIONS | TRACKING);
    waitUntilStopped(ALL_DIRECTIONS);
    _coordinator.parking().beginHoming();
    startSlewingToHome();
    applyParkingStatus();
    publishEvent(core::MountEventType::ParkingStarted);
}

/////////////////////////////////
void MountExecutor::moveBy(int direction, float arcMinutes)
{
    if (kHasAzStepper && direction == AZIMUTH_STEPS)
    {
        enableAzMotor();
        long stepsToMove = arcMinutes * AZIMUTH_STEPS_PER_ARC_MINUTE;
        _axes.az()->move(stepsToMove);
    }
    if (kHasAltStepper && direction == ALTITUDE_STEPS)
    {
        enableAltMotor();
        long stepsToMove = arcMinutes * ALTITUDE_STEPS_PER_ARC_MINUTE;
        _axes.alt()->move(stepsToMove);
    }
}

/////////////////////////////////
void MountExecutor::disableAzAltMotors()
{
    if (kHasAzStepper)
    {
        _axes.az()->stop();
    }
    if (kHasAltStepper)
    {
        _axes.alt()->stop();
    }

    if (kHasAzStepper)
    {
        while (_axes.az()->isRunning())
        {
            loop();
        }
    }

    if (kHasAltStepper)
    {
        while (_axes.alt()->isRunning())
        {
            loop();
        }
    }

    if (AZ_ALWAYS_ON == 0 && kHasAzStepper)
    {
        _deps.azEnablePin->write(hal::PinLevel::High);  // Logic HIGH to disable driver
    }

    if (ALT_ALWAYS_ON == 0 && kHasAltStepper)
    {
        _deps.altEnablePin->write(hal::PinLevel::High);  // Logic HIGH to disable driver
    }
}

/////////////////////////////////
void MountExecutor::enableAzMotor()
{
    if (!kHasAzStepper)
    {
        return;
    }
    _deps.azEnablePin->write(hal::PinLevel::Low);  // Logic LOW to enable driver
    // Let loop() disable the driver again, even if the move turns out to be zero steps
    // (e.g. move to home while already at home), so it never stays powered.
    _coordinator.azAlt().update(true);
}

void MountExecutor::enableAltMotor()
{
    if (!kHasAltStepper)
    {
        return;
    }
    _deps.altEnablePin->write(hal::PinLevel::Low);  // Logic LOW to enable driver
    // See enableAzMotor()
    _coordinator.azAlt().update(true);
}

/////////////////////////////////
bool MountExecutor::isRunningFocus() const
{
    return _coordinator.focus().isRunning();
}

float MountExecutor::getFocusSpeed() const
{
    return _coordinator.focus().speed();
}

/////////////////////////////////
void MountExecutor::focusSetSpeedByRate(int rate)
{
    _coordinator.focus().setRate(rate, _axes.limits().focusSpeed);
    OAT_LOG(_deps.logger,
            DEBUG_FOCUS,
            "[FOCUS]: focusSetSpeedByRate: rate is %d, factor %f, maxspeed %f -> %f",
            _coordinator.focus().rate(),
            _coordinator.focus().rateFactor(),
            _axes.limits().focusSpeed,
            _coordinator.focus().maxRateSpeed());

    if (_coordinator.focus().isRunning())
    {
        OAT_LOG(_deps.logger, DEBUG_FOCUS, "[FOCUS]: focusSetSpeedByRate: stepper is already running so should adjust speed?");
        //_axes.focus()->setSpeed(_coordinator.focus().rateFactor() * _axes.limits().focusSpeed);
    }
}

/////////////////////////////////
void MountExecutor::focusContinuousMove(FocuserDirection direction)
{
    // maxSpeed is set to what the rate dictates
    OAT_LOG(_deps.logger,
            DEBUG_FOCUS,
            "[FOCUS]: focusContinuousMove: direction is %d, maxspeed %f",
            direction,
            _coordinator.focus().maxRateSpeed());
    setSpeed(FOCUS_STEPS, static_cast<int>(direction) * _coordinator.focus().maxRateSpeed());
}

/////////////////////////////////
void MountExecutor::focusMoveBy(long steps)
{
    OAT_LOG(_deps.logger,
            DEBUG_FOCUS,
            "[FOCUS]: focusMoveBy: move by %ld steps to %ld. Target Mode.",
            steps,
            _coordinator.focus().position() + steps);
    enableFocusMotor();
    _coordinator.focus().moveBy(steps);
    publishEvent(core::MountEventType::FocusMoving);
}

/////////////////////////////////
long MountExecutor::focusGetStepperPosition()
{
    return _coordinator.focus().position();
}

/////////////////////////////////
void MountExecutor::focusSetStepperPosition(long steps)
{
    _coordinator.focus().setPosition(steps);
}

/////////////////////////////////
void MountExecutor::disableFocusMotor()
{
    OAT_LOG(_deps.logger, DEBUG_FOCUS, "[FOCUS]: disableFocusMotor: stopping motor and waiting.");
    _axes.focus()->stop();
    waitUntilStopped(FOCUSING);

    if (FOCUSER_ALWAYS_ON == 0 && kHasFocusStepper)
    {
        _deps.focusEnablePin->write(hal::PinLevel::High);  // Logic HIGH to disable driver
    }
}

/////////////////////////////////
void MountExecutor::enableFocusMotor()
{
    OAT_LOG(_deps.logger, DEBUG_FOCUS, "[FOCUS]: enableFocusMotor: enabling driver pin.");
    _deps.focusEnablePin->write(hal::PinLevel::Low);  // Logic LOW to enable driver
}

/////////////////////////////////
void MountExecutor::focusStop()
{
    OAT_LOG(_deps.logger, DEBUG_FOCUS, "[FOCUS]: focusStop: stopping motor.");
    _coordinator.focus().stop();
}

}  // namespace app
