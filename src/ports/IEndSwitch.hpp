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

    /// True while the switch is closed, in the sense that means "end reached" (the adapter fixes
    /// the polarity).
    virtual bool isTriggered() = 0;
};

}  // namespace ports
