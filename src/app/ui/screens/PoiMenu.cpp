#include "app/ui/screens/PoiMenu.hpp"

#include <stdlib.h>

#include "app/ui/PointOfInterestTable.hpp"
#include "types/MathUtil.hpp"
#include "types/Declination.hpp"

namespace
{
/// The axis declination for a celestial coordinate, the way this screen has always computed it:
/// 90° minus the celestial arc-seconds in the north, 90° plus them in the south.
///
/// The southern branch keeps the positive sign, which is *not* what
/// `types::Declination::celestialToAxisSeconds()` produces — that one negates for the southern
/// hemisphere, matching how a wire declination is parsed into an axis coordinate. The two only
/// differ south of the equator, the legacy screen used this one, and changing it here would change
/// where a southern mount slews. Recorded as a legacy quirk rather than fixed.
types::Declination axisDeclinationFor(const app::PointOfInterest &point, bool northernHemisphere)
{
    long celestialSeconds = ((60L * abs(point.decDegrees)) + point.decMinutes) * 60L + point.decSeconds;
    if (point.decDegrees < 0)
    {
        celestialSeconds = -celestialSeconds;
    }

    const long quarterTurn = 90L * 60L * 60L;
    return types::Declination::fromTotalSeconds(northernHemisphere ? (quarterTurn - celestialSeconds) : (quarterTurn + celestialSeconds));
}

}  // namespace

namespace app
{
MenuKeyResult PoiMenu::handleKey(const MenuKeyInput &input,
                                 ports::IMountReadModel &read,
                                 ports::IMountMotion &motion,
                                 bool northernHemisphere)
{
    switch (input.edge)
    {
        case types::Button::Select:
            {
                const PointOfInterest &point = kPointsOfInterest[_selected];

                motion.stopSlewing(ALL_DIRECTIONS);
                if (_selected == kHomePointOfInterest)
                {
                    motion.startSlewingToHome();
                }
                else if (_selected == kParkPointOfInterest)
                {
                    motion.park();
                }
                else if (_selected == kUnparkPointOfInterest)
                {
                    motion.startSlewing(TRACKING);
                }
                else
                {
                    read.targetRA().set(point.raHours, point.raMinutes, point.raSeconds);
                    read.targetDEC() = axisDeclinationFor(point, northernHemisphere);
                    motion.startSlewingToTarget();
                }
            }
            return MenuKeyResult::WaitForRelease;

        case types::Button::Left:
        case types::Button::Down:
            _selected = static_cast<uint16_t>(types::adjustWrap(_selected, 1, 0, kParkPointOfInterest));
            return MenuKeyResult::WaitForRelease;

        case types::Button::Up:
            _selected = static_cast<uint16_t>(types::adjustWrap(_selected, -1, 0, kParkPointOfInterest));
            return MenuKeyResult::WaitForRelease;

        case types::Button::Right:
            return MenuKeyResult::NextScreen;

        default:
            return MenuKeyResult::Ignored;
    }
}

void PoiMenu::render(ports::IMountReadModel &read, ports::IDisplay &display) const
{
    if (!read.isSlewingIdle())
    {
        return;
    }

    display.writeLine(1, kPointsOfInterest[_selected].display);
}

}  // namespace app
