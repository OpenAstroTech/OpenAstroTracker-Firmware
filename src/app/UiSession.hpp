#pragma once

namespace app
{
/// The session state the menu loop and the screens share.
///
/// One field today: whether the serial host has taken the display away from the menu. It lives here
/// rather than with the screens because it is not a screen's business — the loop and the serial
/// path both set it, and every screen only asks.
struct UiSession {
    /// True when the serial port is in control, so the menu stops painting over the host's view.
    bool serialControl = false;
};

}  // namespace app

/// The one instance, defined in `BoardWiring.cpp` next to the object graph, like `ui` and `mount`.
extern app::UiSession uiSession;
