#pragma once

/// The flags the menu loop owns, in one object.
///
/// They were three free `bool`s with the same prefixless names in every scope that read them; they
/// are always read and written together, so they travel together.
struct LoopState {
    /// The guided startup wizard is driving the display.
    bool inStartup;

    /// False while a screen has taken the display over with its own prompt, so the loop stops
    /// repainting the menu underneath it.
    bool okToUpdateMenu;

    /// Set when the SELECT key means "leave serial mode" on its next release.
    bool quitSerialOnNextButtonRelease;
};

/// Defined once, in `app/BoardWiring.cpp`, and read from the loop and from the serial path.
extern LoopState loopState;
