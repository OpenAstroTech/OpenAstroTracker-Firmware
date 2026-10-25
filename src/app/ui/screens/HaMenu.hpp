#pragma once

#include <stdint.h>

#include "app/UiPorts.hpp"
#include "app/ui/MenuScreen.hpp"
#include "ports/IDisplay.hpp"
#include "ports/IGps.hpp"

namespace app
{

/// What the loop has to do after the HA screen consumed a key.
struct HaMenuOutcome {
    MenuKeyResult result = MenuKeyResult::Ignored;

    /// The operator stored the hour angle: the loop flashes "Stored." and lets the mount keep
    /// tracking, which is what the legacy screens did inline.
    bool haStored = false;

    /// The guided startup was waiting for this confirmation: hand control back to the wizard.
    bool resumeStartup = false;
};

/// The hour-angle screen, in both of its shapes.
///
/// A board with a receiver walks through "Found n sats" → "Sync" → "Set" → manual entry, and the
/// receiver is the thing that actually sets the site and the clock. A board without one is only the
/// manual editor, with a different key feel: the arrows auto-repeat while held, which is why the
/// legacy no-GPS screen called `delay(200)` from inside its key handler.
///
/// The receiver is passed in rather than owned so this screen is host-testable and so nothing has to
/// agree about static initialisation order.
class HaMenu
{
  public:
    enum class State : uint8_t
    {
        StartingGps,
        ShowingSync,
        ShowingSet,
        EnterManually,
    };

    /// Puts the screen back where a board with a receiver starts, or straight into manual entry.
    void enter(bool hasGps);

    MenuKeyResult handleKey(const MenuKeyInput &input, UiPorts &ports, ports::IGps *gps, bool startupWaitingForHa, HaMenuOutcome &outcome);

    void render(ports::IMountReadModel &read, ports::IDisplay &display) const;

    State state() const
    {
        return _state;
    }

  private:
    /// Applies a fix to the site and the clock; true when the fix was fresh enough to use.
    bool applyFix(ports::IGps &gps, UiPorts &ports);

    /// The manual entry step: one hour or one minute, depending on the selected field.
    void adjustHourAngle(UiPorts &ports, int direction) const;

    State _state = State::EnterManually;

    /// Which half of "hh mm" the cursor is on.
    uint8_t _field = 0;

    /// The satellite count of the last poll, and the spinner beside it.
    uint8_t _satellites              = 0;
    uint8_t _spinnerPosition         = 0;
    unsigned long _lastSpinnerUpdate = 0;
};

}  // namespace app
