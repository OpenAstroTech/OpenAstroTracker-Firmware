#include "../Configuration.hpp"
#include "ports/ILogger.hpp"
#include "adapters/EepromSettingsFormat.hpp"
#include "HomingRunner.hpp"
#include "EndSwitchMonitor.hpp"
#include "MountExecutor.hpp"
#include "../core/CalendarMath.hpp"
#include "../core/CoordinateFormatter.hpp"
#include "../core/CoordinateText.hpp"
#include "../core/ManualSlew.hpp"
#include "../core/RelativeMove.hpp"

// The menu screens pass the historical `B0xxx` constants through `ports::IMountReadModel`; core
// owns the format vocabulary, so the two must not drift.
static_assert(static_cast<uint8_t>(core::CoordinateFormat::LcdMenu) == LCDMENU_STRING, "format value drift: LcdMenu");
static_assert(static_cast<uint8_t>(core::CoordinateFormat::Meade) == MEADE_STRING, "format value drift: Meade");
static_assert(static_cast<uint8_t>(core::CoordinateFormat::Print) == PRINT_STRING, "format value drift: Print");
static_assert(static_cast<uint8_t>(core::CoordinateFormat::Lcd) == LCD_STRING, "format value drift: Lcd");
static_assert(static_cast<uint8_t>(core::CoordinateFormat::Compact) == COMPACT_STRING, "format value drift: Compact");
static_assert(core::kCoordinateFormatMask == FORMAT_STRING_MASK, "format mask drift");
static_assert(core::kTargetFlag == TARGET_STRING, "target flag drift");

static_assert(static_cast<uint8_t>(core::SlewDirection::North) == NORTH, "direction value drift: North");
static_assert(static_cast<uint8_t>(core::SlewDirection::East) == EAST, "direction value drift: East");
static_assert(static_cast<uint8_t>(core::SlewDirection::South) == SOUTH, "direction value drift: South");
static_assert(static_cast<uint8_t>(core::SlewDirection::West) == WEST, "direction value drift: West");
static_assert(static_cast<uint8_t>(core::SlewDirection::Tracking) == TRACKING, "direction value drift: Tracking");
#include "../core/CoordinateMath.hpp"
#include "../core/ActivityState.hpp"
#include "../core/meade/MountStatusReport.hpp"
#include "../core/CoordinateRead.hpp"
#include "../core/DecLimitClamp.hpp"
#include "../core/FirmwareVersion.hpp"
#include "../core/PersistentConfig.hpp"
#include "../core/SiderealClock.hpp"
#include "../core/RaTrackingLimit.hpp"
#include "../core/SlewProgressReport.hpp"
#include "../core/SlewStart.hpp"
#include "../core/SyncAdjustment.hpp"
#include "../libs/MappedDict/MappedDict.hpp"

#include "UiPorts.hpp"
#include "../adapters/GpioEndSwitch.hpp"
#include "../adapters/HallHomingSensor.hpp"
#include "../adapters/NullMotorDriver.hpp"
#include "../adapters/Tmc2209Driver.hpp"

#include "../hal/CriticalSection.hpp"
#include "../hal/arduino/ArduinoGpioPin.hpp"

// slewStatus()
#define SLEWING_DEC      B00000010
#define SLEWING_RA       B00000001
#define SLEWING_BOTH     B00000011
#define SLEWING_TRACKING B00001000
#define NOT_SLEWING      B00000000
#define SLEW_MASK_ANY    B1111

#define UART_CONNECTION_TEST_RETRIES 5

const float siderealDegreesInHour = 14.95904348958;

namespace app
{

/////////////////////////////////
//
// CTOR
//
/////////////////////////////////
namespace
{
/// The readout cannot evaluate the RA limit itself — reaching it stops the tracker — so the
/// executor answers through this thunk. Same shape as `SiteTimePort::LatitudeObserver`.
float hoursToLimitThunk(void *context)
{
    return static_cast<MountExecutor *>(context)->checkRALimit();
}

/// A settings write has to recompute the tracking rate while the mount is tracking, which is a
/// motion consequence the settings class must not decide for itself.
bool isTrackingThunk(void *context)
{
    return static_cast<MountExecutor *>(context)->isSlewingTRK();
}
}  // namespace

MountExecutor::MountExecutor(const MountDeps &deps, const core::MountConfig &config, Coordinator &coordinator)
    : _deps(deps), _config(config), _coordinator(coordinator), _readout(_axes,
                                                                        coordinator,
                                                                        _settings,
                                                                        config,
                                                                        _siteTime,
                                                                        _targetRA,
                                                                        _targetDEC,
                                                                        _zeroPosDEC,
                                                                        *deps.clock,
                                                                        *deps.logger,
                                                                        _commandReceived,
                                                                        _bootComplete,
                                                                        {this, hoursToLimitThunk}),
      _persisted(_settings, *deps.settings, _axes, coordinator, *deps.logger, {this, isTrackingThunk})
{
    _commandReceived = 0;

    initializeVariables();
}

void MountExecutor::initializeVariables()
{
    // We are now defaulting to northern hemisphere at 45deg. Switching is now supported
    // at runtime when the Latitude is received via Meade command.
    inNorthernHemisphere = NORTHERN_HEMISPHERE == 1;

    // u-steps per degree when slewing, plus the motion tuning defaults
    _settings.reset(_config.defaultRaStepsPerDegree, _config.defaultDecStepsPerDegree);

    _coordinator.state().reset(0);
    _coordinator.slew().noteRunning(false);

    // Created by the configure* methods below; null until then. The drivers go back to the null
    // driver, which `reset()` puts in place.
    _axes.reset();
    _coordinator.tracking().detach();
    _coordinator.focus().detach();

    _siteTime.setLatitude(core::Latitude(inNorthernHemisphere ? 45.0f : -45.0f));
    _siteTime.setLongitude(core::Longitude(100.0));
    _zeroPosDEC = 0.0f;

    _coordinator.slew().clearTotalMove();
    _coordinator.slew().clearBacklashCorrection();
    _coordinator.slew().end();
    _coordinator.parking().clear();
    applyParkingStatus();
    _persisted.reset();

    _siteTime.setLocalUtcOffset(0);
    _siteTime.setLocalStartDate({2021, 1, 1});
    _siteTime.setLocalStartTime(DayTime(0), static_cast<unsigned long>(-1));
    _lastTRKCheck = 0;
}

/// Records a fact for the tick boundary. The next `loop()` delivers it, so a subscriber
/// is never called from inside the code that just changed the mount's state.
void MountExecutor::publishEvent(core::MountEventType type, bool succeeded)
{
    core::MountEvent event;
    event.type      = type;
    event.succeeded = succeeded;
    _coordinator.events().publish(event);
}

void MountExecutor::setStatus(int state)
{
    // The bitfield is a read-modify-write; hal's guard is what makes it safe against the step
    // generator. See hal/CriticalSection.hpp.
    const hal::CriticalSection lock;
    _coordinator.state().reset((uint16_t) state);
}

void MountExecutor::setStatusFlag(int flag)
{
    // The bitfield is a read-modify-write; hal's guard is what makes it safe against the step
    // generator. See hal/CriticalSection.hpp.
    const hal::CriticalSection lock;
    _coordinator.state().set((uint16_t) flag);
}

void MountExecutor::clearStatusFlag(int flag)
{
    // The bitfield is a read-modify-write; hal's guard is what makes it safe against the step
    // generator. See hal/CriticalSection.hpp.
    const hal::CriticalSection lock;
    _coordinator.state().clear((uint16_t) flag);
}

bool MountExecutor::isSlewingRAorDEC() const
{
    if (isParking())
        return true;
    return (slewStatus() & (SLEWING_DEC | SLEWING_RA)) != 0;
}

void MountExecutor::applyParkingStatus()
{
    clearStatusFlag(STATUS_PARKING | STATUS_PARKING_POS);
    const uint16_t bit = core::ParkingController::statusBitFor(_coordinator.parking().phase());
    if (bit != 0)
    {
        setStatusFlag(bit);
    }
}

core::MountActivityFacts MountExecutor::activityFacts() const
{
    return _coordinator.activityFacts();
}

/////////////////////////////////
void MountExecutor::bootComplete()
{
    _bootComplete = true;
}

/////////////////////////////////
float MountExecutor::checkRALimit()
{
    // The arithmetic and the folding live in core::RaTrackingLimit; the side effect — stopping the
    // tracker — stays here, because it owns the axes.
    core::RaLimitFacts facts;
    facts.homeRaHours        = DayTime(_siteTime.raZeroPoint()).getTotalHours() + getTrackedHours();
    facts.currentRaHours     = currentRA().getTotalHours();
    facts.decAxisDegrees     = (_axes.dec()->currentPosition() / _settings.stepsPerDegree(DEC_STEPS)) + _zeroPosDEC;
    facts.northernHemisphere = inNorthernHemisphere;
    facts.limitHours         = RA_TRACKING_LIMIT;

    const core::RaLimitResult result = core::RaTrackingLimit::evaluate(facts);

    OAT_LOG(_deps.logger,
            DEBUG_MOUNT_VERBOSE,
            "[MOUNT]: checkRALimit: homeRA: %f (ZeroPos: %f + TrkHrs: %f), remaining: %f",
            facts.homeRaHours,
            DayTime(_siteTime.raZeroPoint()).getTotalHours(),
            getTrackedHours(),
            result.remainingHours);

    if (result.limitReached)
    {
        OAT_LOG(_deps.logger,
                DEBUG_MOUNT,
                "[MOUNT]: checkRALimit: Tracking limit reached. TrkHrs:%f, HomeRA:%f, remaining:%f",
                getTrackedHours(),
                facts.homeRaHours,
                result.remainingHours);
        stopSlewing(TRACKING);
    }
    _lastTRKCheck = _deps.clock->now();

    return result.remainingHours;
}

/////////////////////////////////
core::DayTime MountExecutor::currentRA() const
{
    return _readout.currentRA();
}

core::Declination MountExecutor::currentDEC() const
{
    return _readout.currentDEC();
}

String MountExecutor::RAString(byte type, byte active)
{
    return _readout.RAString(type, active);
}

String MountExecutor::DECString(byte type, byte active)
{
    return _readout.DECString(type, active);
}

const char *MountExecutor::raText(uint8_t type, uint8_t active)
{
    return _readout.raText(type, active);
}

const char *MountExecutor::decText(uint8_t type, uint8_t active)
{
    return _readout.decText(type, active);
}

byte MountExecutor::slewStatus() const
{
    return _readout.slewStatus();
}

bool MountExecutor::isAxisRunning(StepperAxis axis)
{
    return _readout.isAxisRunning(axis);
}

bool MountExecutor::isSlewingTRK() const
{
    return _readout.isSlewingTRK();
}

bool MountExecutor::isGuiding() const
{
    return _readout.isGuiding();
}

bool MountExecutor::isFindingHome() const
{
    return _readout.isFindingHome();
}

bool MountExecutor::getStepperProgress(int &raPercentage, int &decPercentage)
{
    return _readout.getStepperProgress(raPercentage, decPercentage);
}

String MountExecutor::getStatusStateString()
{
    return _readout.getStatusStateString();
}

const char *MountExecutor::getStatusString()
{
    return _readout.getStatusString();
}

const char *MountExecutor::getStepperInfo()
{
    return _readout.getStepperInfo();
}

const char *MountExecutor::getMountHardwareInfo()
{
    return _readout.getMountHardwareInfo();
}

const char *MountExecutor::getAutoHomingStates() const
{
    return _readout.getAutoHomingStates();
}

float MountExecutor::getStepsPerDegree(StepperAxis which)
{
    return _readout.getStepsPerDegree(which);
}

bool MountExecutor::isRunningAZ() const
{
    return _readout.isRunningAZ();
}

bool MountExecutor::isRunningALT() const
{
    return _readout.isRunningALT();
}

long MountExecutor::getCurrentStepperPosition(StepperAxis which)
{
    return _readout.getCurrentStepperPosition(which);
}

long MountExecutor::getCurrentStepperPosition(int direction)
{
    return _readout.getCurrentStepperPosition(direction);
}

core::DayTime MountExecutor::calculateLst()
{
    return _readout.calculateLst();
}

core::DayTime &MountExecutor::targetRA()
{
    return _readout.targetRA();
}

core::Declination &MountExecutor::targetDEC()
{
    return _readout.targetDEC();
}

float MountExecutor::getTrackedHours() const
{
    return _readout.getTrackedHours();
}

bool MountExecutor::isParking() const
{
    return _readout.isParking();
}

void MountExecutor::calculateRAandDECSteppers(long &targetRASteps, long &targetDECSteps, long pSolutions[6]) const
{
    _readout.calculateRAandDECSteppers(targetRASteps, targetDECSteps, pSolutions);
}

/////////////////////////////////
//
// The configuration forwards
//
// `app::MountSettings` owns the write side of the configuration; the executor keeps the
// `ports::IMountConfiguration` entry points so the menu screens and the command layer still talk
// to one object, exactly as they do for the read model.
//
/////////////////////////////////
float MountExecutor::getSpeedCalibration()
{
    return _persisted.getSpeedCalibration();
}

void MountExecutor::setSpeedCalibration(float value, bool saveToStorage)
{
    _persisted.setSpeedCalibration(value, saveToStorage);
}

#if USE_GYRO_LEVEL == 1
float MountExecutor::getPitchCalibrationAngle()
{
    return _persisted.getPitchCalibrationAngle();
}

void MountExecutor::setPitchCalibrationAngle(float angle)
{
    _persisted.setPitchCalibrationAngle(angle);
}

float MountExecutor::getRollCalibrationAngle()
{
    return _persisted.getRollCalibrationAngle();
}

void MountExecutor::setRollCalibrationAngle(float angle)
{
    _persisted.setRollCalibrationAngle(angle);
}
#endif

void MountExecutor::setStepsPerDegree(StepperAxis which, float steps)
{
    _persisted.setStepsPerDegree(which, steps);
}

int MountExecutor::getBacklashCorrection()
{
    return _persisted.getBacklashCorrection();
}

void MountExecutor::setBacklashCorrection(int steps)
{
    _persisted.setBacklashCorrection(steps);
}

void MountExecutor::setSlewRate(int rate)
{
    _persisted.setSlewRate(rate);
}

int MountExecutor::getSlewRate()
{
    return _persisted.getSlewRate();
}

void MountExecutor::setHomingOffset(StepperAxis axis, long offset)
{
    _persisted.setHomingOffset(axis, offset);
}

long MountExecutor::getHomingOffset(StepperAxis axis)
{
    return _persisted.getHomingOffset(axis);
}

void MountExecutor::setDecLimitPosition(bool upper, float limitAngle)
{
    _persisted.setDecLimitPosition(upper, limitAngle);
}

void MountExecutor::clearDecLimitPosition(bool upper)
{
    _persisted.clearDecLimitPosition(upper);
}

void MountExecutor::getDecLimitPositions(float &lowerLimit, float &upperLimit)
{
    _persisted.getDecLimitPositions(lowerLimit, upperLimit);
}

}  // namespace app