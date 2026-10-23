#pragma once

#include <stdint.h>

#include "hal/IGpioPin.hpp"

namespace fakes
{

/// A pin that remembers how it was configured, what was written to it and what the
/// test says it should read.
class FakeGpioPin final : public hal::IGpioPin
{
  public:
    void configure(hal::PinMode mode) override
    {
        _mode       = mode;
        _configured = true;
    }

    void write(hal::PinLevel level) override
    {
        _level = level;
        _writeCount++;
    }

    hal::PinLevel read() const override
    {
        return _inputLevel;
    }

    uint16_t analogRead() const override
    {
        return _analogValue;
    }

    void writeAnalog(uint8_t duty) override
    {
        _analogDuty = duty;
        _writeCount++;
    }

    // --- test controls ---

    void setInputLevel(hal::PinLevel level)
    {
        _inputLevel = level;
    }

    void setAnalogValue(uint16_t value)
    {
        _analogValue = value;
    }

    // --- assertions ---

    bool isConfigured() const
    {
        return _configured;
    }

    hal::PinMode mode() const
    {
        return _mode;
    }

    hal::PinLevel level() const
    {
        return _level;
    }

    uint8_t analogDuty() const
    {
        return _analogDuty;
    }

    uint32_t writeCount() const
    {
        return _writeCount;
    }

  private:
    hal::PinMode _mode        = hal::PinMode::Input;
    hal::PinLevel _level      = hal::PinLevel::Low;
    hal::PinLevel _inputLevel = hal::PinLevel::Low;
    uint16_t _analogValue     = 0;
    uint8_t _analogDuty       = 0;
    uint32_t _writeCount      = 0;
    bool _configured          = false;
};

}  // namespace fakes
