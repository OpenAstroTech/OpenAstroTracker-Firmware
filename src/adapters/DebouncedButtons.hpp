#pragma once

#include "adapters/ButtonDebounce.hpp"
#include "hal/IButtonMatrix.hpp"
#include "ports/IButtons.hpp"
#include "ports/IClock.hpp"

namespace adapters
{

/// `ports::IButtons` over any button matrix.
///
/// The matrix reports what is held; this decides when it counts. The mapping from `hal::Button` to
/// `types::Button` is a cast because the two enumerations describe the same five keys in the same
/// order, which `static_assert`s below keep true.
class DebouncedButtons final : public ports::IButtons
{
  public:
    DebouncedButtons(hal::IButtonMatrix &matrix, ports::IClock &clock);

    types::Button pressed() override;

    bool keyChanged(types::Button *out) override;

    uint16_t rawValue() override;

  private:
    void poll();

    hal::IButtonMatrix &_matrix;
    ports::IClock &_clock;
    adapters::ButtonDebounce _debounce;
};

}  // namespace adapters
