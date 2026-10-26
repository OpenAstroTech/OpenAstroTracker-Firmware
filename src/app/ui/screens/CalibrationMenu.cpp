#include "app/ui/screens/CalibrationMenu.hpp"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "core/MathUtil.hpp"
#include "ports/LogFormat.hpp"

namespace
{

/// The legacy screen's repeat delay: it starts here and shrinks by 6% per step while a key is held.
constexpr int kInitialRepeatDelay = 150;
constexpr int kMinimumRepeatDelay = 25;

/// The speed editor shrinks faster and stops sooner.
constexpr int kMinimumSpeedRepeatDelay = 5;

/// How long a confirmation row stays up before the screen returns to the menu.
constexpr unsigned kStoredDelay      = 500;
constexpr unsigned kLimitStoredDelay = 800;
constexpr unsigned kAlignmentDelay   = 750;

/// The DEC limits confirmation row, and where the cursor sits for each answer.
constexpr const char *kLimitChoices = " Set  Clr  Cancl";
constexpr const char *kLevelChoices = " Yes  No  ";

/// The drift-alignment durations, in seconds: 1 m, 2 m, 3 m, 5 m of *run*, less the padding.
constexpr uint8_t kDriftDurations[] = {27, 57, 87, 147};

/// The celestial declination the polar-alignment step points at: as far past Polaris as Polaris is
/// from the pole, which puts the DEC axis along the pole-Polaris line.
constexpr int kPolarDecDegrees = 88;
constexpr int kPolarDecMinutes = 42;
constexpr int kPolarDecSeconds = 11;

/// Polaris itself, for the sync at the end of the step.
constexpr int kPolarisDecDegrees = 89;
constexpr int kPolarisDecMinutes = 21;
constexpr int kPolarisDecSeconds = 6;

}  // namespace

namespace app
{

namespace
{

/// Queues one confirmation row, in order.
void addMessage(CalibrationOutcome &outcome, const char *text, unsigned delay)
{
    if (outcome.messageCount < 2)
    {
        outcome.messages[outcome.messageCount]      = text;
        outcome.messageDelays[outcome.messageCount] = delay;
        outcome.messageCount++;
    }
}

}  // namespace

CalibrationMenu::CalibrationMenu(const Board &board) : _board(board)
{
    _ring[_ringSize++] = State::HighlightPolar;
    if (_board.hasDriftAlignment)
    {
        _ring[_ringSize++] = State::HighlightDrift;
    }
    _ring[_ringSize++] = State::HighlightSpeed;
    _ring[_ringSize++] = State::HighlightRaSteps;
    _ring[_ringSize++] = State::HighlightDecSteps;
    _ring[_ringSize++] = State::HighlightBacklash;
    _ring[_ringSize++] = State::HighlightDecLowerLimit;
    _ring[_ringSize++] = State::HighlightDecUpperLimit;
    _ring[_ringSize++] = State::HighlightUtcOffset;
    if (_board.hasAzimuth)
    {
        _ring[_ringSize++] = State::HighlightAzimuth;
    }
    if (_board.hasAltitude)
    {
        _ring[_ringSize++] = State::HighlightAltitude;
    }
    if (_board.hasLevelSensor)
    {
        _ring[_ringSize++] = State::HighlightRoll;
        _ring[_ringSize++] = State::HighlightPitch;
    }
    _ring[_ringSize++] = State::HighlightBacklight;

    _state = _ring[0];
}

void CalibrationMenu::enter()
{
    _state       = _ring[0];
    _repeatDelay = kInitialRepeatDelay;
}

bool CalibrationMenu::isHighlight(State state) const
{
    return state <= State::HighlightBacklight;
}

void CalibrationMenu::moveHighlight(int direction, UiPorts &ports, ports::IDisplay &display)
{
    int index = 0;
    for (uint8_t i = 0; i < _ringSize; i++)
    {
        if (_ring[i] == _state)
        {
            index = i;
            break;
        }
    }

    index  = core::adjustWrap(index, direction, 0, static_cast<int>(_ringSize) - 1);
    _state = _ring[index];

    // The value edited by an entry is read back from the mount when the entry is selected: the
    // mount is the store of record, so the menu must not show a stale copy.
    switch (_state)
    {
        case State::HighlightRaSteps:
            _raStepsPerDegree = static_cast<long>(ports.config->getStepsPerDegree(RA_STEPS) * 10.0F);
            break;
        case State::HighlightDecSteps:
            _decStepsPerDegree = static_cast<long>(ports.config->getStepsPerDegree(DEC_STEPS) * 10.0F);
            break;
        case State::HighlightBacklash:
            _backlashSteps = ports.config->getBacklashCorrection();
            break;
        case State::HighlightSpeed:
            _speedCalibration = (ports.config->getSpeedCalibration() - 1.0F) * 10000.0F + 0.5F;
            break;
        case State::HighlightBacklight:
            _brightness = display.backlight();
            break;
        case State::HighlightUtcOffset:
            _utcOffset = ports.siteTime->getLocalUtcOffset();
            break;
        default:
            break;
    }
}

bool CalibrationMenu::updateProgressive(long &value, int minimumDelay, const MenuKeyInput &input, UiPorts &ports)
{
    if (input.held != core::Button::Up && input.held != core::Button::Down)
    {
        _repeatDelay = kInitialRepeatDelay;
        return true;
    }

    value += (input.held == core::Button::Up) ? 1 : -1;
    ports.delay(_repeatDelay);

    // The longer the key is held, the faster the value runs: 6% off the delay per step, floored.
    const int shrunk = static_cast<int>(0.94 * static_cast<float>(_repeatDelay));
    _repeatDelay     = (shrunk < minimumDelay) ? minimumDelay : shrunk;
    return false;
}

bool CalibrationMenu::updateSpeedCalibration(const MenuKeyInput &input, UiPorts &ports)
{
    if (input.held != core::Button::Up && input.held != core::Button::Down)
    {
        _repeatDelay = kInitialRepeatDelay;
        return true;
    }

    // A ten-thousandth at a time, guarding the 16-bit signed range the legacy editor assumed.
    if (input.held == core::Button::Up && _speedCalibration < 32760.0F)
    {
        _speedCalibration += 1.0F;
    }
    else if (input.held == core::Button::Down && _speedCalibration > -32760.0F)
    {
        _speedCalibration -= 1.0F;
    }
    ports.config->setSpeedCalibration(1.0F + (_speedCalibration / 10000.0F), false);

    ports.delay(_repeatDelay);
    const int shrunk = static_cast<int>(0.96 * static_cast<float>(_repeatDelay));
    _repeatDelay     = (shrunk < kMinimumSpeedRepeatDelay) ? kMinimumSpeedRepeatDelay : shrunk;
    return false;
}

void CalibrationMenu::updatePolarDrive(core::Button held, UiPorts &ports)
{
    const bool up    = (held == core::Button::Up);
    const bool down  = (held == core::Button::Down);
    const bool right = (held == core::Button::Right);
    const bool left  = (held == core::Button::Left);

    if (_board.hasAltitude && (up || down))
    {
        if (!ports.read->isAxisRunning(ALTITUDE_STEPS))
        {
            ports.config->setSpeed(ALTITUDE_STEPS, up ? _board.altitudeSpeed : -_board.altitudeSpeed);
        }
    }
    if (_board.hasAzimuth && (right || left))
    {
        if (!ports.read->isAxisRunning(AZIMUTH_STEPS))
        {
            ports.config->setSpeed(AZIMUTH_STEPS, right ? _board.azimuthSpeed : -_board.azimuthSpeed);
        }
    }

    if (held != core::Button::None)
    {
        return;
    }

    // Released: stop whichever axis is still running. Note the legacy code tested the *altitude* axis
    // with a negated `isAxisRunning()` and re-commanded a downwards speed on release; that is kept
    // verbatim, because it is what these boards have always done.
    if (_board.hasAzimuth && ports.read->isAxisRunning(AZIMUTH_STEPS))
    {
        ports.config->setSpeed(AZIMUTH_STEPS, 0.0F);
    }
    if (_board.hasAltitude && !ports.read->isAxisRunning(ALTITUDE_STEPS))
    {
        ports.config->setSpeed(ALTITUDE_STEPS, -_board.altitudeSpeed);
    }
}

CalibrationOutcome CalibrationMenu::handleKey(
    const MenuKeyInput &input, UiPorts &ports, ports::IGyro *gyro, ports::IDisplay &display, bool startupWaitingForRoll)
{
    CalibrationOutcome outcome;

    if (_board.hasLevelSensor && !_gyroStarted && (gyro != nullptr))
    {
        gyro->begin();
        _gyroStarted = true;
    }

    if (_board.hasLevelSensor && startupWaitingForRoll && (_state != State::RollOffsetCalibration))
    {
        // The wizard sent us here to level the mount: go straight to the roll readout.
        _state = State::RollOffsetCalibration;
    }

    // The states that act without a key press run first, exactly as the legacy dispatch did.
    if (_state == State::DriftRunning)
    {
        display.writeLine(1, "Drift alignment...");
        ports.motion->startDriftAlignment(kDriftDurations[_driftSubIndex]);
        _state = State::HighlightDrift;
        return outcome;
    }
    if (_state == State::PolarWaitHome)
    {
        if (!ports.motion->isSlewingRAorDEC())
        {
            outcome.resumeMenuUpdate = true;
            _state                   = State::HighlightPolar;
        }
        // A key still has to be able to cancel out of the homing move.
    }

    switch (_state)
    {
        case State::SpeedCalibration:
            if (!updateSpeedCalibration(input, ports))
            {
                return outcome;
            }
            break;

        case State::UtcOffsetCalibration:
            {
                long value = _utcOffset;
                if (!updateProgressive(value, kMinimumRepeatDelay, input, ports))
                {
                    _utcOffset = static_cast<int>(value);
                    return outcome;
                }
            }
            break;

        case State::AzimuthAdjustment:
            {
                long value = _azimuthMinutes;
                if (!updateProgressive(value, kMinimumRepeatDelay, input, ports))
                {
                    _azimuthMinutes = core::clamp(static_cast<int>(value), -60, 60);
                    return outcome;
                }
            }
            break;

        case State::AltitudeAdjustment:
            {
                long value = _altitudeMinutes;
                if (!updateProgressive(value, kMinimumRepeatDelay, input, ports))
                {
                    _altitudeMinutes = core::clamp(static_cast<int>(value), -60, 60);
                    return outcome;
                }
            }
            break;

        case State::RaStepCalibration:
            if (!updateProgressive(_raStepsPerDegree, kMinimumSpeedRepeatDelay, input, ports))
            {
                return outcome;
            }
            break;

        case State::DecStepCalibration:
            if (!updateProgressive(_decStepsPerDegree, kMinimumSpeedRepeatDelay, input, ports))
            {
                return outcome;
            }
            break;

        case State::BacklashCalibration:
            {
                long value = _backlashSteps;
                if (!updateProgressive(value, kMinimumRepeatDelay, input, ports))
                {
                    _backlashSteps = static_cast<int>(value);
                    return outcome;
                }
            }
            break;

        case State::BacklightCalibration:
            {
                long value = _brightness;
                if (!updateProgressive(value, kMinimumRepeatDelay, input, ports))
                {
                    uint8_t minimum = 0;
                    uint8_t maximum = 255;
                    display.backlightRange(&minimum, &maximum);
                    _brightness = core::clamp(static_cast<int>(value), static_cast<int>(minimum), static_cast<int>(maximum));
                    display.setBacklight(static_cast<uint8_t>(_brightness), false);
                    return outcome;
                }
            }
            break;

        case State::PolarWaitCenter:
            updatePolarDrive(input.held, ports);
            break;

        default:
            break;
    }

    if (input.edge == core::Button::None)
    {
        return outcome;
    }

    const core::Button key = input.edge;

    if (isHighlight(_state))
    {
        return handleHighlightKey(key, ports, display, outcome);
    }

    switch (_state)
    {
        case State::PolarWaitCenter:
            if (key == core::Button::Select)
            {
                _state = State::PolarWaitHome;
                addMessage(outcome, "Aligned, homing", kAlignmentDelay);

                // Pointing at Polaris, so say so, then park: the DEC axis is now along the pole.
                const int hemisphere = (ports.siteTime->latitude().getTotalHours() >= 0.0F) ? 90 : -90;
                ports.motion->syncPosition(ports.read->currentRA(),
                                           core::Declination(kPolarisDecDegrees - hemisphere, kPolarisDecMinutes, kPolarisDecSeconds));
                ports.motion->startSlewingToHome();
            }
            break;

        case State::PolarWaitHome:
            if (key == core::Button::Right)
            {
                ports.motion->stopSlewing(ALL_DIRECTIONS);
                ports.waitUntilStopped(ALL_DIRECTIONS);
                outcome.result       = MenuKeyResult::NextScreen;
                outcome.shutdownGyro = _board.hasLevelSensor;
                _state               = State::HighlightPolar;
            }
            break;

        case State::DriftWait:
            if (key == core::Button::Down || key == core::Button::Left)
            {
                _driftSubIndex = static_cast<uint8_t>(core::adjustWrap(_driftSubIndex, 1, 0, 3));
            }
            else if (key == core::Button::Up)
            {
                _driftSubIndex = static_cast<uint8_t>(core::adjustWrap(_driftSubIndex, -1, 0, 3));
            }
            else if (key == core::Button::Select)
            {
                _state = State::DriftRunning;
            }
            else if (key == core::Button::Right)
            {
                _state         = State::HighlightDrift;
                _driftSubIndex = 1;
            }
            break;

        case State::SpeedCalibration:
            if (key == core::Button::Select || key == core::Button::Right)
            {
                ports.config->setSpeedCalibration(1.0F + (_speedCalibration / 10000.0F), true);
                if (key == core::Button::Right)
                {
                    outcome.result       = MenuKeyResult::NextScreen;
                    outcome.shutdownGyro = _board.hasLevelSensor;
                }
                else
                {
                    addMessage(outcome, "Speed Stored.", kStoredDelay);
                }
                _state = State::HighlightSpeed;
            }
            break;

        case State::RaStepCalibration:
            if (key == core::Button::Select)
            {
                ports.config->setStepsPerDegree(RA_STEPS, 0.1F * static_cast<float>(_raStepsPerDegree));
                addMessage(outcome, "RA steps stored", kStoredDelay);
                _state = State::HighlightRaSteps;
            }
            else if (key == core::Button::Right)
            {
                outcome.result       = MenuKeyResult::NextScreen;
                outcome.shutdownGyro = _board.hasLevelSensor;
                _state               = State::HighlightRaSteps;
            }
            break;

        case State::DecStepCalibration:
            if (key == core::Button::Select)
            {
                ports.config->setStepsPerDegree(DEC_STEPS, 0.1F * static_cast<float>(_decStepsPerDegree));
                addMessage(outcome, "DEC steps stored.", kStoredDelay);
                _state = State::HighlightDecSteps;
            }
            else if (key == core::Button::Right)
            {
                outcome.result       = MenuKeyResult::NextScreen;
                outcome.shutdownGyro = _board.hasLevelSensor;
                _state               = State::HighlightDecSteps;
            }
            break;

        case State::BacklashCalibration:
            if (key == core::Button::Select)
            {
                ports.config->setBacklashCorrection(_backlashSteps);
                addMessage(outcome, "Backlash stored.", kStoredDelay);
                _state = State::HighlightBacklash;
            }
            else if (key == core::Button::Right)
            {
                outcome.result       = MenuKeyResult::NextScreen;
                outcome.shutdownGyro = _board.hasLevelSensor;
                _state               = State::HighlightBacklash;
            }
            break;

        case State::AzimuthAdjustment:
            if (key == core::Button::Select)
            {
                if (_azimuthMinutes == 0)
                {
                    _state = State::HighlightAzimuth;
                }
                else
                {
                    ports.axes->moveBy(AZIMUTH_STEPS, 1.0F * static_cast<float>(_azimuthMinutes));
                    _azimuthMinutes = 0;
                }
            }
            break;

        case State::AltitudeAdjustment:
            if (key == core::Button::Select)
            {
                if (_altitudeMinutes == 0)
                {
                    _state = State::HighlightAltitude;
                }
                else
                {
                    ports.axes->moveBy(ALTITUDE_STEPS, 1.0F * static_cast<float>(_altitudeMinutes));
                    _altitudeMinutes = 0;
                }
            }
            break;

        case State::RollOffsetCalibration:
            if (key == core::Button::Select)
            {
                if (startupWaitingForRoll)
                {
                    outcome.result        = MenuKeyResult::NextScreen;
                    outcome.shutdownGyro  = true;
                    outcome.resumeStartup = true;
                    _state                = State::HighlightPolar;
                }
                else
                {
                    _state                    = State::RollOffsetConfirm;
                    outcome.suspendMenuUpdate = true;
                    display.writeLine(0, "Set as level?");
                }
            }
            else if (key == core::Button::Left && !startupWaitingForRoll)
            {
                _state = State::HighlightRoll;
            }
            else if (key == core::Button::Right && !startupWaitingForRoll)
            {
                outcome.result       = MenuKeyResult::NextScreen;
                outcome.shutdownGyro = true;
                _state               = State::HighlightRoll;
            }
            break;

        case State::RollOffsetConfirm:
            if (key == core::Button::Select)
            {
                if (_setLevelOffset && (gyro != nullptr))
                {
                    ports.config->setRollCalibrationAngle(gyro->angles().roll);
                }
                _state                   = State::HighlightRoll;
                outcome.resumeMenuUpdate = true;
            }
            else if (key == core::Button::Left)
            {
                _setLevelOffset = !_setLevelOffset;
            }
            break;

        case State::PitchOffsetCalibration:
            if (key == core::Button::Select)
            {
                _state                    = State::PitchOffsetConfirm;
                outcome.suspendMenuUpdate = true;
                display.writeLine(0, "Set as level?");
            }
            else if (key == core::Button::Left)
            {
                _state = State::HighlightPitch;
            }
            else if (key == core::Button::Right)
            {
                outcome.result       = MenuKeyResult::NextScreen;
                outcome.shutdownGyro = true;
                _state               = State::HighlightPitch;
            }
            break;

        case State::PitchOffsetConfirm:
            if (key == core::Button::Select)
            {
                if (_setLevelOffset && (gyro != nullptr))
                {
                    ports.config->setPitchCalibrationAngle(gyro->angles().pitch);
                }
                _state                   = State::HighlightPitch;
                outcome.resumeMenuUpdate = true;
            }
            else if (key == core::Button::Left)
            {
                _setLevelOffset = !_setLevelOffset;
            }
            break;

        case State::BacklightCalibration:
            if (key == core::Button::Select)
            {
                display.setBacklight(static_cast<uint8_t>(_brightness), true);
                addMessage(outcome, "Level stored.", kStoredDelay);
                _state = State::HighlightBacklight;
            }
            else if (key == core::Button::Right)
            {
                outcome.result       = MenuKeyResult::NextScreen;
                outcome.shutdownGyro = _board.hasLevelSensor;
                _state               = State::HighlightBacklight;
            }
            break;

        case State::UtcOffsetCalibration:
            if (key == core::Button::Select)
            {
                ports.siteTime->setLocalUtcOffset(_utcOffset);
                addMessage(outcome, "UTC stored.", kStoredDelay);
                _state = State::HighlightUtcOffset;
            }
            else if (key == core::Button::Right)
            {
                outcome.result       = MenuKeyResult::NextScreen;
                outcome.shutdownGyro = _board.hasLevelSensor;
                _state               = State::HighlightUtcOffset;
            }
            break;

        case State::DecLowerLimitConfirm:
        case State::DecUpperLimitConfirm:
            {
                const bool upper = (_state == State::DecUpperLimitConfirm);

                if ((key == core::Button::Down) || (key == core::Button::Left) || (key == core::Button::Up))
                {
                    _limitIndex = static_cast<uint8_t>(core::adjustWrap(_limitIndex, 1, 0, 2));
                }
                else if (key == core::Button::Select)
                {
                    if (_limitIndex == 0)
                    {
                        ports.config->setDecLimitPosition(upper);
                        addMessage(outcome, upper ? "UprLimit stored" : "LowLimit stored", kLimitStoredDelay);
                    }
                    else if (_limitIndex == 1)
                    {
                        ports.config->clearDecLimitPosition(upper);
                        addMessage(outcome, upper ? "UprLimit cleared" : "LowLimit cleared", kLimitStoredDelay);
                    }
                    else
                    {
                        // "Cancel" explains how to move the mount instead of setting a limit here.
                        addMessage(outcome, "Use CTRL to move", kAlignmentDelay);
                        addMessage(outcome, "OAT to limit,", kAlignmentDelay);
                    }
                    outcome.resumeMenuUpdate = true;
                    _state                   = upper ? State::HighlightDecUpperLimit : State::HighlightDecLowerLimit;
                }
                else if (key == core::Button::Right)
                {
                    _state                   = upper ? State::HighlightDecUpperLimit : State::HighlightDecLowerLimit;
                    outcome.resumeMenuUpdate = true;
                }
            }
            break;

        default:
            break;
    }

    return outcome;
}

CalibrationOutcome CalibrationMenu::handleHighlightKey(const core::Button key,
                                                       UiPorts &ports,
                                                       ports::IDisplay &display,
                                                       CalibrationOutcome outcome)
{
    outcome.result = MenuKeyResult::WaitForRelease;

    if (key == core::Button::Down)
    {
        moveHighlight(1, ports, display);
        return outcome;
    }
    if (key == core::Button::Up)
    {
        moveHighlight(-1, ports, display);
        return outcome;
    }
    if (key == core::Button::Right)
    {
        outcome.result       = MenuKeyResult::NextScreen;
        outcome.shutdownGyro = _board.hasLevelSensor;
        _state               = _ring[0];
        return outcome;
    }
    if (key != core::Button::Select)
    {
        return outcome;
    }

    switch (_state)
    {
        case State::HighlightPolar:
            {
                _state = State::PolarWaitCenter;

                // Swing to Polaris' RA, and as far past it as Polaris is from the pole: that puts
                // the DEC axis along the pole-to-Polaris line so the AZ/ALT nudge can align it.
                ports.read->targetRA().set(_board.polarisRaHours, _board.polarisRaMinutes, _board.polarisRaSeconds);
                const int hemisphere    = (ports.siteTime->latitude().getTotalHours() >= 0.0F) ? 90 : -90;
                ports.read->targetDEC() = core::Declination(kPolarDecDegrees - hemisphere, kPolarDecMinutes, kPolarDecSeconds);
                ports.motion->startSlewingToTarget();
                outcome.suspendMenuUpdate = true;
            }
            break;

        case State::HighlightDrift:
            _state = State::DriftWait;
            break;

        case State::HighlightSpeed:
            _state = State::SpeedCalibration;
            break;

        case State::HighlightRaSteps:
            _state = State::RaStepCalibration;
            break;

        case State::HighlightDecSteps:
            _state = State::DecStepCalibration;
            break;

        case State::HighlightBacklash:
            _state = State::BacklashCalibration;
            break;

        case State::HighlightDecLowerLimit:
        case State::HighlightDecUpperLimit:
            _state = (_state == State::HighlightDecLowerLimit) ? State::DecLowerLimitConfirm : State::DecUpperLimitConfirm;
            outcome.suspendMenuUpdate = true;
            break;

        case State::HighlightUtcOffset:
            _state                    = State::UtcOffsetCalibration;
            outcome.suspendMenuUpdate = true;
            break;

        case State::HighlightAzimuth:
            _state = State::AzimuthAdjustment;
            break;

        case State::HighlightAltitude:
            _state = State::AltitudeAdjustment;
            break;

        case State::HighlightRoll:
            _state = State::RollOffsetCalibration;
            break;

        case State::HighlightPitch:
            _state = State::PitchOffsetCalibration;
            break;

        case State::HighlightBacklight:
            _state = State::BacklightCalibration;
            break;

        default:
            break;
    }

    return outcome;
}

void CalibrationMenu::paintIndicator(char *row, float angle)
{
    // "R: -------------": the reading sits at columns 8-10, the marker one cell per quarter degree.
    angle = core::clamp(angle, -9.9F, 9.9F);

    for (int i = 0; i < 5; i++)
    {
        row[3 + i]  = '-';
        row[11 + i] = '-';
    }

    char reading[8];
    OAT_SNPRINTF(reading, sizeof(reading), "%3.1f", static_cast<double>(fabsf(angle)));
    row[8]  = reading[0];
    row[9]  = reading[1];
    row[10] = reading[2];

    const int position = core::clamp(static_cast<int>(roundf(angle * 4.0F)), -5, 5);
    if (position != 0)
    {
        row[position + ((position < 0) ? 8 : 10)] = (position < 0) ? '>' : '<';
    }
    row[16] = '\0';
}

void CalibrationMenu::render(UiPorts &ports, ports::IGyro *gyro, ports::IDisplay &display)
{
    char row[24];

    switch (_state)
    {
        case State::HighlightPolar:
            display.writeLine(1, ">Polar alignment");
            return;
        case State::HighlightDrift:
            display.writeLine(1, ">Drift alignment");
            return;
        case State::HighlightSpeed:
            display.writeLine(1, ">Speed calibratn");
            return;
        case State::HighlightRaSteps:
            display.writeLine(1, ">RA Step Adjust");
            return;
        case State::HighlightDecSteps:
            display.writeLine(1, ">DEC Step Adjust");
            return;
        case State::HighlightBacklash:
            display.writeLine(1, ">Backlash Adjust");
            return;
        case State::HighlightDecLowerLimit:
            display.writeLine(1, ">DEC Lower Limit");
            return;
        case State::HighlightDecUpperLimit:
            display.writeLine(1, ">DEC Upper Limit");
            return;
        case State::HighlightUtcOffset:
            display.writeLine(1, ">UTC Offset");
            return;
        case State::HighlightAzimuth:
            display.writeLine(1, ">Azimuth Adjst.");
            return;
        case State::HighlightAltitude:
            display.writeLine(1, ">Altitude Adjst.");
            return;
        case State::HighlightRoll:
            display.writeLine(1, ">Roll Offset");
            return;
        case State::HighlightPitch:
            display.writeLine(1, ">Pitch Offset");
            return;
        case State::HighlightBacklight:
            display.writeLine(1, ">LCD Brightness");
            return;

        case State::PolarWaitCenter:
            if (!ports.motion->isSlewingRAorDEC())
            {
                display.writeLine(0, "Center Polaris");
                display.writeLine(1, ">Centered");
            }
            return;

        case State::SpeedCalibration:
            OAT_SNPRINTF(row, sizeof(row), "SpdFctr: %6.4f", static_cast<double>(ports.config->getSpeedCalibration()));
            display.writeLine(1, row);
            return;

        case State::DriftWait:
            OAT_SNPRINTF(row, sizeof(row), " 1m  2m  3m  5m ");
            row[_driftSubIndex * 4] = '>';
            display.writeLine(1, row);
            return;

        case State::RaStepCalibration:
            OAT_SNPRINTF(row, sizeof(row), "RA Steps: %.1f", 0.1 * static_cast<double>(_raStepsPerDegree));
            display.writeLine(1, row);
            return;

        case State::DecStepCalibration:
            OAT_SNPRINTF(row, sizeof(row), "DEC Steps: %.1f", 0.1 * static_cast<double>(_decStepsPerDegree));
            display.writeLine(1, row);
            return;

        case State::BacklashCalibration:
            OAT_SNPRINTF(row, sizeof(row), "Backlash: %d", _backlashSteps);
            display.writeLine(1, row);
            return;

        case State::DecLowerLimitConfirm:
        case State::DecUpperLimitConfirm:
            display.writeLine(0, (_state == State::DecLowerLimitConfirm) ? "DEC Lower Limit" : "DEC Upper Limit");
            OAT_SNPRINTF(row, sizeof(row), "%s", kLimitChoices);
            row[_limitIndex * 5] = '>';
            display.writeLine(1, row);
            return;

        case State::UtcOffsetCalibration:
            OAT_SNPRINTF(row, sizeof(row), "UTC : %d", _utcOffset);
            display.writeLine(1, row);
            return;

        case State::AzimuthAdjustment:
            OAT_SNPRINTF(row, sizeof(row), "Az: %d arcmins", _azimuthMinutes);
            display.writeLine(1, row);
            return;

        case State::AltitudeAdjustment:
            OAT_SNPRINTF(row, sizeof(row), "Alt: %d arcmins", _altitudeMinutes);
            display.writeLine(1, row);
            return;

        case State::RollOffsetCalibration:
            row[0] = 'R';
            row[1] = ':';
            row[2] = ' ';
            paintIndicator(row, (gyro != nullptr) ? (gyro->angles().roll - ports.config->getRollCalibrationAngle()) : 0.0F);
            display.writeLine(1, row);
            return;

        case State::PitchOffsetCalibration:
            row[0] = 'P';
            row[1] = ':';
            row[2] = ' ';
            paintIndicator(row, (gyro != nullptr) ? (gyro->angles().pitch - ports.config->getPitchCalibrationAngle()) : 0.0F);
            display.writeLine(1, row);
            return;

        case State::RollOffsetConfirm:
        case State::PitchOffsetConfirm:
            {
                char choices[16];
                OAT_SNPRINTF(choices, sizeof(choices), "%s", kLevelChoices);
                choices[_setLevelOffset ? 0 : 5] = '>';
                choices[_setLevelOffset ? 4 : 8] = '<';
                display.writeLine(1, choices);
            }
            return;

        case State::BacklightCalibration:
            OAT_SNPRINTF(row, sizeof(row), "Brightness: %d", _brightness);
            display.writeLine(1, row);
            return;

        default:
            return;
    }
}

}  // namespace app
