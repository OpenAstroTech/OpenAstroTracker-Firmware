#pragma once

namespace core
{

/// The value a TMC2209 expects in its MS register for a given microstep setting.
///
/// The datasheet encodes 1x as 0, so writing the configured number straight into the
/// register would ask for a fractional microstep count that does not exist. Every
/// `microsteps(...)` call in the firmware has to apply this rule, which is why it is one
/// function with a test rather than a `== 1 ? 0 : x` repeated at every call site.
constexpr int microstepRegisterValue(int microsteps)
{
    return (microsteps == 1) ? 0 : microsteps;
}

}  // namespace core
