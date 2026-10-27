#pragma once

#include "../../Configuration.hpp"
#include "core/ConsoleLog.hpp"
#include "core/InfoDisplayLayout.hpp"
#include "hal/WarningGuard.hpp"
#include "ports/IClock.hpp"
#include "ports/IInfoDisplay.hpp"

PUSH_NO_WARNINGS
#include <Arduino.h>
POP_NO_WARNINGS

namespace hal
{
class IOledPanel;
}

namespace adapters
{

/// `ports::IInfoDisplay` for the 128x64 status panel.
///
/// The panel used to be a renderer subclass that the mount owned; it is now an adapter the
/// composition root builds, drawing through `hal::IOledPanel` from a `ports::InfoDisplayState` POD
/// it is handed — neither the mount nor the renderer reads the other.
///
/// The boot console is part of this panel: the firmware prints its bring-up lines here, so the
/// adapter keeps the 16-line ring and the six-line window.
class Ssd1306InfoDisplay final : public ports::IInfoDisplay
{
  public:
    static constexpr int kMaxConsoleLines     = core::ConsoleLog::kCapacity;
    static constexpr int kConsoleVisibleLines = core::ConsoleLog::kVisible;

    Ssd1306InfoDisplay(hal::IOledPanel &panel, ports::IClock &clock);

    bool begin() override;

    void render(const ports::InfoDisplayState &state) override;

    /// Switches between the boot console and the mount status frame.
    void setConsoleMode(bool active);

    /// Appends a console line and repaints. Returns the line index, clamped to the ring.
    int addConsoleText(const String &text);

    /// Rewrites one console line in place and repaints.
    void updateConsoleText(int line, const String &text);

  private:
    void renderConsole();
    void drawHeader();
    void drawConsole();
    void drawIndicators(const ports::InfoDisplayState &state);
    void drawTime(const char *label, const types::DayTime &time);
    void drawProgressBar(int percRA, int percDEC);
    void drawStepperState(const char *name, bool active, int xoff, int width, int textOffX = 0);
    void drawStepperStates(const ports::InfoDisplayState &state);
    void drawCommunicationStatus(const ports::InfoDisplayState &state);
    void drawCoordinate(int x, int y, const char *coord);
    void drawCoordinates(const ports::InfoDisplayState &state);
    void drawMountPosition(const ports::InfoDisplayState &state);
    void drawStatus(const ports::InfoDisplayState &state);
    int xRAPixel(float ra) const;
    int yDECPixel(float dec) const;

    /// The panel's own scale geometry. The DEC span is the one thing that differs per product:
    /// the mount and explorer firmware map the whole circle, the tracker half of it.
    core::InfoDisplayScale scale() const;

    hal::IOledPanel &_panel;
    ports::IClock &_clock;
    core::ConsoleLog _console;
    bool _consoleMode;
    char _commLetter;
    long _lastNumCmds;

    static constexpr int kStatusBounceHeight = 63 - 11;
};

}  // namespace adapters
