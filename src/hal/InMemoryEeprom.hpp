#pragma once

#include <stddef.h>
#include <stdint.h>

#include "hal/IEeprom.hpp"

namespace hal
{

/// A backend that stores cells in RAM and forgets them at power-down.
///
/// It exists for `USE_DUMMY_EEPROM` builds (a development switch that lets a board
/// boot without touching its EEPROM) and as the reference behaviour for other
/// backends: cells start erased (0xFF), and `commit()` always succeeds.
template <size_t SIZE> class InMemoryEeprom final : public IEeprom
{
  public:
    InMemoryEeprom()
    {
        for (size_t i = 0; i < SIZE; i++)
        {
            _cells[i] = 0xFF;
        }
    }

    void begin(size_t size) override
    {
        (void) size;  // the buffer size is fixed at compile time
    }

    uint8_t read(size_t address) override
    {
        return address < SIZE ? _cells[address] : 0xFF;
    }

    void write(size_t address, uint8_t value) override
    {
        if (address < SIZE)
        {
            _cells[address] = value;
        }
    }

    void update(size_t address, uint8_t value) override
    {
        if (address < SIZE && _cells[address] != value)
        {
            _cells[address] = value;
        }
    }

    bool commit() override
    {
        return true;
    }

    size_t length() const override
    {
        return SIZE;
    }

  private:
    uint8_t _cells[SIZE];
};

}  // namespace hal
