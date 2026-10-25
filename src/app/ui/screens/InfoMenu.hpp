#pragma once

#include <stdint.h>

#include "app/ui/MenuScreen.hpp"
#include "ports/IDisplay.hpp"
#include "ports/IMountConfiguration.hpp"
#include "ports/IMountReadModel.hpp"
#include "ports/IMountSiteTime.hpp"

namespace app
{

/// The facts the status screen needs that come from the board, not from the mount.
///
/// A value the loop fills in, so the screen stays Arduino-free: free memory, the level sensor and
/// the version string are platform questions, and each board answers them differently (or not at
/// all).
struct InfoMenuEnvironment {
    const char *version = "";
    /// Bytes left on the heap, or a negative number when the platform cannot report it.
    int freeMemoryBytes = -1;
    /// The level sensor's reading in degrees Celsius, or `NAN` when the board has no sensor.
    float gyroTemperatureC = 0.0F;
    bool hasGyro           = false;
};

/// The status screen: sixteen scrolling readouts, three of which have sub-pages.
///
/// Entries 0-2 carry two or three sub-pages (stepper position, target and current coordinate), so
/// SELECT and LEFT walk the sub-page while UP and DOWN walk the list. The sub-page is deliberately
/// *not* reset when the list moves: that is what the legacy screen did, and the value is only
/// meaningful for the entries that read it.
class InfoMenu
{
  public:
    /// Puts the screen back on its first entry and forces the next paint through.
    void enter();

    MenuKeyResult handleKey(const MenuKeyInput &input);

    /// Draws the selected entry, at most once per `DISPLAY_UPDATE_TIME`.
    ///
    /// Not `const`: the gyro entry is skipped in place when the board has no sensor, exactly as the
    /// legacy screen did, so one paint can move the selection.
    void render(ports::IMountReadModel &read,
                ports::IMountConfiguration &config,
                ports::IMountSiteTime &siteTime,
                const InfoMenuEnvironment &environment,
                unsigned long now,
                ports::IDisplay &display);

    static constexpr uint8_t kEntryCount = 15;  // entries 0..15

  private:
    uint8_t _index            = 0;
    uint8_t _subIndex         = 0;
    unsigned long _lastUpdate = 0;
};

}  // namespace app
