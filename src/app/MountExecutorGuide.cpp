#include "MountExecutor.hpp"

#include "ports/ILogger.hpp"
#include "../core/SiderealClock.hpp"

/// Sidereal degrees per hour, for the guide-pulse rate. File-local: the pulse and drift arithmetic
/// are its only readers.
const float siderealDegreesInHour = 14.95904348958;

namespace app
{

/////////////////////////////////
void MountExecutor::stopGuiding(bool ra, bool dec)
{
    if (!isGuiding())
        return;

    // Stop RA guide first, since it's just a speed change back to tracking speed
    if (ra && _coordinator.state().test(STATUS_GUIDE_PULSE_RA))
    {
        OAT_LOG(_deps.logger,
                DEBUG_STEPPERS | DEBUG_GUIDE,
                "[GUIDE]: stopGuide:    TRK stop guide at  : %ld",
                _axes.tracking()->currentPosition());
        _coordinator.tracking().applySpeed();
        OAT_LOG(
            _deps.logger, DEBUG_STEPPERS | DEBUG_GUIDE, "[GUIDE]: stopGuide:    TRK speed set to   : %f", _coordinator.tracking().speed());
        _coordinator.guiding().clearRa();
        clearStatusFlag(STATUS_GUIDE_PULSE_RA);
    }

    if (dec && _coordinator.state().test(STATUS_GUIDE_PULSE_DEC))
    {
        OAT_LOG(_deps.logger,
                DEBUG_STEPPERS | DEBUG_GUIDE,
                "[GUIDE]: stopGuide:    DEC stop guide at   : %ld",
                _axes.guide()->currentPosition());

        // Stop DEC guiding and wait for it to stop.
        _axes.guide()->stop();

#if !defined(ESP32BOARD)
        while (_axes.guide()->isRunning())
        {
            _axes.guide()->run();
            _axes.tracking()->runSpeed();
        }
#endif

        _coordinator.guiding().clearDec();
        clearStatusFlag(STATUS_GUIDE_PULSE_DEC);
    }

    //disable pulse state if no direction is active
    if (!_coordinator.state().test(STATUS_GUIDE_PULSE_DIR))
    {
        OAT_LOG(_deps.logger, DEBUG_STEPPERS | DEBUG_GUIDE, "[GUIDE]: Clear guiding state");
        clearStatusFlag(STATUS_GUIDE_PULSE_MASK);
        publishEvent(types::MountEventType::GuidingStopped);
    }
    else
    {
        OAT_LOG(_deps.logger, DEBUG_STEPPERS | DEBUG_GUIDE, "[GUIDE]: One axis still guiding");
    }
}

/////////////////////////////////
void MountExecutor::guidePulse(byte direction, int duration)
{
#if (DEBUG_LEVEL != DEBUG_NONE)
    const char *directionName = "-NE-S---W";
#endif
    OAT_LOG(_deps.logger,
            DEBUG_STEPPERS | DEBUG_GUIDE,
            "[GUIDE]: guidePulse: > Guide Pulse %c for %dms requested",
            directionName[direction],
            duration);
    const bool wasGuiding = _coordinator.state().test(STATUS_GUIDE_PULSE);

    if (!admit(core::MountRequest::Guide))
    {
        OAT_LOG(_deps.logger, DEBUG_STEPPERS | DEBUG_GUIDE, "[GUIDE]: guidePulse: Not tracking (at limit?), ignoring guide pulse");
    }
    else
    {
        if ((direction == NORTH) || (direction == SOUTH))
        {
            OAT_LOG(_deps.logger,
                    DEBUG_STEPPERS | DEBUG_GUIDE,
                    "[GUIDE]: guidePulse:   DEC current steps   : %ld",
                    _axes.guide()->currentPosition());
            OAT_LOG(_deps.logger,
                    DEBUG_STEPPERS | DEBUG_GUIDE,
                    "[GUIDE]: guidePulse:   DEC steps/deg       : %f",
                    _settings.stepsPerDegree(DEC_STEPS));
            OAT_LOG(_deps.logger,
                    DEBUG_STEPPERS | DEBUG_GUIDE,
                    "[GUIDE]: guidePulse:   DEC Microstep ratio : %f",
                    (1.0 * _config.decGuidingMicrosteps() / _config.decSlewMicrosteps));
        }
        else
        {
            OAT_LOG(_deps.logger,
                    DEBUG_STEPPERS | DEBUG_GUIDE,
                    "[GUIDE]: guidePulse:   RA  current steps   : %ld",
                    _axes.tracking()->currentPosition());
            OAT_LOG(_deps.logger,
                    DEBUG_STEPPERS | DEBUG_GUIDE,
                    "[GUIDE]: guidePulse:   RA  steps/deg       : %f",
                    _settings.stepsPerDegree(RA_STEPS));
            OAT_LOG(_deps.logger,
                    DEBUG_STEPPERS | DEBUG_GUIDE,
                    "[GUIDE]: guidePulse:   RA  Microstep ratio : %f",
                    (1.0 * _config.raTrackingMicrosteps() / _config.raSlewMicrosteps));
        }

        // DEC stepper moves at sidereal rate in both directions
        // RA stepper moves at either 2.5x sidereal rate or 0.5x sidereal rate.
        // Also compensate for microstepping mode change between slew & guiding/tracking
        const core::GuideAxisGeometry raGeometry
            = {_settings.stepsPerDegree(RA_STEPS), _config.raTrackingMicrosteps(), _config.raSlewMicrosteps, RA_PULSE_MULTIPLIER};
        const core::GuideAxisGeometry decGeometry
            = {_settings.stepsPerDegree(DEC_STEPS), _config.decGuidingMicrosteps(), _config.decSlewMicrosteps, DEC_PULSE_MULTIPLIER};

        // TODO: Do we need to track how many steps the steppers took and add them to the GoHome calculation?
        // If so, we need to remember where we were when we started the guide pulse. Then at the end,
        // we can calculate the difference. Ignore DEC Guide for now.
        // TODO: Take guide pulses on DEC into account

        switch (direction)
        {
            case NORTH:
                {
                    // If a upper limit is set, check we're not there. If we are, refuse the guide pulse.
                    const core::DecPulse pulse = core::GuidingController::decPulse(true,
                                                                                   decGeometry,
                                                                                   siderealDegreesInHour,
                                                                                   _axes.dec()->currentPosition(),
                                                                                   _settings.decLowerLimitSteps(),
                                                                                   _settings.decUpperLimitSteps());
                    if (!pulse.allowed)
                    {
                        // TODO: Make sure Southern hemisphere does not need a sign/direction inversion.
                        OAT_LOG(_deps.logger,
                                DEBUG_STEPPERS | DEBUG_GUIDE,
                                "[GUIDE]: guidePulse:   DEC is at (%ld) which is at or above upper limit (%ld), ignoring guide pulse",
                                _axes.dec()->currentPosition(),
                                _settings.decUpperLimitSteps());
                    }
                    else
                    {
                        OAT_LOG(
                            _deps.logger, DEBUG_STEPPERS | DEBUG_GUIDE, "[GUIDE]: guidePulse:   DEC base speed      : %f", pulse.baseSpeed);
                        OAT_LOG(_deps.logger,
                                DEBUG_STEPPERS | DEBUG_GUIDE,
                                "[GUIDE]: guidePulse:   DEC guide speed     : %f",
                                pulse.targetSpeed);
                        _axes.guide()->setSpeed(pulse.targetSpeed);
                        setStatusFlag(STATUS_GUIDE_PULSE | STATUS_GUIDE_PULSE_DEC);
                        _coordinator.guiding().startDec(duration);
                    }
                }
                break;

            case SOUTH:
                {
                    const core::DecPulse pulse = core::GuidingController::decPulse(false,
                                                                                   decGeometry,
                                                                                   siderealDegreesInHour,
                                                                                   _axes.dec()->currentPosition(),
                                                                                   _settings.decLowerLimitSteps(),
                                                                                   _settings.decUpperLimitSteps());
                    if (!pulse.allowed)
                    {
                        OAT_LOG(_deps.logger,
                                DEBUG_STEPPERS | DEBUG_GUIDE,
                                "[GUIDE]: guidePulse:   DEC is at (%ld) which is at or below lower limit (%ld), ignoring guide pulse",
                                _axes.dec()->currentPosition(),
                                _settings.decLowerLimitSteps());
                        OAT_LOG(_deps.logger,
                                DEBUG_STEPPERS | DEBUG_GUIDE,
                                "[GUIDE]: guidePulse:   DEC is at lower limit, ignoring guide pulse");
                    }
                    else
                    {
                        OAT_LOG(
                            _deps.logger, DEBUG_STEPPERS | DEBUG_GUIDE, "[GUIDE]: guidePulse:   DEC base speed      : %f", pulse.baseSpeed);
                        OAT_LOG(_deps.logger,
                                DEBUG_STEPPERS | DEBUG_GUIDE,
                                "[GUIDE]: guidePulse:   DEC guide speed     : %f",
                                pulse.targetSpeed);
                        _axes.guide()->setSpeed(pulse.targetSpeed);
                        setStatusFlag(STATUS_GUIDE_PULSE | STATUS_GUIDE_PULSE_DEC);
                        _coordinator.guiding().startDec(duration);
                    }
                }
                break;

            case WEST:
                {
                    // We were in tracking mode before guiding, so no need to update microstepping mode on RA driver
                    const core::RaPulse pulse
                        = core::GuidingController::raPulse(true, raGeometry, siderealDegreesInHour, _coordinator.tracking().calibration());
                    OAT_LOG(_deps.logger, DEBUG_STEPPERS | DEBUG_GUIDE, "[GUIDE]: guidePulse:   RA  base speed      : %f", pulse.baseSpeed);
                    OAT_LOG(_deps.logger,
                            DEBUG_STEPPERS | DEBUG_GUIDE,
                            "[GUIDE]: guidePulse:   RA  speed factor    : %f",
                            _coordinator.tracking().calibration());
                    OAT_LOG(
                        _deps.logger, DEBUG_STEPPERS | DEBUG_GUIDE, "[GUIDE]: guidePulse:   RA  adjusted speed  : %f", pulse.adjustedSpeed);
                    OAT_LOG(_deps.logger,
                            DEBUG_STEPPERS | DEBUG_GUIDE,
                            "[GUIDE]: guidePulse:   RA  guide speed     : %f (%f x adjusted speed)",
                            pulse.targetSpeed,
                            RA_PULSE_MULTIPLIER);
                    _axes.tracking()->setSpeed(pulse.targetSpeed);  // Faster than siderael
                    setStatusFlag(STATUS_GUIDE_PULSE | STATUS_GUIDE_PULSE_RA);
                    _coordinator.guiding().startRa(duration);
                }
                break;

            case EAST:
                {
                    // We were in tracking mode before guiding, so no need to update microstepping mode on RA driver
                    const core::RaPulse pulse
                        = core::GuidingController::raPulse(false, raGeometry, siderealDegreesInHour, _coordinator.tracking().calibration());
                    OAT_LOG(_deps.logger, DEBUG_STEPPERS | DEBUG_GUIDE, "[GUIDE]: guidePulse:   RA  base speed      : %f", pulse.baseSpeed);
                    OAT_LOG(_deps.logger,
                            DEBUG_STEPPERS | DEBUG_GUIDE,
                            "[GUIDE]: guidePulse:   RA  speed factor    : %f",
                            _coordinator.tracking().calibration());
                    OAT_LOG(
                        _deps.logger, DEBUG_STEPPERS | DEBUG_GUIDE, "[GUIDE]: guidePulse:   RA  adjusted speed  : %f", pulse.adjustedSpeed);
                    OAT_LOG(_deps.logger,
                            DEBUG_STEPPERS | DEBUG_GUIDE,
                            "[GUIDE]: guidePulse:   RA  guide speed     : %f (%f x adjusted speed)",
                            (2.0 - RA_PULSE_MULTIPLIER * pulse.adjustedSpeed),
                            (2.0 - RA_PULSE_MULTIPLIER));
                    _axes.tracking()->setSpeed(pulse.targetSpeed);  // Slower than siderael
                    setStatusFlag(STATUS_GUIDE_PULSE | STATUS_GUIDE_PULSE_RA);
                    _coordinator.guiding().startRa(duration);
                }
                break;
        }
    }
    if (!wasGuiding && _coordinator.state().test(STATUS_GUIDE_PULSE))
    {
        publishEvent(types::MountEventType::GuidingStarted);
    }

    OAT_LOG(_deps.logger, DEBUG_STEPPERS | DEBUG_GUIDE, "[GUIDE]: guidePulse: < Guide Pulse");
}

#if SUPPORT_DRIFT_ALIGNMENT == 1
/////////////////////////////////
void MountExecutor::startDriftAlignment(int durationSecs)
{
    _coordinator.drift().begin(durationSecs, _settings.stepsPerDegree(RA_STEPS));
}

void MountExecutor::updateDriftAlignment(unsigned long now)
{
    if (!_coordinator.drift().isActive())
    {
        return;
    }

    const bool raRunning  = _axes.ra()->isRunning();
    const bool decRunning = _axes.dec()->isRunning();

    switch (_coordinator.drift().update(!raRunning && !decRunning, now))
    {
        case core::DriftAction::None:
            break;

        case core::DriftAction::StopAxes:
            stopSlewing(ALL_DIRECTIONS | TRACKING);
            break;

        case core::DriftAction::MoveEastPass:
            _axes.ra()->setMaxSpeed(_coordinator.drift().passSpeed());
            _axes.ra()->move(_coordinator.drift().passSteps());
            break;

        case core::DriftAction::MoveEastGap:
            _axes.ra()->setMaxSpeed(core::DriftAlignmentController::kGapSpeedStepsSec);
            _axes.ra()->move(-core::DriftAlignmentController::kGearingGapSteps);
            break;

        case core::DriftAction::MoveWestPass:
            _axes.ra()->setMaxSpeed(_coordinator.drift().passSpeed());
            _axes.ra()->move(-_coordinator.drift().passSteps());
            break;

        case core::DriftAction::ResetGap:
            _axes.ra()->setMaxSpeed(core::DriftAlignmentController::kGapSpeedStepsSec);
            _axes.ra()->move(core::DriftAlignmentController::kGearingGapSteps);
            break;

        case core::DriftAction::ResumeTracking:
            // Put the stepper back into its configured slew parameters before tracking resumes.
            _axes.ra()->setMaxSpeed(_axes.limits().raSpeed);
            startSlewing(TRACKING);
            break;
    }
}
#endif

}  // namespace app
