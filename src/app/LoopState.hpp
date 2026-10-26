#pragma once

/// The flags the menu loop owns.
///
/// They are defined once, in `app/BoardWiring.cpp`, and read from the loop and from the serial path.
/// They are here rather than in a screen because they are not a screen's business: the guided
/// startup and the serial host both take the display away from the menu, and the menu has to know.
extern bool inStartup;  ///< The guided startup wizard is driving the display.

/// False while a screen has taken the display over with its own prompt, so the loop stops repainting
/// the menu underneath it.
extern bool okToUpdateMenu;

/// Set when the SELECT key means "leave serial mode" on its next release.
extern bool quitSerialOnNextButtonRelease;
