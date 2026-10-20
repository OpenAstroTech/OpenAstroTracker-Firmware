# hal_fakes/

Hand-written fakes for the HAL interfaces in `src/hal/`. Test code only — nothing here
is compiled into the firmware.

| Fake | Implements | Test controls |
|------|-----------|---------------|
| `FakeSystemClock.hpp` | `hal::ISystemClock` and `ports::IClock` | `advance()`, `set()`; `delay()` advances the clock instead of blocking |
| `FakeGpioPin.hpp` | `hal::IGpioPin` | `setInputLevel()`, `setAnalogValue()`; records mode, output level, PWM duty and write count |
| `FakeSerialPort.hpp` | `hal::ISerialPort` | `feed()` to queue input, `output()` to read what was written, `clearOutput()`, `flushCount()` |
| `FakeEeprom.hpp` | `hal::IEeprom` | in-memory cells starting at `0xFF`, `setFailCommit()`, `fill()`, plus write/commit/begin counters |

Include them by relative path from an adapter test, e.g.

```cpp
#include "../test_common/hal_fakes/FakeEeprom.hpp"
```

`unit_tests/test_adapters/test_hal_fakes.cpp` pins the behaviour of the fakes themselves,
so a broken fake fails there instead of looking like a broken adapter.

See [PLAN.md](../../../PLAN.md) for the target architecture.
