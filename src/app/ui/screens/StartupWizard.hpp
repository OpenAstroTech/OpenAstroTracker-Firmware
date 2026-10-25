#pragma once

#include <stdint.h>

#include "app/UiPorts.hpp"
#include "app/ui/MenuScreen.hpp"
#include "ports/IDisplay.hpp"

namespace app
{

/// What the wizard wants the loop to do after it consumed a key.
///
/// The wizard knows the sequence but not the menu: which screen to switch to, and which loop flags
/// to set, belong to the loop that owns them. It also carries the one-line prompt the legacy code
/// painted before the switch ("Level front", "Set current HA", …).
enum class StartupAction : uint8_t
{
    None,
    /// "Mount is in the home position? No" — hand over to the control menu in manual mode.
    HandOverToControlMenu,
    /// Level the front of the mount, in the calibration menu.
    HandOverToCalibration,
    /// Set the current hour angle, in the HA menu.
    HandOverToHaMenu,
    /// Everything is set: go to the RA screen, and start tracking if the board does that on boot.
    Completed,
};

struct StartupOutcome {
    MenuKeyResult result = MenuKeyResult::Ignored;
    StartupAction action = StartupAction::None;
    const char *caption  = nullptr;
};

/// The guided startup: the wizard that walks a fresh mount through home, level, hour angle and pole.
///
/// It is a plain state machine plus the two facts that change its route (whether the board has a
/// level sensor and whether it has a GPS), so it can be driven on the host. The steps that happen
/// *inside* another screen come back through `rollConfirmed()`, `haConfirmed()` and
/// `poleConfirmed()`, which is what those screens report instead of assigning a global.
class StartupWizard
{
  public:
    enum class Step : uint8_t
    {
        InHomePosition,
        SetRoll,
        WaitForRollCompletion,
        RollConfirmed,
        SetHaTime,
        WaitForHaCompletion,
        HaConfirmed,
        WaitForPoleCompletion,
        PoleConfirmed,
        Completed,
    };

    enum class HomeAnswer : uint8_t
    {
        Yes,
        No,
        Cancel,
    };

    StartupWizard(bool hasLevelSensor, bool hasGps);

    /// Puts the wizard back on its first step, as power-on does.
    void enter();

    StartupOutcome handleKey(const MenuKeyInput &input, UiPorts &ports);

    /// The calibration screen confirmed the roll: the wizard moves on to the hour angle.
    void rollConfirmed();

    /// The hour angle screen confirmed: the wizard sets home and finishes on its next pass.
    void haConfirmed();

    void poleConfirmed();

    void render(ports::IDisplay &display) const;

    Step step() const
    {
        return _step;
    }

    HomeAnswer homeAnswer() const
    {
        return _homeAnswer;
    }

  private:
    /// The one transition the "cancel" answer performs, shared with the yes/no branches.
    StartupOutcome complete();

    /// What confirming the hour angle does to the mount: the current position becomes home, and the
    /// hour angle the operator just set is written back so the two agree.
    void applyHourAngleConfirmation(UiPorts &ports);

    bool _hasLevelSensor;
    bool _hasGps;

    Step _step             = Step::InHomePosition;
    HomeAnswer _homeAnswer = HomeAnswer::No;
};

}  // namespace app
