#include "app/ui/screens/HaMenu.hpp"

#include <stdio.h>

#include "core/MathUtil.hpp"
#include "ports/ILogger.hpp"

namespace
{

/// `DISPLAY_UPDATE_TIME`'s 200 ms repeats, spelled out so `app/ui` needs no board configuration.
constexpr unsigned long kRepeatDelayMs = 200;

/// A fix older than this is not applied: the site would be set from a stale position.
constexpr uint32_t kMaximumFixAgeMs = 30000UL;

/// The spinner beside "Found n sats", one character per poll period.
constexpr const char *kSpinner = "*+* ";

}  // namespace

namespace app
{

void HaMenu::enter(bool hasGps)
{
    _state           = hasGps ? State::StartingGps : State::EnterManually;
    _field           = 0;
    _satellites      = 0;
    _spinnerPosition = 0;
}

bool HaMenu::applyFix(ports::IGps &gps, UiPorts &ports)
{
    if (!gps.hasLocation())
    {
        _satellites = gps.fix().satellites;
        return false;
    }

    const ports::GpsFix fix = gps.fix();
    _satellites             = fix.satellites;

    if (!fix.locationValid || (fix.locationAgeMs >= kMaximumFixAgeMs) || (fix.longitude == 0.0))
    {
        return false;
    }

    core::DayTime utcNow(fix.hour, fix.minute, fix.second);
    utcNow.addHours(ports.siteTime->getLocalUtcOffset());
    ports.siteTime->setLocalStartTime(utcNow);
    ports.siteTime->setLocalStartDate(fix.year, fix.month, fix.day);
    ports.siteTime->setLatitude(core::Latitude(static_cast<float>(fix.latitude)));
    ports.siteTime->setLongitude(core::Longitude(static_cast<float>(fix.longitude)));
    return true;
}

void HaMenu::adjustHourAngle(UiPorts &ports, int direction) const
{
    core::DayTime ha(ports.read->hourAngle());
    if (_field == 0)
    {
        ha.addHours(direction);
    }
    else
    {
        ha.addMinutes(direction);
    }
    ports.siteTime->setHA(ha);
}

MenuKeyResult HaMenu::handleKey(
    const MenuKeyInput &input, UiPorts &ports, ports::IGps *gps, bool startupWaitingForHa, HaMenuOutcome &outcome)
{
    if (gps == nullptr)
    {
        // The plain editor: the arrows repeat while held, then a key change ends the repeat.
        if ((input.held == core::Button::Up) || (input.held == core::Button::Down))
        {
            adjustHourAngle(ports, (input.held == core::Button::Up) ? 1 : -1);
            ports.delay(static_cast<int>(kRepeatDelayMs));
            return MenuKeyResult::Ignored;
        }

        switch (input.edge)
        {
            case core::Button::Left:
                _field = static_cast<uint8_t>(core::adjustWrap(_field, 1, 0, 1));
                return MenuKeyResult::WaitForRelease;

            case core::Button::Select:
                ports.settings->storeHATime(ports.read->hourAngle());
                outcome.haStored = true;
                if (startupWaitingForHa)
                {
                    outcome.resumeStartup = true;
                }
                return MenuKeyResult::WaitForRelease;

            case core::Button::Right:
                return startupWaitingForHa ? MenuKeyResult::WaitForRelease : MenuKeyResult::NextScreen;

            default:
                return MenuKeyResult::Ignored;
        }
    }

    if (_state == State::StartingGps)
    {
        gps->poll();
        if (applyFix(*gps, ports))
        {
            gps->begin();  // the receiver is released once it has been read
            _state = State::ShowingSync;
            if (startupWaitingForHa)
            {
                outcome.resumeStartup = true;
            }
            return MenuKeyResult::WaitForRelease;
        }

        // The spinner moves on a period, not on every byte the receiver sends.
        const unsigned long now = ports.clock->now();
        if ((now - _lastSpinnerUpdate) > kRepeatDelayMs)
        {
            _spinnerPosition   = static_cast<uint8_t>((_spinnerPosition + 1) % 4);
            _lastSpinnerUpdate = now;
        }
    }

    if (input.edge == core::Button::None)
    {
        return MenuKeyResult::Ignored;
    }

    switch (_state)
    {
        case State::StartingGps:
            if (input.edge == core::Button::Select)
            {
                gps->begin();  // start over
            }
            else if ((input.edge == core::Button::Up) || (input.edge == core::Button::Down))
            {
                _state = State::ShowingSet;
            }
            break;

        case State::ShowingSync:
            if (input.edge == core::Button::Select)
            {
                _state = State::StartingGps;
                gps->begin();
            }
            else if ((input.edge == core::Button::Up) || (input.edge == core::Button::Down))
            {
                _state = State::ShowingSet;
            }
            break;

        case State::ShowingSet:
            if (input.edge == core::Button::Select)
            {
                _state = State::EnterManually;
            }
            else if ((input.edge == core::Button::Up) || (input.edge == core::Button::Down))
            {
                _state = State::ShowingSync;
            }
            break;

        case State::EnterManually:
            if (input.edge == core::Button::Select)
            {
                ports.settings->storeHATime(ports.read->hourAngle());
                outcome.haStored = true;
                _state           = State::ShowingSet;
                if (startupWaitingForHa)
                {
                    outcome.resumeStartup = true;
                }
            }
            else if (input.edge == core::Button::Up)
            {
                adjustHourAngle(ports, 1);
            }
            else if (input.edge == core::Button::Down)
            {
                adjustHourAngle(ports, -1);
            }
            else if (input.edge == core::Button::Left)
            {
                _field = static_cast<uint8_t>(core::adjustWrap(_field, 1, 0, 1));
            }
            break;

        default:
            break;
    }

    if (input.edge == core::Button::Right)
    {
        // The legacy screen treats RIGHT differently per state: it stops listening when the
        // receiver is still running, and otherwise leaves.
        if (_state == State::StartingGps)
        {
            _state = State::ShowingSync;
            return MenuKeyResult::WaitForRelease;
        }
        if (startupWaitingForHa)
        {
            outcome.resumeStartup = true;
            return MenuKeyResult::WaitForRelease;
        }
        return MenuKeyResult::NextScreen;
    }

    return MenuKeyResult::WaitForRelease;
}

void HaMenu::render(ports::IMountReadModel &read, ports::IDisplay &display) const
{
    const core::DayTime hourAngle = read.hourAngle();
    char row[20];

    switch (_state)
    {
        case State::StartingGps:
            OAT_SNPRINTF(row, sizeof(row), "  Found %u sats", static_cast<unsigned>(_satellites));
            row[0] = kSpinner[_spinnerPosition];
            break;

        case State::ShowingSync:
            OAT_SNPRINTF(row, sizeof(row), "%02dh %02dm >Sync", hourAngle.getHours(), hourAngle.getMinutes());
            break;

        case State::ShowingSet:
            OAT_SNPRINTF(row, sizeof(row), "%02dh %02dm >Set", hourAngle.getHours(), hourAngle.getMinutes());
            break;

        case State::EnterManually:
        default:
            OAT_SNPRINTF(row, sizeof(row), " %02dh %02dm", hourAngle.getHours(), hourAngle.getMinutes());
            row[_field * 4] = '>';
            break;
    }

    display.writeLine(1, row);
}

}  // namespace app
