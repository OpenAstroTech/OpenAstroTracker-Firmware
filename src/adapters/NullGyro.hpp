#pragma once

#include "ports/IGyro.hpp"

namespace adapters
{

/// `ports::IGyro` for a board without a levelling sensor.
///
/// The null object makes "no gyro" a runtime answer rather than a compile-time absence, so the
/// menus and the command layer carry no `#if` around the calls they make.
class NullGyro final : public ports::IGyro
{
  public:
    bool begin() override
    {
        return false;
    }

    void shutdown() override
    {
    }

    bool isPresent() override
    {
        return false;
    }

    ports::GyroAngles angles() override
    {
        return ports::GyroAngles{0.0f, 0.0f};
    }

    float temperature() override
    {
        return 99.0f;  // the sentinel the firmware has always reported for "not available"
    }
};

}  // namespace adapters
