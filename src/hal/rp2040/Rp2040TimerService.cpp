#include "hal/WarningGuard.hpp"

PUSH_NO_WARNINGS
#include <hardware/timer.h>
POP_NO_WARNINGS

#include "hal/rp2040/Rp2040TimerService.hpp"

namespace hal
{

Rp2040TimerService::Rp2040TimerService() : _repeating(), _alarm(0), _callback(nullptr), _context(nullptr), _running(false)
{
}

Rp2040TimerService::~Rp2040TimerService()
{
    stop();
}

bool Rp2040TimerService::startPeriodic(uint32_t periodMicroseconds, Callback callback, void *context)
{
    stop();
    _callback = callback;
    _context  = context;
    _running  = add_repeating_timer_us(static_cast<int64_t>(periodMicroseconds), &Rp2040TimerService::onPeriodic, this, &_repeating);
    return _running;
}

bool Rp2040TimerService::startOneShot(uint32_t delayMicroseconds, Callback callback, void *context)
{
    stop();
    _callback = callback;
    _context  = context;
    _alarm    = add_alarm_in_us(delayMicroseconds, &Rp2040TimerService::onOneShot, this, false);
    _running  = _alarm >= 0;
    return _running;
}

void Rp2040TimerService::stop()
{
    if (_running)
    {
        cancel_repeating_timer(&_repeating);
        if (_alarm > 0)
        {
            cancel_alarm(_alarm);
            _alarm = 0;
        }
        _running = false;
    }
}

bool Rp2040TimerService::isRunning() const
{
    return _running;
}

bool Rp2040TimerService::onPeriodic(repeating_timer_t *timer)
{
    Rp2040TimerService *self = static_cast<Rp2040TimerService *>(timer->user_data);
    if (self != nullptr && self->_callback != nullptr && self->_running)
    {
        self->_callback(self->_context);
    }
    return self != nullptr && self->_running;
}

int64_t Rp2040TimerService::onOneShot(alarm_id_t id, void *context)
{
    (void) id;
    Rp2040TimerService *self = static_cast<Rp2040TimerService *>(context);
    if (self != nullptr)
    {
        self->_running = false;
        self->_alarm   = 0;
        if (self->_callback != nullptr)
        {
            self->_callback(self->_context);
        }
    }
    return 0;  // do not reschedule
}

}  // namespace hal
