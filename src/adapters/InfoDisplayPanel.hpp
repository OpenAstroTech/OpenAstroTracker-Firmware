#pragma once

#include "core/MountEvent.hpp"
#include "ports/IClock.hpp"
#include "ports/IInfoDisplay.hpp"
#include "ports/IMountEvents.hpp"

class InfoDisplayRender;

namespace adapters
{
/// Drives the optional 128x64 status panel.
///
/// The panel used to be driven *by* the mount: `Mount::loop()` ended with
/// `infoDisplay->renderScreen(this)`, and the guide-pulse path called it explicitly because it
/// returns early. That made the mount aware of a peripheral that only exists on some boards, and
/// it put the refresh-rate policy — which exists to protect the serial ring buffer during a slew —
/// inside the motion code. The frame is now a POD the composition root fills, so the renderer
/// never reads the mount either.
///
/// Ownership is inverted here: the composition root creates the panel, the main loop ticks it, and
/// the mount's events tell it when something worth showing has happened. `Mount` no longer knows
/// the display exists.
///
/// The refresh policy is preserved exactly, including the platform split, because it was a
/// measured workaround and not a preference:
/// * 30 Hz when idle.
/// * One axis slewing: 5 Hz (RP2040: 15 Hz, it has the headroom).
/// * Both axes slewing: no periodic frames at all — an I2C frame is long enough to starve the
///   serial ring buffer, which is what produced ASCOM driver timeouts.
/// * During a guide pulse: one frame when the pulse starts (the `GuidingStarted` event), nothing
///   until it stops. The old code got the same shape by returning early from the loop.
class InfoDisplayPanel final : private ::core::IEventSink
{
  public:
    /// Highest frame rate a transition may ask for, whatever the slew bucket says.
    static constexpr uint32_t kMinimumFrameIntervalMs = 33;  // ~30 Hz

    InfoDisplayPanel(ports::IInfoDisplaySource &source, InfoDisplayRender &panel, ports::IClock &clock, ports::IMountEvents &events);

    /// Subscribes to the mount's events.
    void begin();

    /// Renders when the policy allows. Called once per main-loop iteration, after `mount.loop()`
    /// so the frame shows the state that tick produced.
    void tick();

  private:
    void onMountEvent(const ::core::MountEvent &event) override;
    void render();
    int refreshRateHz();
    bool anyAxisSlewing();
    bool bothAxesSlewing();

    ports::IInfoDisplaySource &_source;
    ports::InfoDisplayState _state{};
    InfoDisplayRender &_panel;
    ports::IClock &_clock;
    ports::IMountEvents &_events;
    uint32_t _lastRender = 0;
    bool _dirty          = false;
    bool _guiding        = false;
};

}  // namespace adapters
