#include "app/ui/screens/InfoMenu.hpp"

#include <math.h>
#include <string.h>

#include "core/MathUtil.hpp"
#include "core/types/DayTime.hpp"
#include "ports/ILogger.hpp"

namespace
{

/// Same cadence the legacy screen used (`DISPLAY_UPDATE_TIME`): the values change slowly and the
/// panel is slow to redraw. Spelled out rather than included, so `app/ui` stays free of the board
/// configuration on the host.
constexpr unsigned long kUpdateInterval = 200;

/// Wide enough for "Up: 999d 23:59:59" and "TRK Spd: 1.002738"; the display truncates at its width.
constexpr uint8_t kRowCapacity = 24;

/// Entries 0 and 1 (the two coordinate readouts) have a third sub-page; everything else has two.
uint8_t subPageCount(uint8_t index)
{
    return (index < 2) ? 3 : 2;
}

}  // namespace

namespace app
{

void InfoMenu::enter()
{
    _index      = 0;
    _subIndex   = 0;
    _lastUpdate = 0;  // forces the first paint through
}

MenuKeyResult InfoMenu::handleKey(const MenuKeyInput &input)
{
    switch (input.edge)
    {
        case core::Button::Down:
            _index = static_cast<uint8_t>(core::adjustWrap(_index, 1, 0, kEntryCount));
            break;

        case core::Button::Up:
            _index = static_cast<uint8_t>(core::adjustWrap(_index, -1, 0, kEntryCount));
            break;

        case core::Button::Select:
        case core::Button::Left:
            _subIndex = static_cast<uint8_t>(core::adjustWrap(_subIndex, 1, 0, subPageCount(_index) - 1));
            break;

        case core::Button::Right:
            return MenuKeyResult::NextScreen;

        default:
            return MenuKeyResult::Ignored;
    }

    _lastUpdate = 0;  // a key press always repaints immediately
    return MenuKeyResult::WaitForRelease;
}

void InfoMenu::render(ports::IMountReadModel &read,
                      ports::IMountConfiguration &config,
                      ports::IMountSiteTime &siteTime,
                      const InfoMenuEnvironment &environment,
                      unsigned long now,
                      ports::IDisplay &display)
{
    if ((now - _lastUpdate) <= kUpdateInterval)
    {
        return;
    }

    char row[kRowCapacity];

    switch (_index)
    {
        case 0:
            if (_subIndex == 0)
            {
                OAT_SNPRINTF(row, sizeof(row), "RA Stpr: %ld", read.getCurrentStepperPosition(static_cast<int>(WEST)));
            }
            else if (_subIndex == 1)
            {
                OAT_SNPRINTF(row, sizeof(row), "RTrg: %s", read.raText(LCD_STRING | TARGET_STRING));
            }
            else
            {
                OAT_SNPRINTF(row, sizeof(row), "RCur: %s", read.raText(LCD_STRING | CURRENT_STRING));
            }
            break;

        case 1:
            if (_subIndex == 0)
            {
                OAT_SNPRINTF(row, sizeof(row), "DEC Stpr:%ld", read.getCurrentStepperPosition(static_cast<int>(NORTH)));
            }
            else if (_subIndex == 1)
            {
                OAT_SNPRINTF(row, sizeof(row), "DTrg: %s", read.decText(LCD_STRING | TARGET_STRING));
            }
            else
            {
                OAT_SNPRINTF(row, sizeof(row), "DCur: %s", read.decText(LCD_STRING | CURRENT_STRING));
            }
            break;

        case 2:
            if (_subIndex == 0)
            {
                OAT_SNPRINTF(row, sizeof(row), "TRK Stpr:%ld", read.getCurrentStepperPosition(static_cast<int>(TRACKING)));
            }
            else
            {
                OAT_SNPRINTF(row, sizeof(row), "TRK Spd:%8.6f", static_cast<double>(config.getSpeed(TRACKING)));
            }
            break;

        case 3:
            {
                const float latitude  = siteTime.latitude().getTotalHours();
                const float longitude = siteTime.longitude().getTotalHours();
                OAT_SNPRINTF(row,
                             sizeof(row),
                             "Loc %.1f%c %.1f%c",
                             static_cast<double>(fabsf(latitude)),
                             (latitude < 0.0F) ? 'S' : 'N',
                             static_cast<double>(fabsf(longitude)),
                             (longitude < 0.0F) ? 'W' : 'E');
            }
            break;

        case 4:
            if (environment.hasGyro)
            {
                const int celsius    = static_cast<int>(roundf(environment.gyroTemperatureC));
                const int fahrenheit = static_cast<int>(roundf(32.0F + (9.0F * environment.gyroTemperatureC / 5.0F)));
                OAT_SNPRINTF(row, sizeof(row), "Temp: %d@C %d@F", celsius, fahrenheit);
            }
            else
            {
                // No sensor on this board: step over the entry instead of painting an empty row,
                // which is what the legacy screen did.
                _index = static_cast<uint8_t>(core::adjustWrap(_index, 1, 0, kEntryCount));
                return;
            }
            break;

        case 5:
            {
                float lowerLimit = 0.0F;
                float upperLimit = 0.0F;
                config.getDecLimitPositions(lowerLimit, upperLimit);
                OAT_SNPRINTF(row, sizeof(row), "DEC Lo: %.1f", static_cast<double>(lowerLimit));
            }
            break;

        case 6:
            {
                float lowerLimit = 0.0F;
                float upperLimit = 0.0F;
                config.getDecLimitPositions(lowerLimit, upperLimit);
                OAT_SNPRINTF(row, sizeof(row), "DEC Up: %.1f", static_cast<double>(upperLimit));
            }
            break;

        case 7:
            OAT_SNPRINTF(row, sizeof(row), "MemAvail: %d", environment.freeMemoryBytes);
            break;

        case 8:
            {
                const unsigned long msPerDay = 60UL * 60UL * 24UL * 1000UL;
                const int days               = static_cast<int>(now / msPerDay);
                const unsigned long today    = now - (static_cast<unsigned long>(days) * msPerDay);
                const core::DayTime elapsed(static_cast<double>(today) / (1000.0 * 3600.0));
                OAT_SNPRINTF(
                    row, sizeof(row), "Up: %dd %02d:%02d:%02d", days, elapsed.getHours(), elapsed.getMinutes(), elapsed.getSeconds());
            }
            break;

        case 9:
            {
                const LocalDate date = siteTime.getLocalDate();
                OAT_SNPRINTF(row, sizeof(row), "Date: %04d-%02d-%02d", date.year, date.month, date.day);
            }
            break;

        case 10:
            {
                const core::DayTime utc = siteTime.getUtcTime();
                OAT_SNPRINTF(row, sizeof(row), "UTC: %02d:%02d:%02d", utc.getHours(), utc.getMinutes(), utc.getSeconds());
            }
            break;

        case 11:
            {
                const core::DayTime local = siteTime.getLocalTime();
                OAT_SNPRINTF(row, sizeof(row), "Time: %02d:%02d:%02d", local.getHours(), local.getMinutes(), local.getSeconds());
            }
            break;

        case 12:
            OAT_SNPRINTF(row, sizeof(row), "Timezone: %d", siteTime.getLocalUtcOffset());
            break;

        case 13:
            {
                const core::DayTime lst = read.calculateLst();
                OAT_SNPRINTF(row, sizeof(row), "LST: %02d:%02d:%02d", lst.getHours(), lst.getMinutes(), lst.getSeconds());
            }
            break;

        case 14:
            {
                const core::DayTime ha = read.calculateHa();
                OAT_SNPRINTF(row, sizeof(row), "HA: %02d:%02d:%02d", ha.getHours(), ha.getMinutes(), ha.getSeconds());
            }
            break;

        default:
            OAT_SNPRINTF(row, sizeof(row), "Firmw.: %s", environment.version);
            break;
    }

    display.writeLine(1, row);
    _lastUpdate = now;
}

}  // namespace app
