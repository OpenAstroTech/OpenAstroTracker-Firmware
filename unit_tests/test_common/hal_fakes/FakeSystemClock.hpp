#pragma once

#include <stdint.h>

#include "hal/ISystemClock.hpp"
#include "ports/IClock.hpp"

namespace fakes
{

/// A clock the test moves by hand. `delay()` does not block; it records the request
/// and advances the clock by the same amount, which is what a real delay would do to
/// a subsequent `millis()`.
class FakeSystemClock final : public hal::ISystemClock
{
  public:
    uint32_t millis() const override
    {
        return _millis;
    }

    uint32_t micros() const override
    {
        return _micros;
    }

    void delay(uint32_t milliseconds) const override
    {
        _delaysRequested++;
        advance(milliseconds);
    }

    void delayMicroseconds(uint16_t microseconds) const override
    {
        _microDelaysRequested++;
        _micros += microseconds;
    }

    /// Moves time forward without recording a delay request.
    void advance(uint32_t milliseconds) const
    {
        _millis += milliseconds;
        _micros += milliseconds * 1000u;
    }

    void set(uint32_t milliseconds) const
    {
        _millis = milliseconds;
        _micros = milliseconds * 1000u;
    }

    uint32_t delaysRequested() const
    {
        return _delaysRequested;
    }

    uint32_t microDelaysRequested() const
    {
        return _microDelaysRequested;
    }

  private:
    mutable uint32_t _millis             = 0;
    mutable uint32_t _micros             = 0;
    mutable uint32_t _delaysRequested    = 0;
    mutable uint32_t _microDelaysRequested = 0;
};

/// The domain-side clock (`ports::IClock`) backed by a hand-moved counter.
class FakeClock final : public ports::IClock
{
  public:
    uint32_t now() override
    {
        return _now;
    }

    void delay(uint32_t milliseconds) override
    {
        _delaysRequested++;
        _now += milliseconds;
    }

    void advance(uint32_t milliseconds)
    {
        _now += milliseconds;
    }

    uint32_t delaysRequested() const
    {
        return _delaysRequested;
    }

  private:
    uint32_t _now             = 0;
    uint32_t _delaysRequested = 0;
};

}  // namespace fakes
