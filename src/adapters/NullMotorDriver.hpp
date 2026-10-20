#pragma once

#include <stdint.h>

#include "ports/IMotorDriver.hpp"

namespace adapters
{

/// The driver port for boards whose steppers are not driven over UART.
///
/// A null object rather than a `#ifdef`: the composition root injects this when the
/// configured `*_DRIVER_TYPE` has no registers to talk to (A4988, or a TMC2209 in
/// standalone mode), so the domain calls the port unconditionally.
class NullMotorDriver final : public ports::IMotorDriver
{
  public:
    void begin() override
    {
    }

    void beginSerial(uint32_t baudRate) override
    {
        (void) baudRate;
    }

    bool testConnection() override
    {
        return false;
    }

    void microsteps(uint16_t value) override
    {
        (void) value;
    }

    void rms_current(uint16_t milliamps) override
    {
        (void) milliamps;
    }

    void rms_current(uint16_t milliamps, float holdMultiplier) override
    {
        (void) milliamps;
        (void) holdMultiplier;
    }

    void toff(uint8_t value) override
    {
        (void) value;
    }

    void blank_time(uint8_t value) override
    {
        (void) value;
    }

    void vsense(bool value) override
    {
        (void) value;
    }

    void semin(uint8_t value) override
    {
        (void) value;
    }

    void iholddelay(uint8_t value) override
    {
        (void) value;
    }

    void TPOWERDOWN(uint8_t value) override
    {
        (void) value;
    }

    void TCOOLTHRS(uint32_t value) override
    {
        (void) value;
    }

    void SGTHRS(uint8_t value) override
    {
        (void) value;
    }

    void en_spreadCycle(bool value) override
    {
        (void) value;
    }

    void pdn_disable(bool value) override
    {
        (void) value;
    }

    void mstep_reg_select(bool value) override
    {
        (void) value;
    }

    void I_scale_analog(bool value) override
    {
        (void) value;
    }

    void fclktrim(uint8_t value) override
    {
        (void) value;
    }

    void VACTUAL(int32_t value) override
    {
        (void) value;
    }

    void shaft(bool value) override
    {
        (void) value;
    }

    uint16_t rms_current() override
    {
        return 0;
    }

    uint16_t cs_actual() override
    {
        return 0;
    }

    bool vsense() override
    {
        return false;
    }
};

}  // namespace adapters
