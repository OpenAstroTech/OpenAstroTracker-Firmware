#pragma once

#include <stdint.h>

#include "app/UiPorts.hpp"
#include "app/ui/MenuScreen.hpp"
#include "ports/IDisplay.hpp"
#include "ports/IGyro.hpp"

namespace app
{

/// What the loop has to do after the calibration screen consumed a key.
struct CalibrationOutcome {
    MenuKeyResult result = MenuKeyResult::Ignored;

    /// The screen has taken over the display with a prompt: the loop must stop repainting the menu.
    bool suspendMenuUpdate = false;

    /// The prompt is done and the menu may repaint again.
    bool resumeMenuUpdate = false;

    /// Everything the screen touched is closed: the loop should shut the level sensor down.
    bool shutdownGyro = false;

    /// The roll step of the guided startup is confirmed: hand control back to the wizard.
    bool resumeStartup = false;

    /// One-off confirmations ("Speed Stored.", "UprLimit stored") the loop paints before waiting.
    /// Two of them can follow each other, which is how the "use CTRL to move" hint reads.
    const char *messages[2]   = {nullptr, nullptr};
    unsigned messageDelays[2] = {0, 0};
    uint8_t messageCount      = 0;
};

/// The calibration menu: polar alignment, the two-step calibrations, the DEC soft limits, the UTC
/// offset, the AZ/ALT nudge, the level offsets and the backlight.
///
/// The legacy screen was one flat `switch` over sixteen highlight states and sixteen action states,
/// with four `#if` families deciding which entries existed. Here the entries that exist are decided
/// once, in the constructor, and the highlight ring is that list — so moving through the menu stays
/// a wrap over the entries this board actually has, without a preprocessor branch in the way.
class CalibrationMenu
{
  public:
    /// The board facts that change the menu. Everything else the screen reads through its ports.
    struct Board {
        bool hasDriftAlignment = false;
        bool hasAzimuth        = false;
        bool hasAltitude       = false;
        bool hasLevelSensor    = false;

        /// The speeds the polar-alignment step drives the AZ/ALT axes at, in degrees per second.
        float azimuthSpeed  = 0.0F;
        float altitudeSpeed = 0.0F;

        /// Polaris, which is configurable, so the polar-alignment step is told where it is.
        uint8_t polarisRaHours   = 3;
        uint8_t polarisRaMinutes = 0;
        uint8_t polarisRaSeconds = 8;
    };

    enum class State : uint8_t
    {
        // Highlights: the menu rows.
        HighlightPolar,
        HighlightDrift,
        HighlightSpeed,
        HighlightRaSteps,
        HighlightDecSteps,
        HighlightBacklash,
        HighlightDecLowerLimit,
        HighlightDecUpperLimit,
        HighlightUtcOffset,
        HighlightAzimuth,
        HighlightAltitude,
        HighlightRoll,
        HighlightPitch,
        HighlightBacklight,

        // Actions.
        PolarWaitCenter,
        PolarWaitHome,
        DriftWait,
        DriftRunning,
        SpeedCalibration,
        RaStepCalibration,
        DecStepCalibration,
        BacklashCalibration,
        DecLowerLimitConfirm,
        DecUpperLimitConfirm,
        UtcOffsetCalibration,
        AzimuthAdjustment,
        AltitudeAdjustment,
        RollOffsetCalibration,
        RollOffsetConfirm,
        PitchOffsetCalibration,
        PitchOffsetConfirm,
        BacklightCalibration,
    };

    explicit CalibrationMenu(const Board &board);

    /// Back to the first entry, as selecting CAL always did.
    void enter();

    CalibrationOutcome handleKey(
        const MenuKeyInput &input, UiPorts &ports, ports::IGyro *gyro, ports::IDisplay &display, bool startupWaitingForRoll);

    void render(UiPorts &ports, ports::IGyro *gyro, ports::IDisplay &display);

    State state() const
    {
        return _state;
    }

  private:
    bool isHighlight(State state) const;

    /// The shared behaviour of the menu rows: UP/DOWN walk the ring, SELECT opens the entry, RIGHT
    /// leaves the screen.
    CalibrationOutcome handleHighlightKey(core::Button key, UiPorts &ports, ports::IDisplay &display, CalibrationOutcome outcome);

    /// Moves the highlight by `direction` through the entries this board has, refreshing the value
    /// the newly selected entry edits.
    void moveHighlight(int direction, UiPorts &ports, ports::IDisplay &display);

    /// The held-arrow editor every numeric entry shares. Returns false when it consumed the arrow.
    bool updateProgressive(long &value, int minimumDelay, const MenuKeyInput &input, UiPorts &ports);

    /// The speed editor, which steps a ten-thousandth at a time and shrinks its own repeat delay.
    bool updateSpeedCalibration(const MenuKeyInput &input, UiPorts &ports);

    /// The polar-alignment step's manual AZ/ALT drive, which acts on the held key.
    void updatePolarDrive(core::Button held, UiPorts &ports);

    void paintIndicator(char *row, float angle);

    Board _board;
    State _state = State::HighlightPolar;

    /// The entries this board has, in menu order.
    State _ring[14]   = {};
    uint8_t _ringSize = 0;

    /// The repeat delay, which shrinks while an arrow is held.
    int _repeatDelay = 150;

    float _speedCalibration = 0.0F;
    long _raStepsPerDegree  = 0;
    long _decStepsPerDegree = 0;
    int _backlashSteps      = 0;
    int _utcOffset          = 0;
    int _azimuthMinutes     = 0;
    int _altitudeMinutes    = 0;
    int _brightness         = 0;

    uint8_t _driftSubIndex = 1;
    uint8_t _limitIndex    = 0;  ///< Set/Clear/Cancel answer while confirming a DEC limit
    bool _setLevelOffset   = true;
    bool _gyroStarted      = false;
};

}  // namespace app
