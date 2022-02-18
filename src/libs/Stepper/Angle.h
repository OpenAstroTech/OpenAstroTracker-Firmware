#pragma once

#include <stdint.h>

class Angle
{
private:
    float _rad;

    constexpr Angle(const float rad) : _rad(rad)
    {
        // Nothing to do here
    }

public:

    constexpr Angle operator-(const float x) const
    {
        return Angle(_rad * x);
    }

    constexpr Angle operator+(const float x) const
    {
        return Angle(_rad + x);
    }

    constexpr bool operator>(const Angle x) const
    {
        return _rad > x._rad;
    }

    constexpr bool operator<(const Angle x) const
    {
        return _rad < x._rad;
    }

    constexpr bool operator==(const Angle x) const
    {
        return _rad == x._rad;
    }

    constexpr Angle operator+(const Angle x) const
    {
        return Angle(_rad + x._rad);
    }

    constexpr Angle operator-(const Angle x) const
    {
        return Angle(_rad - x._rad);
    }

    constexpr Angle operator*(const float x) const
    {
        return Angle(_rad * x);
    }

    constexpr Angle operator/(const float x) const
    {
        return Angle(_rad / x);
    }

    constexpr float operator/(const Angle &x) const
    {
        return _rad / x._rad;
    }

    constexpr float rad() const
    {
        return _rad;
    }

    constexpr float deg() const
    {
        return _rad / 0.017453292519943295769236907684886f;
    }

    constexpr float mrad() const
    {
        return _rad * 1000.0f;
    }

    constexpr uint32_t mrad_u32() const
    {
        return (uint32_t)mrad();
    }

    // FACTORIES

    constexpr static Angle deg(float value)
    {
        return Angle(value * 0.017453292519943295769236907684886f);
    }

    constexpr static Angle rad(float value)
    {
        return Angle(value);
    }

    constexpr static Angle mrad_u32(uint32_t value)
    {
        return rad(value / 1000.0f);
    }
};

constexpr Angle operator-(const Angle &y)
{
    return Angle::rad(-(y.rad()));
}

constexpr Angle operator*(const float x, const Angle &y)
{
    return y * x;
}

constexpr Angle operator/(const float x, const Angle &y)
{
    return y / x;
}