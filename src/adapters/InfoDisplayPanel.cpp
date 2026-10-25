#include "InfoDisplayPanel.hpp"

namespace adapters
{
InfoDisplayPanel::InfoDisplayPanel(ports::IInfoDisplaySource &source,
                                   ports::IInfoDisplay &panel,
                                   ports::IClock &clock,
                                   ports::IMountEvents &events)
    : _source(source), _panel(panel), _clock(clock), _events(events)
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
    _source.fillInfoDisplayState(_state);
    _panel.render(_state);
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
    const ports::InfoDisplayActivity activity = _source.activity();
    return activity.raRunning || activity.decRunning;
}

bool InfoDisplayPanel::bothAxesSlewing()
{
    const ports::InfoDisplayActivity activity = _source.activity();
    return activity.raRunning && activity.decRunning;
}

}  // namespace adapters
