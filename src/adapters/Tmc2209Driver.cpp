#include "hal/WarningGuard.hpp"

PUSH_NO_WARNINGS
#include <Arduino.h>
#include <TMCStepper.h>
POP_NO_WARNINGS

#include "adapters/Tmc2209Driver.hpp"

namespace adapters
{

Tmc2209Driver::Tmc2209Driver(Stream *serial, float rsense, uint8_t address) : _driver(nullptr)
{
    _driver = new TMC2209Stepper(serial, rsense, address);
}

Tmc2209Driver::Tmc2209Driver(uint16_t softwareSerialRxPin, uint16_t softwareSerialTxPin, float rsense, uint8_t address) : _driver(nullptr)
{
#if SW_CAPABLE_PLATFORM
    _driver = new TMC2209Stepper(softwareSerialRxPin, softwareSerialTxPin, rsense, address);
#else
    // The library deletes this constructor where it has no software serial; the mount
    // only takes this path on platforms that do have it.
    (void) softwareSerialRxPin;
    (void) softwareSerialTxPin;
    (void) rsense;
    (void) address;
#endif
}

bool Tmc2209Driver::testConnection()
{
    return _driver != nullptr && _driver->test_connection() == 0;
}

void Tmc2209Driver::begin()
{
    if (_driver != nullptr)
    {
        _driver->begin();
    }
}

void Tmc2209Driver::beginSerial(uint32_t baudRate)
{
#if SW_CAPABLE_PLATFORM
    if (_driver != nullptr)
    {
        _driver->beginSerial(baudRate);
    }
#else
    (void) baudRate;  // the library has no software serial on this platform
#endif
}

void Tmc2209Driver::microsteps(uint16_t value)
{
    if (_driver != nullptr)
    {
        _driver->microsteps(value);
    }
}

void Tmc2209Driver::rms_current(uint16_t milliamps)
{
    if (_driver != nullptr)
    {
        _driver->rms_current(milliamps);
    }
}

void Tmc2209Driver::rms_current(uint16_t milliamps, float holdMultiplier)
{
    if (_driver != nullptr)
    {
        _driver->rms_current(milliamps, holdMultiplier);
    }
}

void Tmc2209Driver::toff(uint8_t value)
{
    if (_driver != nullptr)
    {
        _driver->toff(value);
    }
}

void Tmc2209Driver::blank_time(uint8_t value)
{
    if (_driver != nullptr)
    {
        _driver->blank_time(value);
    }
}

void Tmc2209Driver::vsense(bool value)
{
    if (_driver != nullptr)
    {
        _driver->vsense(value);
    }
}

void Tmc2209Driver::semin(uint8_t value)
{
    if (_driver != nullptr)
    {
        _driver->semin(value);
    }
}

void Tmc2209Driver::iholddelay(uint8_t value)
{
    if (_driver != nullptr)
    {
        _driver->iholddelay(value);
    }
}

void Tmc2209Driver::TPOWERDOWN(uint8_t value)
{
    if (_driver != nullptr)
    {
        _driver->TPOWERDOWN(value);
    }
}

void Tmc2209Driver::TCOOLTHRS(uint32_t value)
{
    if (_driver != nullptr)
    {
        _driver->TCOOLTHRS(value);
    }
}

void Tmc2209Driver::SGTHRS(uint8_t value)
{
    if (_driver != nullptr)
    {
        _driver->SGTHRS(value);
    }
}

void Tmc2209Driver::en_spreadCycle(bool value)
{
    if (_driver != nullptr)
    {
        _driver->en_spreadCycle(value);
    }
}

void Tmc2209Driver::pdn_disable(bool value)
{
    if (_driver != nullptr)
    {
        _driver->pdn_disable(value);
    }
}

void Tmc2209Driver::mstep_reg_select(bool value)
{
    if (_driver != nullptr)
    {
        _driver->mstep_reg_select(value);
    }
}

void Tmc2209Driver::I_scale_analog(bool value)
{
    if (_driver != nullptr)
    {
        _driver->I_scale_analog(value);
    }
}

void Tmc2209Driver::fclktrim(uint8_t value)
{
    if (_driver != nullptr)
    {
        _driver->fclktrim(value);
    }
}

void Tmc2209Driver::VACTUAL(int32_t value)
{
    if (_driver != nullptr)
    {
        _driver->VACTUAL(value);
    }
}

void Tmc2209Driver::shaft(bool value)
{
    if (_driver != nullptr)
    {
        _driver->shaft(value);
    }
}

uint16_t Tmc2209Driver::rms_current()
{
    return _driver != nullptr ? _driver->rms_current() : 0;
}

uint16_t Tmc2209Driver::cs_actual()
{
    return _driver != nullptr ? _driver->cs_actual() : 0;
}

bool Tmc2209Driver::vsense()
{
    return _driver != nullptr && _driver->vsense();
}

}  // namespace adapters
