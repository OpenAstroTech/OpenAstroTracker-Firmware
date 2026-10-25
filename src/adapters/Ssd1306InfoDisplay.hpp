#pragma once

#include "../../Configuration.hpp"
#include "hal/WarningGuard.hpp"
#include "ports/IInfoDisplay.hpp"
#include "ports/IClock.hpp"

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
    static constexpr int kMaxConsoleLines     = 16;
    static constexpr int kConsoleVisibleLines = 6;

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
    void drawTime(const char *label, const core::DayTime &time);
    void drawProgressBar(int percRA, int percDEC);
    void drawStepperState(const char *name, bool active, int xoff, int width, int textOffX = 0);
    void drawStepperStates(const ports::InfoDisplayState &state);
    void drawCommunicationStatus(const ports::InfoDisplayState &state);
    void drawCoordinate(int x, int y, const char *coord);
    void drawCoordinates(const ports::InfoDisplayState &state);
    void drawMountPosition(const ports::InfoDisplayState &state);
    void drawStatus(const ports::InfoDisplayState &state);
    int xRAPixel(float ra);
    int yDECPixel(float dec);

    hal::IOledPanel &_panel;
    ports::IClock &_clock;
    bool _consoleMode;
    String _textList[kMaxConsoleLines];
    int _curLine;
    char _commLetter;
    long _lastNumCmds;

#if defined(OAM) || defined(OAE)
    static constexpr float kBottomDec = -180.0f;
    static constexpr float kRangeDec  = 360.0f;
#else
    static constexpr float kBottomDec = -90.0f;
    static constexpr float kRangeDec  = 270.0f;
#endif
    static constexpr float kLeftRa  = -6.0f;
    static constexpr float kRightRa = 6.0f;

    static constexpr int kLeftEdgeMount = 69;  // x position where the RA scale starts
    static constexpr int kRaSize        = 41;  // width of the RA scale
    static constexpr int kRaScalePos    = 51;  // y position of the RA scale dotted line

    static constexpr int kTopEdgeMount = 14;
    static constexpr int kDecSize      = 43;
    static constexpr int kDecScalePos  = 115;

    static constexpr int kStatusBounceHeight = 63 - 11;
};

}  // namespace adapters
