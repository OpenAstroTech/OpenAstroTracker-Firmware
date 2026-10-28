#pragma once

#include <stddef.h>

#include <avr/pgmspace.h>

#include "AccelerationRamp.h"

namespace hal
{
namespace progmem_ramp
{
/// @brief The interval table of `RAMP`, emitted into flash.
template <typename RAMP, size_t... INDICES> struct Table {
    static const uint32_t data[sizeof...(INDICES)] PROGMEM;
};

template <typename RAMP, size_t... INDICES>
const uint32_t Table<RAMP, INDICES...>::data[sizeof...(INDICES)] PROGMEM = {RAMP::interval(INDICES)...};

/// @brief `Table` with indices `0 .. N-1`, spelled out for a C++11 pack expansion.
template <typename RAMP, size_t N, size_t... INDICES> struct MakeTable : MakeTable<RAMP, N - 1, N - 1, INDICES...> {};

template <typename RAMP, size_t... INDICES> struct MakeTable<RAMP, 0, INDICES...> {
    using type = Table<RAMP, INDICES...>;
};
}  // namespace progmem_ramp

/**
 * @brief `AccelerationRamp` with its step-interval table held in flash instead of SRAM.
 *
 * `AccelerationRamp` declares its table `constexpr static`, which the AVR backend places in
 * `.data`: 1 KiB per 256-stair ramp, 256 B per 64-stair ramp, resident in SRAM for the life of
 * the program. The table is read-only, so the copy belongs in `.progmem`; the same bytes then
 * cost flash, of which the AVR builds have far more to spare. The ISR reads a stair with
 * `pgm_read_dword`, a few cycles against the 500 us step budget.
 *
 * Everything but `interval()` is inherited unchanged, so `Stepper` sees the same interface and
 * the same compile-time constants.
 */
template <uint16_t STAIRS, uint32_t T_FREQ, uint32_t MAX_SPEED, uint32_t ACCELERATION>
class ProgmemAccelerationRamp : public AccelerationRamp<STAIRS, T_FREQ, MAX_SPEED, ACCELERATION>
{
    using Base = AccelerationRamp<STAIRS, T_FREQ, MAX_SPEED, ACCELERATION>;

    using Table = typename progmem_ramp::MakeTable<Base, STAIRS>::type;

  public:
    static inline __attribute__((always_inline)) uint32_t interval(const uint16_t stair)
    {
        return pgm_read_dword(&Table::data[stair]);
    }
};
}  // namespace hal
