#pragma once

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <string>
#include <vector>

#include "hal/ISerialPort.hpp"

namespace fakes
{

/// A serial port whose input the test feeds and whose output the test inspects.
class FakeSerialPort final : public hal::ISerialPort
{
  public:
    int available() override
    {
        return static_cast<int>(_input.size());
    }

    int read() override
    {
        if (_input.empty())
        {
            return -1;
        }
        const uint8_t value = _input.front();
        _input.pop_front();
        return value;
    }

    int peek() override
    {
        return _input.empty() ? -1 : static_cast<int>(_input.front());
    }

    size_t write(uint8_t value) override
    {
        _output.push_back(value);
        return 1;
    }

    size_t write(const uint8_t *data, size_t length) override
    {
        _output.insert(_output.end(), data, data + length);
        return length;
    }

    void flush() override
    {
        _flushCount++;
    }

    // --- test controls ---

    void feed(const char *text)
    {
        for (const char *cursor = text; *cursor != '\0'; cursor++)
        {
            _input.push_back(static_cast<uint8_t>(*cursor));
        }
    }

    void feed(const uint8_t *data, size_t length)
    {
        _input.insert(_input.end(), data, data + length);
    }

    void clearOutput()
    {
        _output.clear();
    }

    // --- assertions ---

    std::string output() const
    {
        return std::string(_output.begin(), _output.end());
    }

    uint32_t flushCount() const
    {
        return _flushCount;
    }

  private:
    std::deque<uint8_t> _input;
    std::vector<uint8_t> _output;
    uint32_t _flushCount = 0;
};

}  // namespace fakes
