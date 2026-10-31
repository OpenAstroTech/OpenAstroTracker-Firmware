#pragma once

namespace app
{
/// The menu loop: the one place that knows which screen is active.
///
/// It owns nothing but the dispatch — `app::MenuKeyInput` in from the button port, a screen's
/// `Outcome` out, and the two rows painted. Every screen is a class in `app/ui/`, so this is the
/// last of the `#if`-heavy UI code that *belongs* to the board: the panel family, the feature
/// selection and the serial path are chosen in `MenuHost.cpp`, at the top of `app/`, which is where
/// Phase 8 wanted them (PLAN.md 8.8).
///
/// This header declares the loop and nothing else. `MenuHost.cpp` used to be a header that
/// *defined* five globals, five helpers and `loop()` itself, which meant it could only ever be
/// included by one translation unit — true by convention, with no diagnostic if that changed.
/// The Arduino entry points now live in `main.cpp`, where `CLAUDE.md` says they are.
void menuLoop();

}  // namespace app
