#pragma once

#include <stdint.h>

#include "hal/ITimerService.hpp"

namespace hal
{

/// `ITimerService` over the ESP-IDF `esp_timer` API.
///
/// The Arduino-ESP32 core already depends on `esp_timer`, so this needs no FreeRTOS
/// object of its own. Callbacks are dispatched from the `esp_timer` task, **not** from
/// an ISR, which makes this the cheapest timer backend in the firmware — but it also
/// means a callback may take longer than the step ISRs it replaces could, so it must
/// still not block.
class Esp32TimerService final : public ITimerService
{
  public:
    Esp32TimerService();

    ~Esp32TimerService() override;

    bool startPeriodic(uint32_t periodMicroseconds, Callback callback, void *context) override;

    bool startOneShot(uint32_t delayMicroseconds, Callback callback, void *context) override;

    void stop() override;

    bool isRunning() const override;

  private:
    static void onTimer(void *argument);

    // Opaque `esp_timer_handle_t`, kept as `void *` so this header needs no
    // ESP-IDF include.
    void *_timer;
    Callback _callback;
    void *_context;
};

}  // namespace hal
