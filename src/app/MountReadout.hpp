#pragma once

#include "../inc/Globals.hpp"
#include "../../Configuration.hpp"
#include "../Types.hpp"
#include "../core/MountConfig.hpp"
#include "../core/MountCoordinator.hpp"
#include "../core/MountRuntimeSettings.hpp"
#include "../core/SiteTime.hpp"
#include "../core/types/DayTime.hpp"
#include "../core/types/Declination.hpp"
#include "../ports/IInfoDisplay.hpp"
#include "AxisBank.hpp"

class HallSensorHoming;

namespace app
{

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
/// them: `scratchBuffer` backs `RAString`/`DECString` and the two `*Text` port methods, and
/// `_readoutScratch`/`_infoStatusScratch` back the status strings. The rule the ports state —
/// "valid until the next call to the same method" — is unchanged by the move.
class MountReadout
{
  public:
    /// The mount's coordinate state is shared with the executor, which owns it and mutates it
    /// when it moves an axis or writes a target. Everything is held by reference for that reason:
    /// this class reports, it does not own. The two coordinates are mutable references because
    /// `targetRA()`/`targetDEC()` hand the caller a writable target, as they always have.
    MountReadout(AxisBank &axes,
                 core::MountCoordinator &coordinator,
                 core::MountRuntimeSettings &settings,
                 const core::MountConfig &config,
                 core::SiteTime &siteTime,
                 core::DayTime &targetRa,
                 core::Declination &targetDec,
                 float &zeroPosDec);

    core::DayTime currentRA() const;
    core::Declination currentDEC() const;

    String RAString(byte type, byte active = 0);
    String DECString(byte type, byte active = 0);
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
    ports::InfoDisplayActivity activity();

    /// The status strings. The `String` ones are internal to the class; the `const char *` ones are
    /// the ports, and their storage rule is the one the port documents.
    String getStatusStateString();
    const char *getStatusString();
    const char *getStepperInfo();
    const char *getMountHardwareInfo();
    const char *getAutoHomingStates() const;

    /// The status line the panel shows, pointing into this class's buffer so it survives the call.
    const char *panelStatusText();

  private:
    String homingStateText(bool present, const HallSensorHoming *sensor) const;

    AxisBank &_axes;
    core::MountCoordinator &_coordinator;
    core::MountRuntimeSettings &_settings;
    const core::MountConfig &_config;
    core::SiteTime &_siteTime;
    core::DayTime &_targetRa;
    core::Declination &_targetDec;
    float &_zeroPosDec;

    mutable String _readoutScratch;
    String _infoStatusScratch;
    char scratchBuffer[24];
};

}  // namespace app
