#pragma once

namespace ports
{

/// A mechanical end switch guarding one end of an axis' travel.
///
/// One instance per switch: an axis with both a minimum and a maximum switch holds
/// two. Polarity is normalised by the adapter.
class IEndSwitch
{
  public:
    virtual ~IEndSwitch()
    {
    }

    virtual bool isTriggered() = 0;
};

}  // namespace ports
