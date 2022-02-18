#pragma once

class NewtonRaphson
{
private:
    NewtonRaphson() = delete;

    static float constexpr sqrt(float x, float curr, float prev)
    {
        return curr == prev
                   ? curr
                   : sqrt(x, 0.5f * (curr + x / curr), curr);
    }

public:
    static float constexpr sqrt(float x)
    {
        return sqrt(x, x, 0);
    }
};