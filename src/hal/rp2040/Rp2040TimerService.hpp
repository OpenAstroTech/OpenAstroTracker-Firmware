#pragma once

#include <stdint.h>

#include "hal/ITimerService.hpp"

// `repeating_timer_t` and `alarm_id_t` are SDK value types, not Arduino ones.
#include <pico/time.h>

namespace hal
{

/// `ITimerService` over the RP2040 SDK's repeating timer and alarm APIs.
///
/// One instance drives either a repeating timer (`add_repeating_timer_us`) or a
/// single alarm (`add_alarm_in_us`). Callbacks run in the SDK's timer interrupt
/// context, so they must not block or allocate.
class Rp2040TimerService final : public ITimerService
{
  public:
    Rp2040TimerService();

    ~Rp2040TimerService() override;

    bool startPeriodic(uint32_t periodMicroseconds, Callback callback, void *context) override;

    bool startOneShot(uint32_t delayMicroseconds, Callback callback, void *context) override;

    void stop() override;

    bool isRunning() const override;

  private:
    static bool onPeriodic(repeating_timer_t *timer);

    static int64_t onOneShot(alarm_id_t id, void *context);

    repeating_timer_t _repeating;
    alarm_id_t _alarm;
    Callback _callback;
    void *_context;
    volatile bool _running;
};

}  // namespace hal
