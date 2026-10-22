#include "InfoDisplayPanel.hpp"

#include "InfoDisplayRender.hpp"
#include "Mount.hpp"

namespace adapters
{
InfoDisplayPanel::InfoDisplayPanel(Mount &mount, InfoDisplayRender &panel, ports::IClock &clock, ports::IMountEvents &events)
    : _mount(mount), _panel(panel), _clock(clock), _events(events)
{
}

void InfoDisplayPanel::begin()
{
    _events.subscribe(this);
}

void InfoDisplayPanel::onMountEvent(const ::core::MountEvent &event)
{
    switch (event.type)
    {
        case ::core::MountEventType::GuidingStarted:
            _guiding = true;
            _dirty   = true;  // one frame at the start of the pulse, as the guide path used to draw
            break;

        case ::core::MountEventType::GuidingStopped:
            _guiding = false;
            _dirty   = true;
            break;

        default:
            // Everything else is a state change the panel shows: it gets one frame at the next
            // opportunity rather than waiting out a slow slew bucket.
            _dirty = true;
            break;
    }
}

void InfoDisplayPanel::render()
{
    _panel.renderScreen(_mount);
    _lastRender = _clock.now();
    _dirty      = false;
}

int InfoDisplayPanel::refreshRateHz()
{
#if defined(ARDUINO_ARCH_RP2040)
    return anyAxisSlewing() ? 15 : 30;
#else
    return anyAxisSlewing() ? 5 : 30;
#endif
}

void InfoDisplayPanel::tick()
{
    const uint32_t now = _clock.now();

    if (_guiding)
    {
        // Nothing periodic while a guide pulse runs; the pulse-start frame is the exception, and
        // it is rate-limited like any other transition.
        if (_dirty && ((now - _lastRender) >= kMinimumFrameIntervalMs))
        {
            render();
        }
        return;
    }

    if (bothAxesSlewing())
    {
        // Deliberately no frame: two axes slewing is when the serial ring buffer is thinnest.
        return;
    }

    if ((now - _lastRender) >= static_cast<uint32_t>(1000 / refreshRateHz()))
    {
        render();
        return;
    }

    if (_dirty && ((now - _lastRender) >= kMinimumFrameIntervalMs))
    {
        render();
    }
}

bool InfoDisplayPanel::anyAxisSlewing()
{
    return _mount.isAxisRunning(RA_STEPS) || _mount.isAxisRunning(DEC_STEPS);
}

bool InfoDisplayPanel::bothAxesSlewing()
{
    return _mount.isAxisRunning(RA_STEPS) && _mount.isAxisRunning(DEC_STEPS);
}

}  // namespace adapters
