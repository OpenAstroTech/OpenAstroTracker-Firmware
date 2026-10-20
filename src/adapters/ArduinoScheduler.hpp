#pragma once

#include "ports/IScheduler.hpp"

namespace adapters
{

/// `ports::IScheduler` over the Arduino `yield()`.
class ArduinoScheduler final : public ports::IScheduler
{
  public:
    void yield() override;
};

}  // namespace adapters
