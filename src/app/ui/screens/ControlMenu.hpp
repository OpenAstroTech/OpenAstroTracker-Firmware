#pragma once

#include <stdint.h>

#include "app/ui/MenuScreen.hpp"
#include "app/ui/SlewStatusView.hpp"
#include "ports/IDisplay.hpp"
#include "ports/IMountMotion.hpp"
#include "ports/IMountReadModel.hpp"

namespace app
{

/// What the loop has to do after the control screen consumed a key.
///
/// The screen has no business reaching for the loop's globals, so it reports the side effects it
/// wants and the loop applies them. Every field defaults to "nothing to do".
struct ControlMenuOutcome {
    MenuKeyResult result = MenuKeyResult::Ignored;

    /// The operator confirmed "set home": the loop should keep the menu off the display until the
    /// confirmation is resolved.
    bool suspendMenuUpdate = false;

    /// The confirmation is done and the menu may repaint again.
    bool resumeMenuUpdate = false;

    /// The operator confirmed home while the guided startup was waiting for the pole: the loop
    /// should hand control back to the startup wizard instead of moving to the next menu item.
    bool resumeStartup = false;

    /// SELECT toggled serial mode; `serialActive` is the value the loop should apply.
    bool serialChanged = false;
    bool serialActive  = false;
};

/// The CTRL menu: manual slewing, the serial-mode toggle, and "set home".
///
/// Manual slewing is the interesting half: it acts on the key that is *held*, not on the edge, so the
/// screen needs both. The legacy code carried a ten-pass "confirm the key" countdown, but its counter
/// could never arm itself — it was only ever started from a state in which it was already running —
/// so the mount was in fact commanded on the first pass of every new key. That is the behaviour kept
/// here, without the unreachable counter.
class ControlMenu
{
  public:
    void enter();

    /// The guided startup's "skip the manual control prompt": jump straight into manual slewing.
    void startManual();

    ControlMenuOutcome handleKey(const MenuKeyInput &input, ports::IMountMotion &motion, bool serialActive, bool startupAwaitingPole);

    void render(ports::IMountReadModel &read, bool serialActive, unsigned long now, ports::IDisplay &display);

  private:
    enum class State : uint8_t
    {
        HighlightManual,
        HighlightSerial,
        Manual,
        ConfirmHome,
    };

    /// The held-key slewing state machine: when the held key changes, stop both axes and start the
    /// one the key names.
    void updateManualSlew(types::Button held, ports::IMountMotion &motion);

    static int directionFor(types::Button key);

    State _state = State::HighlightManual;

    /// The key that is currently commanding the mount.
    types::Button _commandedKey = types::Button::None;

    bool _setZeroPoint = true;

    /// The slew-progress paint for the manual state, which is the same view the loop uses.
    SlewStatusView _slewStatus;
};

}  // namespace app
