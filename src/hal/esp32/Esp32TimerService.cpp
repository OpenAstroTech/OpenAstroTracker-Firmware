#include "hal/WarningGuard.hpp"

PUSH_NO_WARNINGS
#include <esp_timer.h>
POP_NO_WARNINGS

#include "hal/esp32/Esp32TimerService.hpp"

namespace hal
{

Esp32TimerService::Esp32TimerService() : _timer(nullptr), _callback(nullptr), _context(nullptr)
{
    esp_timer_create_args_t args = {};
    args.callback                = &Esp32TimerService::onTimer;
    args.arg                     = this;
    args.dispatch_method         = ESP_TIMER_TASK;
    args.name                    = "oat-timer";

    esp_timer_handle_t handle = nullptr;
    if (esp_timer_create(&args, &handle) == ESP_OK)
    {
        _timer = handle;
    }
}

Esp32TimerService::~Esp32TimerService()
{
    if (_timer != nullptr)
    {
        esp_timer_stop(static_cast<esp_timer_handle_t>(_timer));
        esp_timer_delete(static_cast<esp_timer_handle_t>(_timer));
        _timer = nullptr;
    }
}

bool Esp32TimerService::startPeriodic(uint32_t periodMicroseconds, Callback callback, void *context)
{
    if (_timer == nullptr)
    {
        return false;
    }

    esp_timer_stop(static_cast<esp_timer_handle_t>(_timer));
    _callback = callback;
    _context  = context;
    return esp_timer_start_periodic(static_cast<esp_timer_handle_t>(_timer), periodMicroseconds) == ESP_OK;
}

bool Esp32TimerService::startOneShot(uint32_t delayMicroseconds, Callback callback, void *context)
{
    if (_timer == nullptr)
    {
        return false;
    }

    esp_timer_stop(static_cast<esp_timer_handle_t>(_timer));
    _callback = callback;
    _context  = context;
    return esp_timer_start_once(static_cast<esp_timer_handle_t>(_timer), delayMicroseconds) == ESP_OK;
}

void Esp32TimerService::stop()
{
    if (_timer != nullptr)
    {
        esp_timer_stop(static_cast<esp_timer_handle_t>(_timer));
    }
}

bool Esp32TimerService::isRunning() const
{
    return _timer != nullptr && esp_timer_is_active(static_cast<esp_timer_handle_t>(_timer));
}

void Esp32TimerService::onTimer(void *argument)
{
    Esp32TimerService *self = static_cast<Esp32TimerService *>(argument);
    if (self != nullptr && self->_callback != nullptr)
    {
        self->_callback(self->_context);
    }
}

}  // namespace hal
