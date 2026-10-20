#pragma once

#include <stdint.h>

#include "ports/IMotorDriver.hpp"

class Stream;
class TMC2209Stepper;

namespace adapters
{

/// `ports::IMotorDriver` over a TMC2209, in either UART wiring.
///
/// Two constructors rather than a flag: a driver behind a shared `Stream` (hardware
/// UART, or a `Stream` the caller already created) and one behind the library's own
/// software serial. Both own the `TMC2209Stepper` they create.
class Tmc2209Driver final : public ports::IMotorDriver
{
  public:
    Tmc2209Driver(Stream *serial, float rsense, uint8_t address);

    /// Software-serial wiring. On platforms without software serial the resulting
    /// driver is inert (every call is a no-op), matching the library's own
    /// "unsupported platform" behaviour.
    Tmc2209Driver(uint16_t softwareSerialRxPin, uint16_t softwareSerialTxPin, float rsense, uint8_t address);

    void begin() override;

    void beginSerial(uint32_t baudRate) override;

    /// Asks the driver to identify itself over UART. True when it answered.
    bool testConnection() override;

    void microsteps(uint16_t value) override;

    void rms_current(uint16_t milliamps) override;

    void rms_current(uint16_t milliamps, float holdMultiplier) override;

    void toff(uint8_t value) override;

    void blank_time(uint8_t value) override;

    void vsense(bool value) override;

    void semin(uint8_t value) override;

    void iholddelay(uint8_t value) override;

    void TPOWERDOWN(uint8_t value) override;

    void TCOOLTHRS(uint32_t value) override;

    void SGTHRS(uint8_t value) override;

    void en_spreadCycle(bool value) override;

    void pdn_disable(bool value) override;

    void mstep_reg_select(bool value) override;

    void I_scale_analog(bool value) override;

    void fclktrim(uint8_t value) override;

    void VACTUAL(int32_t value) override;

    void shaft(bool value) override;

    uint16_t rms_current() override;

    uint16_t cs_actual() override;

    bool vsense() override;

  private:
    // Owned but never freed: the driver lives for the lifetime of the program, which
    // is how the mount has always used it. TMCStepper's classes have no virtual
    // destructor, so deleting through this pointer would rely on the exact type.
    TMC2209Stepper *_driver;
};

}  // namespace adapters
