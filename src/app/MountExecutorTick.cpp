#include "MountExecutor.hpp"

#include "ports/ILogger.hpp"
#include "../core/ActivityState.hpp"
#include "../core/SlewCompletion.hpp"

namespace app
{

/////////////////////////////////
void MountExecutor::delay(int ms)
{
    // A bounded wait: the deadline comes from the clock port and the yield from the scheduler
    // port, so this needs no direct millis()/yield().
    //
    // It deliberately does **not** call `loop()` any more. Every caller is a menu screen, i.e.
    // already inside a tick, so re-entering the update path from here ran the arrival sequence and
    // the coordinator's tick from inside a handler — re-entrancy nobody could reason about, and
    // the reason a menu calibration could interleave with a slew arriving. The steppers still step
    // (the AVR timer ISR, or the ESP32 stepper task), so a wait does not stop motion; what it no
    // longer does is run the mount's bookkeeping behind the menu's back.
    const unsigned long start = _deps.clock->now();
    while (_deps.clock->now() - start < (unsigned long) ms)
    {
        _deps.scheduler->yield();
    }
}

/////////////////////////////////
void MountExecutor::loop()
{
    bool raStillRunning  = false;
    bool decStillRunning = false;

    unsigned long now = _deps.clock->now();

#if (DEBUG_LEVEL & DEBUG_MOUNT) && (DEBUG_LEVEL & DEBUG_VERBOSE)
    if (now - _lastMountPrint > 2000)
    {
        OAT_LOG(_deps.logger, DEBUG_MOUNT, "[MOUNT]: Status -> %s", getStatusString());
        _lastMountPrint = now;
    }
#endif

    if (_coordinator.focus().isRunning())
    {
        OAT_LOG(_deps.logger, DEBUG_FOCUS, "[MOUNT]: Loop: Focuser running at speed %f", _coordinator.focus().speed());
    }

    bool oneIsRunning = false;
    if (kHasAzStepper)
    {
        oneIsRunning |= _axes.az()->isRunning();
    }
    if (kHasAltStepper)
    {
        oneIsRunning |= _axes.alt()->isRunning();
    }

    // The controller half of a tick: dispatch published facts, poll the axes, then the guide
    // pulse. Everything it decides is reported back, because the pins and the EEPROM live
    // here. See `core::MountCoordinator::tick()`.
    const core::TickOutcome tick = _coordinator.tick(oneIsRunning);

    if (tick.azAltJustStopped)
    {
        // One of the motors was running last time through the loop, but not anymore, so shutdown the outputs.
        disableAzAltMotors();
        if (kHasAzStepper)
        {
            _deps.settings->storeAZPosition(_axes.az()->currentPosition());
        }
        if (kHasAltStepper)
        {
            _deps.settings->storeALTPosition(_axes.alt()->currentPosition());
        }
    }

    if (tick.focusStopped)
    {
        OAT_LOG(_deps.logger, DEBUG_FOCUS, "[MOUNT]: Loop: Focuser is stopped, but was running ");
        OAT_LOG(_deps.logger, DEBUG_FOCUS, "[MOUNT]: Loop: Focuser is stopped, but was running, disabling");
        disableFocusMotor();
        publishEvent(types::MountEventType::FocusStopped);
    }

    if (tick.guiding)
    {
        now = _deps.clock->now();
        if (tick.guidingRaExpired || tick.guidingDecExpired)
        {
            OAT_LOG(_deps.logger,
                    DEBUG_GUIDE,
                    "[MOUNT]: Loop: StopGuiding. Now: %ld, RA End: %ld, DEC End: %ld",
                    now,
                    _coordinator.guiding().raDeadline(),
                    _coordinator.guiding().decDeadline());
            stopGuiding(tick.guidingRaExpired, tick.guidingDecExpired);
        }
        return;
    }

    if (_axes.dec()->isRunning())
    {
        decStillRunning = true;
    }

    if (_axes.ra()->isRunning())
    {
        raStillRunning = true;
    }

    {
        // Check whether we should stop tracking every 5 seconds
        if (now - _lastTRKCheck > 5000)
        {
            checkRALimit();
        }

        //
        // Arrived at target after Slew!
        //
        clearStatusFlag(STATUS_SLEWING | STATUS_SLEWING_TO_TARGET | STATUS_SLEWING_MANUAL);

        if (_coordinator.slew().wasRunning())
        {
            publishEvent(types::MountEventType::SlewArrived);
            OAT_LOG(_deps.logger,
                    DEBUG_MOUNT | DEBUG_STEPPERS,
                    "[MOUNT]: Loop: Reached target. RA:%ld, DEC:%ld",
                    _axes.ra()->currentPosition(),
                    _axes.dec()->currentPosition());

            // The rules for what happens at the end of a slew live in
            // core/SlewCompletion.hpp, as a plan this loop executes in order. The facts they
            // are decided from belong to the controllers, so the coordinator assembles them.
            const core::CompletionPlan plan = _coordinator.completionPlan();

            // Set once the park leg is programmed: true when there is nowhere to go.
            bool parkLegIsNoOp = false;

            // A park just reached home: stop tracking, and record the home position
            // unless this is the second leg (which starts from it).
            if (plan.stopTracking)
            {
                stopSlewing(TRACKING);
            }
            if (plan.setHome)
            {
                // setHome() resets the slew, and the rest of this block still needs to
                // know what the slew was for.
                const core::SlewSnapshot savedSlew = _coordinator.slew().snapshot();
                OAT_LOG(_deps.logger,
                        DEBUG_MOUNT | DEBUG_STEPPERS,
                        "[MOUNT]: Loop:   Was Parking, not slewing to park so stop tracking and set home. SlewToHome: %d",
                        _coordinator.slew().intent() == core::SlewIntent::Home);
                setHome(false);
                _coordinator.slew().restore(savedSlew);
            }

            _currentRAStepperPosition = _axes.ra()->currentPosition();
            if (plan.raTrackingMicrosteps)  // RA stays in slew configuration while homing
            {
                setRaMicrosteps(_config.raTrackingMicrosteps());
            }

            if (plan.compensateTracker)
            {
                now                          = _deps.clock->now();
                const long compensationSteps = _coordinator.tracking().compensationSteps(now);
                OAT_LOG(_deps.logger,
                        DEBUG_STEPPERS,
                        "[STEPPERS]: loop: Arrived at %ldms. Tracking was off for %ldms (%ld steps), compensating.",
                        now,
                        now - _coordinator.tracking().stoppedAt(),
                        compensationSteps);
                _coordinator.tracking().applyCompensation(compensationSteps);

                OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: loop: compensation complete.");
            }

            if (plan.startTracking)
            {
                OAT_LOG(_deps.logger, DEBUG_STEPPERS, "[STEPPERS]: Loop: Not parking and not finding home, so start tracking");
                startSlewing(TRACKING);
            }

            // Guiding is always ready again once the slew is over.
            if (plan.decGuideMicrosteps)
            {
                setDecMicrosteps(_config.decGuidingMicrosteps());
            }

            if (plan.correctBacklash)
            {
                OAT_LOG(_deps.logger,
                        DEBUG_MOUNT | DEBUG_STEPPERS,
                        "[MOUNT]: Loop:   Reached target at %d. Compensating for backlash by %d",
                        (int) _currentRAStepperPosition,
                        _settings.backlashCorrectionSteps());
                _currentRAStepperPosition += _settings.backlashCorrectionSteps();
                _axes.ra()->runToNewPosition(_currentRAStepperPosition);
                _coordinator.slew().clearBacklashCorrection();
                OAT_LOG(_deps.logger,
                        DEBUG_MOUNT | DEBUG_STEPPERS,
                        "[MOUNT]: Loop:   Backlash correction done. Pos: %d",
                        _currentRAStepperPosition);
            }
            else
            {
                OAT_LOG(_deps.logger,
                        DEBUG_MOUNT | DEBUG_STEPPERS,
                        "[MOUNT]: Loop:   Reached target at %d, no backlash compensation needed",
                        _currentRAStepperPosition);
            }

            if (plan.retargetRa)
            {
                OAT_LOG(_deps.logger, DEBUG_MOUNT | DEBUG_STEPPERS, "[MOUNT]: Loop:   Was Slewing home...");
                _targetRA = currentRA();
            }

            if (plan.startParkPositionLeg)
            {
                // Home is known; now go to the configured park position.
                setDecMicrosteps(_config.decSlewMicrosteps);
                OAT_LOG(_deps.logger,
                        DEBUG_MOUNT | DEBUG_STEPPERS,
                        "[MOUNT]: Loop:   Was parking, so no tracking. Proceeding to park position...");
                _coordinator.parking().beginPositionLeg();
                applyParkingStatus();
                _axes.ra()->moveTo(-getHomingOffset(StepperAxis::RA_STEPS));
                _axes.dec()->moveTo(-getHomingOffset(StepperAxis::DEC_STEPS));
                _coordinator.slew().setTotalMove(1.0f * _axes.ra()->distanceToGo(), 1.0f * _axes.dec()->distanceToGo());
                OAT_LOG(_deps.logger,
                        DEBUG_MOUNT | DEBUG_STEPPERS,
                        "[MOUNT]: Loop:   Park Position is R:%ld  D:%ld, TotalMove is R:%f, D:%f",
                        -getHomingOffset(StepperAxis::RA_STEPS),
                        -getHomingOffset(StepperAxis::DEC_STEPS),
                        _coordinator.slew().totalRaSteps(),
                        _coordinator.slew().totalDecSteps());
                parkLegIsNoOp = core::SlewCompletion::parkLegIsNoOp(_axes.ra()->distanceToGo(), _axes.dec()->distanceToGo());
                if (parkLegIsNoOp)
                {
                    OAT_LOG(_deps.logger, DEBUG_MOUNT | DEBUG_STEPPERS, "[MOUNT]: Loop:   Already at Parking pos, so done.");
                }
                else
                {
                    OAT_LOG(_deps.logger,
                            DEBUG_MOUNT | DEBUG_STEPPERS,
                            "[MOUNT]: Loop:   Distance to Parking is non-zero, slewing to park position...");
                    setStatusFlag(STATUS_PARKING_POS | STATUS_SLEWING);
                }
            }

            if (plan.restartTrackingAfterHome)
            {
                OAT_LOG(_deps.logger, DEBUG_MOUNT | DEBUG_STEPPERS, "[MOUNT]: Loop:   Slewed home, not parking, so restart tracking.");
                startSlewing(TRACKING);
            }

            if (plan.decGuideAfterHome)
            {
                setDecMicrosteps(_config.decGuidingMicrosteps());
            }

            if (plan.parkComplete)
            {
                OAT_LOG(_deps.logger, DEBUG_MOUNT | DEBUG_STEPPERS, "[MOUNT]: Loop:   Arrived at park position...");
            }

            // Both ways of finishing a park end in the same two writes.
            if (plan.parkComplete || parkLegIsNoOp)
            {
                noInterrupts();
                setStatus(STATUS_PARKED);
                interrupts();
                publishEvent(types::MountEventType::Parked);
                _coordinator.parking().markParked();
                applyParkingStatus();
            }

            if (plan.endSlew)
            {
                _coordinator.slew().end();
            }
            _coordinator.slew().clearTotalMove();
        }
    }

#if SUPPORT_DRIFT_ALIGNMENT == 1
    updateDriftAlignment(now);
#endif

    if (_coordinator.state().test(STATUS_FINDING_HOME))
    {
        processHomingProgress();
    }

    pollEndSwitches();

    _coordinator.finishTick(raStillRunning, decStillRunning);
}

}  // namespace app
