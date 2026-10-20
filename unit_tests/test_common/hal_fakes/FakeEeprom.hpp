#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "hal/IEeprom.hpp"

namespace fakes
{

/// In-memory EEPROM. Cells start erased (0xFF) and `commit()` is counted so a test
/// can tell a buffered write from a durable one.
class FakeEeprom final : public hal::IEeprom
{
  public:
    explicit FakeEeprom(size_t size = 4096) : _cells(size, 0xFF)
    {
    }

    void begin(size_t size) override
    {
        _cells.assign(size, 0xFF);
        _beginCount++;
    }

    uint8_t read(size_t address) override
    {
        if (address >= _cells.size())
        {
            return 0xFF;
        }
        return _cells[address];
    }

    void write(size_t address, uint8_t value) override
    {
        if (address >= _cells.size())
        {
            return;
        }
        _cells[address] = value;
        _writeCount++;
    }

    void update(size_t address, uint8_t value) override
    {
        if (address >= _cells.size() || _cells[address] == value)
        {
            return;
        }
        write(address, value);
    }

    bool commit() override
    {
        _commitCount++;
        return !_failCommit;
    }

    size_t length() const override
    {
        return _cells.size();
    }

    // --- test controls ---

    void setFailCommit(bool fail)
    {
        _failCommit = fail;
    }

    void fill(uint8_t value)
    {
        _cells.assign(_cells.size(), value);
    }

    // --- assertions ---

    uint32_t writeCount() const
    {
        return _writeCount;
    }

    uint32_t commitCount() const
    {
        return _commitCount;
    }

    uint32_t beginCount() const
    {
        return _beginCount;
    }

  private:
    std::vector<uint8_t> _cells;
    uint32_t _writeCount  = 0;
    uint32_t _commitCount = 0;
    uint32_t _beginCount  = 0;
    bool _failCommit      = false;
};

}  // namespace fakes
