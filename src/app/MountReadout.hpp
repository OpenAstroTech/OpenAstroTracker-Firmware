#pragma once

PUSH_NO_WARNINGS
#include <Arduino.h>
POP_NO_WARNINGS

#include "../../Configuration.hpp"
#include "ports/PortVocabulary.hpp"
#include "../core/MountConfig.hpp"
#include "../core/MountCoordinator.hpp"
#include "../core/MountRuntimeSettings.hpp"
#include "../core/SiteTime.hpp"
#include "../types/DayTime.hpp"
#include "../types/Declination.hpp"
#include "adapters/DayTime.hpp"
#include "adapters/Declination.hpp"
#include "../ports/IClock.hpp"
#include "../ports/ILogger.hpp"
#include "../ports/IMountReadModel.hpp"
#include "../ports/IInfoDisplay.hpp"
#include "AxisBank.hpp"

namespace app
{
class HallSensorHoming;
class EndSwitch;

/// Everything the mount says about itself: the coordinate readouts, the status strings and the
/// motion predicates.
///
/// This is the *formatting* half of `ports::IMountReadModel`, not the whole port. The port is
/// cross-cutting — `checkRALimit` belongs to the homing cluster, `calculateStepperPositions` to
/// the axes cluster — so a class implementing all of it would have to reach every other class.
/// What does move cleanly is the half that only *reads*: it takes references to the state it
/// reports on and never mutates any of it, which is why it could be lifted out of the executor
/// without re-sequencing anything.
///
/// Two members are scratch, and they are here because the strings this class returns point into
/// them: `scratchBuffer` backs the two `*Text` port methods, and
/// `_readoutScratch`/`_infoStatusScratch` back the status strings. The rule the ports state —
/// "valid until the next call to the same method" — is unchanged by the move.
class MountReadout final : public ports::IMountReadModel, public ports::IInfoDisplaySource
{
  public:
    /// `checkRALimit()` is the one read this class cannot make for itself: reaching the limit *stops
    /// the tracker*, so the decision belongs to the motion executor and the answer is asked for
    /// through this callback. A function pointer and a context rather than a virtual, for the same
    /// reason `SiteTimeService::LatitudeObserver` is one: a vtable costs SRAM on AVR.
    struct RaLimitQuery {
        void *context;
        float (*hoursToLimit)(void *context);

        bool valid() const
        {
            return hoursToLimit != nullptr;
        }
    };

    /// The mount's coordinate state is shared with the executor, which owns it and mutates it
    /// when it moves an axis or writes a target. Everything is held by reference for that reason:
    /// this class reports, it does not own. The two coordinates are mutable references because
    /// `targetRA()`/`targetDEC()` hand the caller a writable target, as they always have.
    MountReadout(AxisBank &axes,
                 core::MountCoordinator &coordinator,
                 core::MountRuntimeSettings &settings,
                 const core::MountConfig &config,
                 core::SiteTime &siteTime,
                 DayTime &targetRa,
                 Declination &targetDec,
                 float &zeroPosDec,
                 ports::IClock &clock,
                 ports::ILogger &logger,
                 long &commandsReceived,
                 bool &bootComplete,
                 RaLimitQuery raLimit);

    types::DayTime currentRA() const;
    types::Declination currentDEC() const;

    const char *raText(uint8_t type, uint8_t active = 0);
    const char *decText(uint8_t type, uint8_t active = 0);

    /// The motion predicates. Every one of them is a question about `core::MountState` plus the
    /// axes, so they belong with the rest of the reads.
    byte slewStatus() const;
    bool isAxisRunning(StepperAxis axis);
    bool isSlewingTRK() const;
    bool isGuiding() const;
    bool isFindingHome() const;

    bool getStepperProgress(int &raPercentage, int &decPercentage);
    ports::InfoDisplayActivity activity() override;

    float getStepsPerDegree(StepperAxis which);
    bool isRunningAZ() const;
    bool isRunningALT() const;
    long getCurrentStepperPosition(StepperAxis which);
    long getCurrentStepperPosition(int direction);
    types::DayTime calculateLst();
    types::DayTime calculateHa();

    /// The targets are the executor's storage, handed out writable because `:Sr#`/`:Sd#` write them
    /// in place — the port has always exposed them that way.
    types::DayTime &targetRA();
    types::Declination &targetDEC();
    types::DayTime hourAngle();

    void calculateStepperPositions(float raCoord, float decCoord, long &raPos, long &decPos);
    void calculateRAandDECSteppers(long &targetRASteps, long &targetDECSteps, long pSolutions[6] = nullptr) const;

    bool isSlewingIdle() const;
    bool isParking() const;
    float plannedRaSteps() const;
    float plannedDecSteps() const;
    bool isBootComplete();
    bool isNorthernHemisphere() const override;

    /// The port asks this, and the answer comes from the executor through `_raLimit`: reaching the
    /// limit stops the tracker, so the decision is not this class's to make.
    float checkRALimit() override;

    /// The command counter lives in the executor (the command layer is its only writer), so this
    /// reads and bumps it through a reference.
    void commandReceived() override;

    float getTrackedHours() const;
    const DayTime HA() const;

    /// `ports::IInfoDisplaySource`: the one frame the status panel draws.
    void fillInfoDisplayState(ports::InfoDisplayState &state) override;

    /// The status strings. The `String` ones are internal to the class; the `const char *` ones are
    /// the ports, and their storage rule is the one the port documents.
    String getStatusStateString();
    const char *getStatusString();
    const char *getStepperInfo();
    const char *getMountHardwareInfo();
    const char *getAutoHomingStates() const;
    long getNumCommandsReceived() override;

    /// The status line the panel shows, pointing into this class's buffer so it survives the call.
    const char *panelStatusText();

  private:
    String homingStateText(bool present, const HallSensorHoming *sensor) const;

    AxisBank &_axes;
    core::MountCoordinator &_coordinator;
    core::MountRuntimeSettings &_settings;
    const core::MountConfig &_config;
    core::SiteTime &_siteTime;
    DayTime &_targetRa;
    Declination &_targetDec;
    float &_zeroPosDec;
    ports::IClock &_clock;
    ports::ILogger &_logger;
    long &_commandsReceived;
    bool &_bootComplete;
    RaLimitQuery _raLimit;

    mutable String _readoutScratch;
    String _infoStatusScratch;
    char scratchBuffer[24];
};

}  // namespace app
