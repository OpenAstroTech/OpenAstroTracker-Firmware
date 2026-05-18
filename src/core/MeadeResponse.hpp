#pragma once

/**
 * @file MeadeResponse.hpp
 * @brief Fixed-capacity NUL-terminated Meade reply value type.
 */

#include <stddef.h>

namespace oat
{
namespace core
{
namespace meade
{

class MeadeResponse
{
  public:
    static constexpr size_t Capacity = 200;

    MeadeResponse();

    const char *c_str() const
    {
        return _data;
    }

    size_t length() const
    {
        return _length;
    }

    bool empty() const
    {
        return _length == 0;
    }

    char operator[](size_t i) const
    {
        return _data[i];
    }

    operator const char *() const
    {
        return _data;
    }

    void assign(const char *s)
    {
        if (s == nullptr)
        {
            _data[0] = '\0';
            _length  = 0;
            return;
        }

        size_t i = 0;
        while ((s[i] != '\0') && (i + 1 < Capacity))
        {
            _data[i] = s[i];
            ++i;
        }
        _data[i] = '\0';
        _length  = i;
    }

    // Internal mutators used by parser-local response builders.
    char *buffer()
    {
        return _data;
    }

    static constexpr size_t capacity()
    {
        return Capacity;
    }

    void setLength(size_t n)
    {
        _length = n;
    }

  private:
    char _data[Capacity];
    size_t _length;
};

using MeadePayload = MeadeResponse;

}  // namespace meade
}  // namespace core
}  // namespace oat
