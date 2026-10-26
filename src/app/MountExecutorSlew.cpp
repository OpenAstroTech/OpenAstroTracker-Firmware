#include "MountExecutor.hpp"

#include "ports/ILogger.hpp"
#include "../core/DecLimitClamp.hpp"
#include "../core/ManualSlew.hpp"
#include "../core/RelativeMove.hpp"
#include "../core/SlewStart.hpp"
#include "../core/SyncAdjustment.hpp"

namespace app
{

void MountExecutor::syncPosition(const core::DayTime &ra, const core::Declination &dec)
{
    long solutions[6];
    _targetDEC = Declination(dec);
    _targetRA  = DayTime(ra);
    OAT_LOG(_deps.logger,
            DEBUG_COORD_CALC,
            "[MOUNT]: syncPosition: Target Sync is RA: %f  and DEC: %f",
            _targetRA.getTotalHours(),
            _targetDEC.getTotalDegrees());
    OAT_LOG(_deps.logger,
            DEBUG_COORD_CALC,
            "[MOUNT]: syncPosition: Current Pos is RA: %f  and DEC: %f )",
            currentRA().getTotalHours(),
            currentDEC().getTotalDegrees());
    OAT_LOG(_deps.logger,
            DEBUG_COORD_CALC,
            "[MOUNT]: syncPosition: ZeroPos values RA: %f  and DEC: %f)",
            DayTime(_siteTime.raZeroPoint()).getTotalHours(),
            _zeroPosDEC);

    // The declination the stepper position maps to, before the correction: negative when the
    // mount sits on the far side of home, which is what flips the declination correction.
    const float degreePos
        = (_axes.dec()->currentPosition() / _settings.stepsPerDegree(DEC_STEPS)) + _zeroPosDEC;  // u-steps / u-steps/deg = deg
    OAT_LOG(_deps.logger, DEBUG_COORD_CALC, "[MOUNT]: syncPosition: DEC degreePos is: %f", degreePos);

    const core::SyncAdjustment adjustment = core::syncAdjustment(
        ra.getTotalHours(), currentRA().getTotalHours(), dec.getTotalDegrees(), currentDEC().getTotalDegrees(), degreePos);
    OAT_LOG(_deps.logger, DEBUG_COORD_CALC, "[MOUNT]: syncPosition: AdjustRA is %f", adjustment.raHours);
    _siteTime.adjustRaZeroPoint(adjustment.raHours);
    OAT_LOG(_deps.logger, DEBUG_COORD_CALC, "[MOUNT]: syncPosition: ZeroPosRA is now %f", DayTime(_siteTime.raZeroPoint()).getTotalHours());

    _zeroPosDEC += adjustment.decDegrees;
    OAT_LOG(_deps.logger, DEBUG_COORD_CALC, "[MOUNT]: syncPosition: _zeroPosDEC adjusted by: %f", adjustment.decDegrees);
    OAT_LOG(_deps.logger, DEBUG_COORD_CALC, "[MOUNT]: syncPosition: _zeroPosDEC: %f", _zeroPosDEC);

    long targetRAPosition, targetDECPosition;
    calculateRAandDECSteppers(targetRAPosition, targetDECPosition, solutions);

    OAT_LOG(_deps.logger, DEBUG_COORD_CALC, "[MOUNT]: syncPosition: Solution 1: RA %ld and DEC %ld", solutions[0], solutions[1]);
    OAT_LOG(_deps.logger, DEBUG_COORD_CALC, "[MOUNT]: syncPosition: Solution 2: RA %ld and DEC %ld", solutions[2], solutions[3]);
    OAT_LOG(_deps.logger, DEBUG_COORD_CALC, "[MOUNT]: syncPosition: Solution 3: RA %ld and DEC %ld", solutions[4], solutions[5]);
    OAT_LOG(_deps.logger, DEBUG_COORD_CALC, "[MOUNT]: syncPosition: Chosen    : RA %ld and DEC %ld", targetRAPosition, targetDECPosition);
}

void MountExecutor::startSlewingToTarget()
{
    if (!admit(core::MountRequest::GoTo))
    {
        // A go-to used to reprogram both axes underneath a park sequence that was still
        // homing. PLAN.md lists this refusal as the one intentional behaviour change.
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: startSlewingToTarget: refused while parking");
        return;
    }

    stopGuiding();
    setSlewSpeeds();

    // Calculate new RA stepper target (and DEC). We are never in guiding mode here.
    _currentRAStepperPosition = _axes.ra()->currentPosition();
    long targetRAPosition, targetDECPosition;
    calculateRAandDECSteppers(targetRAPosition, targetDECPosition);

    const core::SlewStartPlan plan
        = core::planSlewStart(core::SlewStartKind::Target, {_axes.ra()->currentPosition(), targetRAPosition, isSlewingTRK()});
    beginSlew(plan, core::SlewIntent::Target, targetRAPosition, targetDECPosition);
}

void MountExecutor::startSlewingToHome()
{
    stopGuiding();
    setSlewSpeeds();

    _currentRAStepperPosition = _axes.ra()->currentPosition();

    const long targetDECPosition = 0;
    const long targetRAPosition
        = core::homeRaTargetSteps(_axes.tracking()->currentPosition(), _config.raSlewMicrosteps, _config.raTrackingMicrosteps());
    OAT_LOG(_deps.logger,
            DEBUG_STEPPERS,
            "[STEPPERS]: startSlewingToHome: Adjusted with tracking distance: %ld, result: %ld",
            _axes.tracking()->currentPosition(),
            targetRAPosition);

    const core::SlewStartPlan plan
        = core::planSlewStart(core::SlewStartKind::Home, {_axes.ra()->currentPosition(), targetRAPosition, isSlewingTRK()});
    beginSlew(plan, core::SlewIntent::Home, targetRAPosition, targetDECPosition);
}

void MountExecutor::beginSlew(const core::SlewStartPlan &plan, core::SlewIntent intent, long raTarget, long decTarget)
{
    if (plan.stopTracking)
    {
        // Only stop tracking if we're actually going to slew somewhere else, otherwise the
        // loop() code won't detect the end of the slewing operation...
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[MOUNT]: Stop tracking (NEMA steppers)");
        stopSlewing(TRACKING);
        _coordinator.tracking().noteStopped(_deps.clock->now());
    }

    if (plan.enterRaSlewMicrosteps)
    {
        // Set RA to slew microsteps for TMC2209 UART once the TRK stepper has stopped
        setRaMicrosteps(_config.raSlewMicrosteps);
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: beginSlew: TRK stopped at %ldms", _coordinator.tracking().stoppedAt());
    }

    _coordinator.slew().begin(intent);
    setStatusFlag(STATUS_SLEWING | STATUS_SLEWING_TO_TARGET);
    publishEvent(core::MountEventType::SlewStarted);
    // Normal state for DEC is guide microstepping, so switch to slew microstepping here.
    setDecMicrosteps(_config.decSlewMicrosteps);
    _coordinator.slew().noteRunning(true);
    moveSteppersTo(raTarget, decTarget, RA_AND_DEC_STEPS);  // u-steps (in slew mode)
    _coordinator.slew().setTotalMove(static_cast<float>(_axes.ra()->distanceToGo()), static_cast<float>(_axes.dec()->distanceToGo()));
    OAT_LOG(_deps.logger, DEBUG_MOUNT, "[MOUNT]: RA Dist: %ld,   DEC Dist: %ld", _axes.ra()->distanceToGo(), _axes.dec()->distanceToGo());
}

void MountExecutor::setSlewSpeeds()
{
    OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: beginSlew: Set DEC to MaxSpeed(%ld)", _axes.limits().decSpeed);
    _axes.dec()->setMaxSpeed(_axes.limits().decSpeed);
    OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: beginSlew: Set RA  to MaxSpeed(%ld)", _axes.limits().raSpeed);
    _axes.ra()->setMaxSpeed(_axes.limits().raSpeed);
}

/////////////////////////////////
void MountExecutor::setManualSlewMode(bool state)
{
    if (state)
    {
        stopSlewing(ALL_DIRECTIONS);
        stopSlewing(TRACKING);
        waitUntilStopped(ALL_DIRECTIONS);
        setStatusFlag(STATUS_SLEWING | STATUS_SLEWING_MANUAL);
        setRaMicrosteps(_config.raSlewMicrosteps);
    }
    else
    {
        clearStatusFlag(STATUS_SLEWING_MANUAL);
        stopSlewing(ALL_DIRECTIONS);
        waitUntilStopped(ALL_DIRECTIONS);
        OAT_LOG(_deps.logger,
                DEBUG_STEPPERS,
                "[STEPPERS]: setManualSlewMode: Set RA  speed/accel:  %ld  / %ld",
                _axes.limits().raSpeed,
                _axes.limits().raAcceleration);
        OAT_LOG(_deps.logger,
                DEBUG_STEPPERS,
                "[STEPPERS]: setManualSlewMode: Set DEC speed/accel:  %ld  / %ld",
                _axes.limits().decSpeed,
                _axes.limits().decAcceleration);
        _axes.ra()->setAcceleration(_axes.limits().raAcceleration);
        _axes.ra()->setMaxSpeed(_axes.limits().raSpeed);
        _axes.dec()->setMaxSpeed(_axes.limits().decSpeed);
        _axes.dec()->setAcceleration(_axes.limits().decAcceleration);
        startSlewing(TRACKING);
    }
}

/////////////////////////////////
void MountExecutor::setSpeed(StepperAxis which, float speedDegsPerSec)
{
    if (which == RA_STEPS)
    {
        float stepsPerSec = speedDegsPerSec * _settings.stepsPerDegree(RA_STEPS);  // deg/sec * u-steps/deg = u-steps/sec
        OAT_LOG(_deps.logger,
                DEBUG_STEPPERS,
                "[STEPPERS]: setSpeed: Set RA speed %f degs/s, which is %f steps/s",
                speedDegsPerSec,
                stepsPerSec);
        // TODO: Are we already in slew mode?
        _axes.ra()->setSpeed(stepsPerSec);
    }
    else if (which == DEC_STEPS)
    {
        float stepsPerSec = speedDegsPerSec * _settings.stepsPerDegree(DEC_STEPS);  // deg/sec * u-steps/deg = u-steps/sec
        OAT_LOG(_deps.logger,
                DEBUG_STEPPERS,
                "[STEPPERS]: setSpeed: Set DEC speed %f degs/s, which is %f steps/s",
                speedDegsPerSec,
                stepsPerSec);
        // TODO: Are we already in slew mode?
        _axes.dec()->setSpeed(stepsPerSec);
    }
    else if (kHasAzStepper && which == AZIMUTH_STEPS)
    {
        float stepsPerSec = speedDegsPerSec * _axes.azStepsPerDegree();  // deg/sec * u-steps/deg = u-steps/sec
        OAT_LOG(_deps.logger,
                DEBUG_STEPPERS,
                "[STEPPERS]: setSpeed: Set AZ speed %f degs/s, which is %f steps/s",
                speedDegsPerSec,
                stepsPerSec);
        _axes.az()->setSpeed(stepsPerSec);
    }
    else if (kHasAltStepper && which == ALTITUDE_STEPS)
    {
        float stepsPerSec = speedDegsPerSec * _axes.altStepsPerDegree();  // deg/sec * u-steps/deg = u-steps/sec
        OAT_LOG(_deps.logger,
                DEBUG_STEPPERS,
                "[STEPPERS]: setSpeed: Set ALT speed %f degs/s, which is %f steps/s",
                speedDegsPerSec,
                stepsPerSec);
        _axes.alt()->setSpeed(stepsPerSec);
    }
    else if (kHasFocusStepper && which == FOCUS_STEPS)
    {
        OAT_LOG(_deps.logger, DEBUG_MOUNT | DEBUG_FOCUS, "[FOCUS]: setSpeed() Focuser setSpeed %f", speedDegsPerSec);
        if (speedDegsPerSec != 0)
        {
            OAT_LOG(_deps.logger,
                    DEBUG_STEPPERS | DEBUG_FOCUS,
                    "[FOCUS]: Mount:setSpeed(): Enabling motor, setting speed to %f. Continuous",
                    speedDegsPerSec);
            enableFocusMotor();
            _coordinator.focus().runAtSpeed(speedDegsPerSec);
            publishEvent(core::MountEventType::FocusMoving);
        }
        else
        {
            OAT_LOG(_deps.logger, DEBUG_STEPPERS | DEBUG_FOCUS, "[FOCUS]: setSpeed(): Stopping motor.");
            _axes.focus()->stop();
        }
    }
}

/////////////////////////////////
void MountExecutor::setTrackingStepperPos(long stepPos)
{
    _axes.tracking()->setCurrentPosition(stepPos);
}

bool MountExecutor::admit(core::MountRequest request) const
{
    return _coordinator.admit(request) == core::Admission::Allowed;
}

void MountExecutor::setRaMicrosteps(int microsteps)
{
    OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: RA driver setMicrosteps(%d)", microsteps);
    _axes.raDriver()->microsteps(core::microstepRegisterValue(microsteps));
}

void MountExecutor::setDecMicrosteps(int microsteps)
{
    OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: DEC driver setMicrosteps(%d)", microsteps);
    _axes.decDriver()->microsteps(core::microstepRegisterValue(microsteps));
}

/////////////////////////////////
void MountExecutor::startSlewing(int direction)
{
    // Parking owns both axes until it finishes; every request in this function is
    // refused while it runs. See core/MountRequests.hpp for the whole table.
    if (!admit((direction & TRACKING) ? core::MountRequest::Track : core::MountRequest::ManualSlew))
    {
        return;
    }

    stopGuiding();

    if (direction & TRACKING)
    {
        setRaMicrosteps(_config.raTrackingMicrosteps());
        _coordinator.tracking().applySpeed();

        setStatusFlag(STATUS_TRACKING);
        publishEvent(core::MountEventType::TrackingStarted);
        return;
    }

    // A manual slew runs at the last commanded rate.
    setSlewRate(_settings.moveRate());
    if (isSlewingTRK())
    {
        stopSlewing(TRACKING);
        _coordinator.tracking().noteStopped(_deps.clock->now());
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: startSlewing: stopped TRK at %ld", _coordinator.tracking().stoppedAt());
    }

    // Both axes run at slew microsteps while a manual move is in flight.
    setRaMicrosteps(_config.raSlewMicrosteps);
    setDecMicrosteps(_config.decSlewMicrosteps);

    core::ManualSlewFacts facts;
    facts.raStepsPerDegree       = _settings.stepsPerDegree(RA_STEPS);
    facts.decStepsPerDegree      = _settings.stepsPerDegree(DEC_STEPS);
    facts.raPhysicalLimitDegrees = RA_PHYSICAL_LIMIT;
    facts.raTrackingLimitDegrees = RA_TRACKING_LIMIT;
    facts.decLimitUpDegrees      = DEC_LIMIT_UP;
    facts.decLimitDownDegrees    = DEC_LIMIT_DOWN;
    facts.decUpperLimitSteps     = _settings.decUpperLimitSteps();
    facts.decLowerLimitSteps     = _settings.decLowerLimitSteps();
    facts.trackedHours           = getTrackedHours();
    facts.northernHemisphere     = inNorthernHemisphere;

    const core::ManualSlewTargets targets = core::manualSlewTargets(static_cast<uint8_t>(direction), facts);

    if (targets.movesDec)
    {
        OAT_LOG(_deps.logger,
                DEBUG_STEPPERS,
                "[STEPPERS]: startSlewing: DEC targetMoveTo is %ld (limits %ld -> %ld)",
                targets.decSteps,
                _settings.decLowerLimitSteps(),
                _settings.decUpperLimitSteps());
        _axes.dec()->moveTo(targets.decSteps);
        setStatusFlag(STATUS_SLEWING);
    }

    if (targets.movesRa)
    {
        OAT_LOG(_deps.logger,
                DEBUG_STEPPERS,
                "[STEPPERS]: startSlewing: RA targetMoveTo is %ld (adjusted for %fh tracked)",
                targets.raSteps,
                facts.trackedHours);
        _axes.ra()->moveTo(targets.raSteps);
        setStatusFlag(STATUS_SLEWING);
    }
}

/////////////////////////////////
void MountExecutor::stopSlewing(int direction)
{
    if (direction & TRACKING)
    {
        // Turn off tracking
        clearStatusFlag(STATUS_TRACKING);
        publishEvent(core::MountEventType::TrackingStopped);

        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: stopSlewing: TRK stepper stop()");
        _coordinator.tracking().stop();
    }

    if ((direction & (NORTH | SOUTH)) != 0)
    {
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: stopSlewing: DEC stepper stop()");
        _axes.dec()->stop();
    }

    if ((direction & (WEST | EAST)) != 0)
    {
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: stopSlewing: RA stepper stop()");
        _axes.ra()->stop();
        if (isFindingHome())
        {
            clearStatusFlag(STATUS_FINDING_HOME);
        }
    }
}

void MountExecutor::stopSlewing(StepperAxis axis)
{
    if (axis == RA_STEPS)
    {
        _axes.ra()->stop();
    }
    else if (axis == DEC_STEPS)
    {
        _axes.dec()->stop();
    }
    else if (axis == AZIMUTH_STEPS)
    {
        if (kHasAzStepper)
        {
            _axes.az()->stop();
        }
    }
    else if (axis == ALTITUDE_STEPS)
    {
        if (kHasAltStepper)
        {
            _axes.alt()->stop();
        }
    }
    else if (axis == FOCUS_STEPS)
    {
        if (kHasFocusStepper)
        {
            _axes.focus()->stop();
        }
    }
}

void MountExecutor::waitUntilStopped(byte direction)
{
    while (((direction & (EAST | WEST)) && _axes.ra()->isRunning()) || ((direction & (NORTH | SOUTH)) && _axes.dec()->isRunning())
           || ((direction & TRACKING) && ((!_coordinator.state().test(STATUS_TRACKING)) && _axes.tracking()->isRunning()))
           || ((direction & FOCUSING) && kHasFocusStepper && _axes.focus()->isRunning()))
    {
        loop();
        yield();
    }
}

void MountExecutor::waitUntilAllStopped()
{
    while (_axes.ra()->isRunning() || _axes.dec()->isRunning()
           || ((!_coordinator.state().test(STATUS_TRACKING)) && _axes.tracking()->isRunning())
           || (kHasFocusStepper && _axes.focus()->isRunning()) || (kHasAzStepper && _axes.az()->isRunning())
           || (kHasAltStepper && _axes.alt()->isRunning()))
    {
        loop();
        yield();
    }
}

/////////////////////////////////
void MountExecutor::setHome(bool clearZeroPos)
{
    OAT_LOG(_deps.logger, DEBUG_MOUNT, "[MOUNT]: setHome() called. Stopping steppers");
    bool wasTracking = isSlewingTRK();
    stopSlewing(ALL_DIRECTIONS);
    waitUntilStopped(ALL_DIRECTIONS);
    if (wasTracking)
    {
        OAT_LOG(_deps.logger, DEBUG_MOUNT, "[MOUNT]: setHome: Tracking was on, so start it again.");
        startSlewing(TRACKING);
    }

    //LOG(DEBUG_MOUNT_VERBOSE, "[MOUNT]: setHomePre: currentRA is %s", currentRA().ToString());
    //LOG(DEBUG_MOUNT_VERBOSE, "[MOUNT]: setHomePre: targetRA is %s", targetRA().ToString());
    //LOG(DEBUG_MOUNT_VERBOSE, "[MOUNT]: setHomePre: zeroPos is %s", DayTime(_siteTime.raZeroPoint()).ToString());
    _siteTime.setRaZeroPoint(clearZeroPos ? DayTime(POLARIS_RA_HOUR, POLARIS_RA_MINUTE, POLARIS_RA_SECOND) : calculateLst());
#if defined(OAM) || defined(OAE)
    _siteTime.adjustRaZeroPoint(6.0f);  // shift all coordinates by 90° for EQ mount movement
#endif
    _zeroPosDEC = 0.0f;

    _axes.ra()->setCurrentPosition(0);
    _axes.dec()->setCurrentPosition(0);
    _axes.tracking()->setCurrentPosition(0);
    _axes.guide()->setCurrentPosition(0);

    _targetRA = currentRA();
    _coordinator.slew().end();
    // `setHome()` is also called by the park sequence itself, which re-establishes its
    // phase straight afterwards; clearing here only affects a hand-driven home.
    if (_coordinator.parking().isParked())
    {
        _coordinator.parking().clear();
        applyParkingStatus();
    }

    //LOG(DEBUG_MOUNT_VERBOSE, "[MOUNT]: setHomePost: currentRA is %s", currentRA().ToString());
    //LOG(DEBUG_MOUNT_VERBOSE, "[MOUNT]: setHomePost: zeroPos is %s", DayTime(_siteTime.raZeroPoint()).ToString());
    //LOG(DEBUG_MOUNT_VERBOSE, "[MOUNT]: setHomePost: targetRA is %s", targetRA().ToString());
}

/////////////////////////////////
void MountExecutor::moveSteppersTo(float targetRASteps, float targetDECSteps, StepperAxis direction)
{  // Units are u-steps (in slew mode)
    // Show time: tell the steppers where to go!
    _coordinator.slew().clearBacklashCorrection();
    OAT_LOG(
        _deps.logger, DEBUG_STEPPERS, "[STEPPERS]: MoveSteppersTo: RA  From: %ld  To: %f", _axes.ra()->currentPosition(), targetRASteps);
    OAT_LOG(
        _deps.logger, DEBUG_STEPPERS, "[STEPPERS]: MoveSteppersTo: DEC From: %ld  To: %f", _axes.dec()->currentPosition(), targetDECSteps);

    if ((direction == RA_AND_DEC_STEPS) || (direction == RA_STEPS))
    {
        if ((_settings.backlashCorrectionSteps() != 0) && ((_axes.ra()->currentPosition() - targetRASteps) > 0))
        {
            OAT_LOG(_deps.logger,
                    DEBUG_STEPPERS,
                    "[STEPPERS]: MoveSteppersTo: Needs backlash correction of %d!",
                    _settings.backlashCorrectionSteps());
            targetRASteps -= _settings.backlashCorrectionSteps();
            _coordinator.slew().requestBacklashCorrection();
        }

        _axes.ra()->moveTo(targetRASteps);
    }

    if ((direction == RA_AND_DEC_STEPS) || (direction == DEC_STEPS))
    {
        const core::DecLimitResult limited
            = core::clampDecStepsToLimits(targetDECSteps, _settings.decLowerLimitSteps(), _settings.decUpperLimitSteps());
        if (limited.clampedToUpper)
        {
            OAT_LOG(_deps.logger,
                    DEBUG_STEPPERS,
                    "[STEPPERS]: MoveSteppersTo: DEC Upper Limit enforced. To: %ld",
                    _settings.decUpperLimitSteps());
        }
        if (limited.clampedToLower)
        {
            OAT_LOG(_deps.logger,
                    DEBUG_STEPPERS,
                    "[STEPPERS]: MoveSteppersTo: DEC Lower Limit enforced. To: %ld",
                    _settings.decLowerLimitSteps());
        }

        _axes.dec()->moveTo(limited.steps);
    }
}

/////////////////////////////////
void MountExecutor::moveStepperBy(StepperAxis direction, long steps)
{
    OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: moveStepperBy: %ld", steps);

    const core::RelativeMovePlan plan = core::planRelativeMove(direction, steps, isSlewingTRK());

    if (!plan.accepted)
    {
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: moveStepperBy: MoveStepperBy does not support multiple axes.");
        return;
    }

    if (plan.stopTracking)
    {
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: moveStepperBy: Stop tracking (NEMA steppers)");
        stopSlewing(TRACKING);
        _coordinator.tracking().noteStopped(_deps.clock->now());
        OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: moveStepperBy: TRK stopped at %ldms", _coordinator.tracking().stoppedAt());
    }

    switch (direction)
    {
        case RA_STEPS:
            // Slew microsteps once the TRK stepper has stopped, before the status bit says a
            // slew is in flight: the step generator reads that bit.
            setRaMicrosteps(_config.raSlewMicrosteps);
            setStatusFlag(STATUS_SLEWING | STATUS_SLEWING_TO_TARGET);
            publishEvent(core::MountEventType::SlewStarted);
            _coordinator.slew().noteRunning(true);
            moveSteppersTo(_axes.ra()->currentPosition() + steps, 0, direction);
            _coordinator.slew().setTotalMove(1.0f * _axes.ra()->distanceToGo(), _coordinator.slew().totalDecSteps());
            break;

        case DEC_STEPS:
            setStatusFlag(STATUS_SLEWING | STATUS_SLEWING_TO_TARGET);
            publishEvent(core::MountEventType::SlewStarted);
            // Normal state for DEC is guide microstepping, so switch to slew microstepping here.
            setDecMicrosteps(_config.decSlewMicrosteps);
            _coordinator.slew().noteRunning(true);
            moveSteppersTo(0, _axes.dec()->currentPosition() + steps, direction);
            _coordinator.slew().setTotalMove(_coordinator.slew().totalRaSteps(), 1.0f * _axes.dec()->distanceToGo());
            break;

        case FOCUS_STEPS:
            if (kHasFocusStepper)
            {
                focusMoveBy(steps);
            }
            break;

        case AZIMUTH_STEPS:
            if (kHasAzStepper)
            {
                enableAzMotor();
                OAT_LOG(_deps.logger,
                        DEBUG_STEPPERS,
                        "[STEPPERS]: moveStepperBy: AZ from %ld to %ld",
                        _axes.az()->currentPosition(),
                        _axes.az()->currentPosition() + steps);
                _axes.az()->moveTo(_axes.az()->currentPosition() + steps);
            }
            break;

        case ALTITUDE_STEPS:
            if (kHasAltStepper)
            {
                enableAltMotor();
                _axes.alt()->moveTo(_axes.alt()->currentPosition() + steps);
            }
            break;

        case RA_AND_DEC_STEPS:
            break;  // rejected above
    }
}

/////////////////////////////////
void MountExecutor::moveStepperTo(StepperAxis axis, long position)
{
    long curPos = getCurrentStepperPosition(axis);
    long delta  = position - curPos;
    moveStepperBy(axis, delta);
}

}  // namespace app
