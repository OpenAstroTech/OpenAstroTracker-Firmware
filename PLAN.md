# Layering Refactor: Decouple Adapters from Core & Extract the Meade Protocol

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Enforce two architectural rules in code and docs: (1) `adapters/` never depends on `core/`; (2) `core/` contains only business logic — no wire protocol, no presentation/input conditioning.

**Architecture:** Add a dependency-free `src/types/` vocabulary leaf under the existing four layers; move the Meade protocol out of `core/` into `adapters/meade/` as a *driving adapter*; move presentation/input helpers into `adapters/`; delete the two "adapter mutates core state" round-trips (`StatusFlags`, `SiteTimePort`). Everything is a mechanical move or a small re-home — no behaviour change to the Meade wire format or the domain rules.

**Tech Stack:** C++ (gnu++11 on AVR, gnu++17 on ESP32/RP2040/native), PlatformIO (`native` env + board envs), ETL in `ETL_NO_STL` mode, GoogleTest on the `native` env, `git mv` + `perl -pi -e` for mechanical renames.

---

## Recorded decisions

| # | Decision | Choice | Why |
|---|----------|--------|-----|
| D1 | Home of pure value vocabulary | **New leaf layer `src/types/`** | User selected "shared leaf" over moving into `ports/`; value types are shared by core, ports, adapters and app, and "vocabulary" is not a domain interface. |
| D2 | Scope | **Full** — includes the design-heavy `StatusFlags`/`SiteTimePort` rework, not just the mechanical moves. | User selected "full". |
| D3 | Home of the Meade protocol | **`src/adapters/meade/`** | The protocol is a driving adapter: it translates bytes into calls on `ports/` mount interfaces. `MeadeCommandAdapter` already lives in `adapters/`. |
| D4 | Home of events | `MountEvent`/`MountEventType` → `types/`; `IEventSink` → `ports/` | Events are shared value types; the sink is a subscription interface, hence a port. |
| D5 | Namespaces | value types `core::X` → `types::X`; `oat::core::meade` → `adapters::meade`; presentation helpers `core::X` → `adapters::X` | Resolves the existing `::core` vs `oat::core::meade` split and keeps layer↔namespace in sync. |

## Constraints (carried over from the old PLAN.md — still binding)

- `core/` must never include `<Arduino.h>` or use `String`, `Serial`, `millis()`, `LOG()`.
- AVR ATmega2560: 256 KB flash / 8 KB SRAM; keep vtable count small, mark adapters `final`.
- `core/` stays C++11-compatible (no `if constexpr`); AVR envs inherit gnu++11.
- Meade LX200 behaviour is frozen byte-for-byte (Stellarium/ASCOM).
- Every phase must be independently shippable: all boards green, `pio test -e native` green.
- Enforced formatter: clang-format v12 (140 col, Allman braces). `src/libs/TimerInterrupt/` is excluded.

---

## Target file map

```
src/
├── types/                              # NEW — dependency-free vocabulary (nothing but std headers)
│   ├── DayTime.{hpp,cpp}               #   (moved from core/types/, namespace types)
│   ├── Declination.{hpp,cpp}
│   ├── Latitude.{hpp,cpp}
│   ├── Longitude.{hpp,cpp}
│   ├── RightAscension.{hpp,cpp}
│   ├── MathUtil.hpp                    #   types::clamp / adjustWrap / adjustClamp / arraySize
│   ├── Button.hpp                      #   types::Button (the key enum)
│   ├── FocuserDirection.hpp            #   split from core/FocusController.hpp
│   └── MountEvent.hpp                  #   types::MountEventType + types::MountEvent
├── ports/                              # domain interfaces + subscription contract
│   ├── IEventSink.hpp                  # NEW — ports::IEventSink (split from core/MountEvent.hpp)
│   ├── PortVocabulary.hpp              #   (moved from core/, keep global symbols as-is this pass)
│   └── ... (all existing ports; includes point at types/ not core/types/)
├── core/                               # business logic only
│   ├── controllers, algorithms, MountCoordinator, EventBus, MountState, MountConfig, SiteTime
│   ├── SiteTimeService.{hpp,cpp}       # NEW — write protocol moved here from adapters/SiteTimePort
│   └── (meade/, types/, Button.hpp, MathUtil.hpp, PortVocabulary.hpp, MountEvent.hpp, InfoDisplayLayout.*, ConsoleLog.*, ButtonDebounce.* removed)
├── adapters/
│   ├── meade/                          # NEW — the whole protocol driver (moved from core/meade/, namespace adapters::meade)
│   ├── InfoDisplayLayout.{hpp,cpp}     #   (moved from core/, namespace adapters)
│   ├── ConsoleLog.{hpp,cpp}
│   ├── ButtonDebounce.{hpp,cpp}
│   └── (StatusFlags.*, SiteTimePort.* removed)
├── hal/                                # unchanged
└── app/                                # composition root; include paths updated
```

### Namespace & include-path rename table (mechanical, applied once)

| Before | After |
|---|---|
| `#include "core/types/<X>.hpp"` | `#include "types/<X>.hpp"` |
| `#include "core/MathUtil.hpp"` | `#include "types/MathUtil.hpp"` |
| `#include "core/Button.hpp"` | `#include "types/Button.hpp"` |
| `#include "core/PortVocabulary.hpp"` | `#include "ports/PortVocabulary.hpp"` |
| `#include "core/MountEvent.hpp"` | `#include "types/MountEvent.hpp"` + `#include "ports/IEventSink.hpp"` |
| `#include "core/meade/<X>.hpp"` | `#include "adapters/meade/<X>.hpp"` |
| `#include "core/InfoDisplayLayout.hpp"` / `core/ConsoleLog.hpp` / `core/ButtonDebounce.hpp` | `#include "adapters/<X>.hpp"` |
| `core::DayTime`, `core::Declination`, `core::Latitude`, `core::Longitude`, `core::RightAscension` | `types::…` |
| `core::clamp`, `core::adjustWrap`, `core::adjustClamp`, `core::arraySize` | `types::…` |
| `core::Button` | `types::Button` |
| `core::FocuserDirection`, `core::FOCUS_BACKWARD`, `core::FOCUS_FORWARD` | `types::…` |
| `core::MountEvent`, `core::MountEventType` | `types::…` |
| `core::IEventSink` | `ports::IEventSink` |
| `oat::core::meade` | `adapters::meade` |
| `core::InfoDisplayScale`, `core::xRAPixel`, `core::yDECPixel`, `core::progressBarWidth`, `core::markerRaHours`, `core::markerDecDegrees` | `adapters::…` |
| `core::ConsoleLog` | `adapters::ConsoleLog` |
| `core::ButtonDebounce` | `adapters::ButtonDebounce` |

> `perl -pi -e` is used throughout instead of `sed` because macOS ships BSD `sed`, which rejects the `\b`/alternation forms used here. `\b` word boundaries make each symbol rename exact (e.g. `\bcore::Button\b` will not touch `core::ButtonDebounce`).

---

## Phase overview

| Phase | Content | Verifies with |
|---|---|---|
| 0 | Baseline | `pio test -e native`, `pio run -e ramps` green before any change |
| 1 | Extract `src/types/` (value types, MathUtil, Button, FocuserDirection) | native tests + ramps build |
| 2 | Move events: `MountEvent`→types, `IEventSink`→ports | native tests + ramps build |
| 3 | Move `PortVocabulary` to ports | native tests + ramps build |
| 4 | Move Meade protocol to `adapters/meade/` | native tests + ramps build |
| 5 | Re-home presentation/input logic (InfoDisplayLayout, ConsoleLog, ButtonDebounce) | native tests + ramps build |
| 6 | Delete `StatusFlags`/`IStatusFlags` indirection | native tests + ramps build |
| 7 | Move site/time write protocol into `core/SiteTimeService` | native tests + ramps build |
| 8 | Update ARCHITECTURE.md + full matrix build + clang-format | `python matrix_build.py`, format check |

Phases 1–3 are prerequisites for 4 (the Meade driver must not see `core/types/…` or `core/MountEvent.hpp`). Phases 5–7 are independent of 4 and of each other.

Every commit carries the trailer:

```
Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>
```

---

## Phase 0: Baseline

**Files:** none.

- [ ] **Step 1: Confirm the tree builds and tests are green before touching anything**

```bash
pio test -e native
```
Expected: all tests pass (`test_core`, `test_adapters`, `test_hal`, `test_ui`), exit 0.

- [ ] **Step 2: Confirm a representative board build is green**

```bash
pio run -e ramps
```
Expected: `SUCCESS` (no warnings; `-Werror` is on).

- [ ] **Step 3: Record the starting HEAD**

```bash
git rev-parse --short HEAD && git status --short
```
Expected: clean tree except the pre-existing ` D PLAN.md` (this file replaces it).

---

## Phase 1: Extract the `src/types/` vocabulary

Moves the five value types plus `MathUtil`, `Button`, and a new `FocuserDirection` into the dependency-free `src/types/` leaf, and renames their namespace from `core` to `types`.

**Files:**
- Create: `src/types/` (the 8 files below, via `git mv`)
- Move: `src/core/types/{DayTime,Declination,Latitude,Longitude,RightAscension}.{hpp,cpp}`, `src/core/MathUtil.hpp`, `src/core/Button.hpp`
- Create: `src/types/FocuserDirection.hpp` (split from `src/core/FocusController.hpp`)
- Modify: `src/core/FocusController.hpp` (drop the enum, include the new header)
- Modify: all 25 `core/types/` includers, 12 `core/MathUtil.hpp` includers, 17 `core/Button.hpp`/`core/PortVocabulary.hpp` includers (include-path + `core::`→`types::` renames)

- [ ] **Step 1: Move the files**

```bash
mkdir -p src/types
git mv src/core/types/DayTime.hpp     src/types/DayTime.hpp
git mv src/core/types/DayTime.cpp     src/types/DayTime.cpp
git mv src/core/types/Declination.hpp src/types/Declination.hpp
git mv src/core/types/Declination.cpp src/types/Declination.cpp
git mv src/core/types/Latitude.hpp    src/types/Latitude.hpp
git mv src/core/types/Latitude.cpp    src/types/Latitude.cpp
git mv src/core/types/Longitude.hpp   src/types/Longitude.hpp
git mv src/core/types/Longitude.cpp   src/types/Longitude.cpp
git mv src/core/types/RightAscension.hpp src/types/RightAscension.hpp
git mv src/core/types/RightAscension.cpp src/types/RightAscension.cpp
git mv src/core/MathUtil.hpp src/types/MathUtil.hpp
git mv src/core/Button.hpp  src/types/Button.hpp
rmdir src/core/types
```

- [ ] **Step 2: Rename the namespace inside the moved files**

The moved files are self-contained (they reference only each other and std headers), so inside `src/types/` every `core` qualifier becomes `types`:

```bash
perl -pi -e 's/\bnamespace core\b/namespace types/g; s/\bcore::/types::/g' src/types/*.hpp src/types/*.cpp
```

Expected: `grep -rn '\bcore\b' src/types/` returns nothing.

- [ ] **Step 3: Create `src/types/FocuserDirection.hpp`**

```cpp
#pragma once

namespace types
{

/// Which way the focuser turns when it runs continuously.
enum FocuserDirection
{
    FOCUS_BACKWARD = -1,
    FOCUS_FORWARD  = 1,
};

}  // namespace types
```

- [ ] **Step 4: Strip the enum out of `src/core/FocusController.hpp`**

Delete the `enum FocuserDirection { … };` block and add `#include "types/FocuserDirection.hpp"` at the top; change every `FocuserDirection`/`FOCUS_BACKWARD`/`FOCUS_FORWARD` inside the file to `types::FocuserDirection`/`types::FOCUS_BACKWARD`/`types::FOCUS_FORWARD` (the controller keeps `FocusMode`/`FocusUpdate`). Confirm with:

```bash
grep -n 'FocuserDirection' src/core/FocusController.hpp
```
Expected: only `types::FocuserDirection` occurrences remain.

- [ ] **Step 5: Rewrite include paths across the tree**

```bash
perl -pi -e 's#core/types/#types/#g' $(grep -rl 'core/types/' src unit_tests)
perl -pi -e 's#core/MathUtil\.hpp#types/MathUtil.hpp#g' $(grep -rl 'core/MathUtil.hpp' src unit_tests)
perl -pi -e 's#core/Button\.hpp#types/Button.hpp#g' $(grep -rl 'core/Button.hpp' src unit_tests)
```

- [ ] **Step 6: Rewrite the symbols across the tree**

```bash
perl -pi -e 's/\bcore::DayTime\b/types::DayTime/g; s/\bcore::Declination\b/types::Declination/g;
s/\bcore::Latitude\b/types::Latitude/g; s/\bcore::Longitude\b/types::Longitude/g;
s/\bcore::RightAscension\b/types::RightAscension/g' $(grep -rl 'core::DayTime\|core::Declination\|core::Latitude\|core::Longitude\|core::RightAscension' src unit_tests)

perl -pi -e 's/\bcore::clamp\b/types::clamp/g; s/\bcore::adjustWrap\b/types::adjustWrap/g;
s/\bcore::adjustClamp\b/types::adjustClamp/g; s/\bcore::arraySize\b/types::arraySize/g' $(grep -rl 'core::clamp\|core::adjustWrap\|core::adjustClamp\|core::arraySize' src unit_tests)

perl -pi -e 's/\bcore::Button\b/types::Button/g' $(grep -rl '\bcore::Button\b' src unit_tests)

perl -pi -e 's/\bcore::FocuserDirection\b/types::FocuserDirection/g; s/\bcore::FOCUS_BACKWARD\b/types::FOCUS_BACKWARD/g; s/\bcore::FOCUS_FORWARD\b/types::FOCUS_FORWARD/g' $(grep -rl 'core::FocuserDirection\|core::FOCUS_BACKWARD\|core::FOCUS_FORWARD' src unit_tests)
```

- [ ] **Step 7: Verify no stragglers**

```bash
grep -rn 'core::DayTime\|core::Declination\|core::Latitude\|core::Longitude\|core::RightAscension\|core::clamp\|core::Button\b\|core::FocuserDirection\|core/types/\|core/MathUtil.hpp\|core/Button.hpp' src unit_tests
```
Expected: no output.

- [ ] **Step 8: Build & test**

```bash
pio test -e native && pio run -e ramps
```
Expected: native tests pass; `SUCCESS` on ramps.

- [ ] **Step 9: Commit**

```bash
git add -A && git commit -m "refactor(types): extract src/types/ value vocabulary (namespace types)"
```

---

## Phase 2: Move events out of core

`MountEvent`/`MountEventType` become `types::` value types; `IEventSink` becomes a `ports::` interface. This is the prerequisite that lets the Meade driver (Phase 4) reference events without touching `core/`.

**Files:**
- Create: `src/types/MountEvent.hpp`, `src/ports/IEventSink.hpp`
- Delete: `src/core/MountEvent.hpp`
- Modify: `src/core/EventBus.hpp`, `src/core/MountCoordinator.hpp`, `src/ports/IMountEvents.hpp`, `src/core/meade/PendingReplies.hpp`, `src/adapters/InfoDisplayPanel.{hpp,cpp}`, `src/adapters/MeadeCommandAdapter.{hpp,cpp}`, `src/app/MountExecutor*.cpp`, the 4 test files using the symbols

- [ ] **Step 1: Create `src/types/MountEvent.hpp`**

Copy the enum + struct from `src/core/MountEvent.hpp`, namespace `core` → `types`, and **drop the `IEventSink` class**:

```cpp
#pragma once

#include <stdint.h>

namespace types
{

enum class MountEventType : uint8_t
{
    TrackingStarted,
    TrackingStopped,
    SlewStarted,
    SlewArrived,
    ParkingStarted,
    Parked,
    HomingStarted,
    HomingFinished,
    GuidingStarted,
    GuidingStopped,
    FocusMoving,
    FocusStopped,
};

struct MountEvent {
    MountEventType type;
    bool succeeded;
};

}  // namespace types
```

- [ ] **Step 2: Create `src/ports/IEventSink.hpp`**

```cpp
#pragma once

#include "types/MountEvent.hpp"

namespace ports
{

/// Anything that wants to observe mount events.
class IEventSink
{
  public:
    virtual ~IEventSink()
    {
    }

    virtual void onMountEvent(const types::MountEvent &event) = 0;
};

}  // namespace ports
```

- [ ] **Step 3: Delete the old header**

```bash
git rm src/core/MountEvent.hpp
```

- [ ] **Step 4: Update `src/ports/IMountEvents.hpp`**

Replace the `namespace core { class IEventSink; }` forward-declaration block with `#include "IEventSink.hpp"` and change the two `::core::IEventSink *` parameters to `IEventSink *`.

- [ ] **Step 5: Update `src/core/EventBus.hpp`**

Replace `#include "MountEvent.hpp"` with:

```cpp
#include "types/MountEvent.hpp"
#include "ports/IEventSink.hpp"
```

and rename every `MountEvent`/`MountEventType`/`IEventSink` use inside to `types::MountEvent`/`types::MountEventType`/`ports::IEventSink`.

- [ ] **Step 6: Mechanical rename across the tree**

```bash
perl -pi -e 's/\bcore::MountEvent\b/types::MountEvent/g; s/\bcore::MountEventType\b/types::MountEventType/g; s/\bcore::IEventSink\b/ports::IEventSink/g' $(grep -rl 'core::MountEvent\|core::IEventSink' src unit_tests)
```

Note the `::core::IEventSink` forms in `InfoDisplayPanel.hpp` (`private ::core::IEventSink`) and `MeadeCommandAdapter.hpp` (`public ::core::IEventSink`) are handled by the same pattern — `\b` matches right before `core` after `::`.

- [ ] **Step 7: Fix the four files that included `core/MountEvent.hpp`**

`src/core/meade/PendingReplies.hpp`, `src/adapters/InfoDisplayPanel.hpp`, `src/adapters/MeadeCommandAdapter.hpp` (plus `src/core/EventBus.hpp` from Step 5) must now include **both**:

```cpp
#include "types/MountEvent.hpp"
#include "ports/IEventSink.hpp"
```

- [ ] **Step 8: Verify**

```bash
grep -rn 'core::MountEvent\|core::IEventSink\|core/MountEvent.hpp' src unit_tests
```
Expected: no output.

- [ ] **Step 9: Build & test**

```bash
pio test -e native && pio run -e ramps
```

- [ ] **Step 10: Commit**

```bash
git add -A && git commit -m "refactor(events): MountEvent to types/, IEventSink to ports/"
```

---

## Phase 3: Move `PortVocabulary` to ports

`src/core/PortVocabulary.hpp` is dependency-free port vocabulary (direction bits, coordinate-format selectors, `StepperAxis`, `LocalDate`). It moves to `src/ports/PortVocabulary.hpp`. Its global `#define`s and un-namespaced enum/struct are left as-is this pass (converting them to `enum class` is a separate hygiene task).

**Files:**
- Move: `src/core/PortVocabulary.hpp` → `src/ports/PortVocabulary.hpp`
- Modify: its 17 includers (include path only — symbols are global)

- [ ] **Step 1: Move the file**

```bash
git mv src/core/PortVocabulary.hpp src/ports/PortVocabulary.hpp
```

- [ ] **Step 2: Rewrite include paths**

```bash
perl -pi -e 's#core/PortVocabulary\.hpp#ports/PortVocabulary.hpp#g' $(grep -rl 'core/PortVocabulary.hpp' src unit_tests)
```

- [ ] **Step 3: Verify**

```bash
grep -rn 'core/PortVocabulary.hpp' src unit_tests
```
Expected: no output.

- [ ] **Step 4: Build & test**

```bash
pio test -e native && pio run -e ramps
```

- [ ] **Step 5: Commit**

```bash
git add -A && git commit -m "refactor(ports): move PortVocabulary into ports/"
```

---

## Phase 4: Move the Meade protocol into `adapters/meade/`

The whole `src/core/meade/` subtree becomes the driving adapter. Namespace `oat::core::meade` → `adapters::meade`. The parser stays pure (no Arduino), so this is a directory move + namespace + include-path rewrite. No wire-byte change.

**Files:**
- Move (21): `src/core/meade/*` → `src/adapters/meade/*`
- Move (18 test files): `unit_tests/test_core/meade/*` → `unit_tests/test_adapters/meade/*`
- Move (1 fake): `unit_tests/test_common/meade_fakes/StubMeadeHandlers.hpp` → `unit_tests/test_common/meade_fakes/StubMeadeHandlers.hpp` (path unchanged, contents updated)
- Modify: `src/adapters/MeadeCommandAdapter.{hpp,cpp}`, `src/adapters/SerialTransport.hpp`, `src/app/MountExecutor.cpp`, `src/app/MountReadout.cpp`, `unit_tests/test_adapters/test_meade_pipeline.cpp`, `unit_tests/test_common/meade_fakes/StubMeadeHandlers.hpp`

- [ ] **Step 1: Move the protocol files**

```bash
mkdir -p src/adapters/meade
git mv src/core/meade/CommandAssembler.hpp          src/adapters/meade/CommandAssembler.hpp
git mv src/core/meade/MeadeParser.hpp               src/adapters/meade/MeadeParser.hpp
git mv src/core/meade/MeadeParser.cpp               src/adapters/meade/MeadeParser.cpp
git mv src/core/meade/MeadeProtocol.hpp             src/adapters/meade/MeadeProtocol.hpp
git mv src/core/meade/MeadeParserHelpers.hpp        src/adapters/meade/MeadeParserHelpers.hpp
git mv src/core/meade/MeadeParserHelpers.cpp        src/adapters/meade/MeadeParserHelpers.cpp
git mv src/core/meade/MountStatusReport.hpp         src/adapters/meade/MountStatusReport.hpp
git mv src/core/meade/MountStatusReport.cpp         src/adapters/meade/MountStatusReport.cpp
git mv src/core/meade/PendingReplies.hpp            src/adapters/meade/PendingReplies.hpp
for f in Distance Extra Focus Get Gps Home Init Movement Quit Set SlewRate SyncControl; do
  git mv "src/core/meade/MeadeParser${f}.cpp" "src/adapters/meade/MeadeParser${f}.cpp"
done
rmdir src/core/meade
```

- [ ] **Step 2: Rename the namespace inside the moved files**

Every meade file opens `namespace oat` → `namespace core` → `namespace meade` (three lines, column 1) and closes with the matching three `}  // namespace …` lines. `MeadeProtocol.hpp` has no namespace and is unaffected. Collapse the three into `adapters` + `meade` with one slurped (`-0777`) substitution that rewrites both the opening and the closing lines in one pass:

```bash
perl -0777 -pi -e 's/namespace oat\nnamespace core\nnamespace meade/namespace adapters\nnamespace meade/g; s/\}  \/\/ namespace meade\n\}  \/\/ namespace core\n\}  \/\/ namespace oat/}  \/\/ namespace meade\n}  \/\/ namespace adapters/g' src/adapters/meade/*.hpp src/adapters/meade/*.cpp
```

Verify the result and the brace balance:

```bash
grep -rn 'namespace oat\|namespace core' src/adapters/meade/ || true   # expect: no output
grep -c 'namespace meade' src/adapters/meade/MeadeParser.hpp           # expect 1
grep -rn 'namespace adapters' src/adapters/meade/MeadeParser.hpp | head -2  # expect line 27
```

- [ ] **Step 3: Rewrite `oat::core::meade` / bare `core::meade` references across the tree**

```bash
perl -pi -e 's/oat::core::meade/adapters::meade/g' $(grep -rl 'oat::core::meade' src unit_tests)
perl -pi -e 's/\bcore::meade\b/adapters::meade/g' $(grep -rl '\bcore::meade\b' src unit_tests)
```

The second form catches the bare `core::meade::CommandAssembler` mention in `src/adapters/SerialTransport.hpp:18` and the stale comment in `src/app/MountReadout.cpp:293`. Also fix the alias in `src/adapters/MeadeCommandAdapter.cpp` (line ~22): `namespace meade = oat::core::meade;` becomes `namespace meade = adapters::meade;` (the first perl above already handles it).

- [ ] **Step 4: Rewrite include paths**

```bash
perl -pi -e 's#core/meade/#adapters/meade/#g' $(grep -rl 'core/meade/' src unit_tests)
```

- [ ] **Step 5: Move the protocol tests**

```bash
mkdir -p unit_tests/test_adapters/meade
git mv unit_tests/test_core/meade/*.cpp unit_tests/test_adapters/meade/
rmdir unit_tests/test_core/meade
```

`test_meade_pipeline.cpp` (already in `test_adapters/`) and `StubMeadeHandlers.hpp` are caught by Step 4's include rewrite; verify their `core/meade/` includes are gone:

```bash
grep -rn 'core/meade/' unit_tests/
```
Expected: no output.

- [ ] **Step 6: Fix `src/app/` consumers of `MountStatusReport`**

`src/app/MountExecutor.cpp` (line 30) and `src/app/MountReadout.cpp` (line 11) include `"../core/meade/MountStatusReport.hpp"`; rewrite to `"../adapters/meade/MountStatusReport.hpp"`:

```bash
perl -pi -e 's#\.\./core/meade/MountStatusReport\.hpp#../adapters/meade/MountStatusReport.hpp#g' src/app/MountExecutor.cpp src/app/MountReadout.cpp
```

- [ ] **Step 7: Verify the seam is clean**

```bash
grep -rn 'core/meade' src unit_tests || true
grep -rn 'oat::core' src unit_tests || true
```
Expected: no output from either.

- [ ] **Step 8: Build & test**

```bash
pio test -e native && pio run -e ramps
```
Expected: moved protocol tests now run under `test_adapters`; ramps `SUCCESS`.

- [ ] **Step 9: Commit**

```bash
git add -A && git commit -m "refactor(meade): move protocol out of core into adapters/meade"
```

---

## Phase 5: Re-home presentation & input logic into adapters

`InfoDisplayLayout`, `ConsoleLog` (panel pixel math + console ring) and `ButtonDebounce` (input conditioning) are not domain rules — they move into `adapters/`, namespace `adapters`.

**Files:**
- Move: `src/core/{InfoDisplayLayout,ConsoleLog,ButtonDebounce}.{hpp,cpp}` → `src/adapters/`
- Move: `unit_tests/test_core/test_info_display_layout.cpp` → `unit_tests/test_adapters/`
- Modify: `src/adapters/Ssd1306InfoDisplay.{hpp,cpp}`, `src/adapters/DebouncedButtons.{hpp,cpp}`, any test referencing them

- [ ] **Step 1: Move the files**

```bash
git mv src/core/InfoDisplayLayout.hpp src/adapters/InfoDisplayLayout.hpp
git mv src/core/InfoDisplayLayout.cpp src/adapters/InfoDisplayLayout.cpp
git mv src/core/ConsoleLog.hpp        src/adapters/ConsoleLog.hpp
git mv src/core/ConsoleLog.cpp        src/adapters/ConsoleLog.cpp
git mv src/core/ButtonDebounce.hpp    src/adapters/ButtonDebounce.hpp
git mv src/core/ButtonDebounce.cpp    src/adapters/ButtonDebounce.cpp
git mv unit_tests/test_core/test_info_display_layout.cpp unit_tests/test_adapters/test_info_display_layout.cpp
```

- [ ] **Step 2: Rename namespaces in the moved files**

```bash
perl -pi -e 's/\bnamespace core\b/namespace adapters/g; s/\bcore::/adapters::/g' src/adapters/InfoDisplayLayout.* src/adapters/ConsoleLog.* src/adapters/ButtonDebounce.*
```

- [ ] **Step 3: Rewrite consumers**

```bash
perl -pi -e 's#core/InfoDisplayLayout\.hpp#adapters/InfoDisplayLayout.hpp#g; s#core/ConsoleLog\.hpp#adapters/ConsoleLog.hpp#g; s#core/ButtonDebounce\.hpp#adapters/ButtonDebounce.hpp#g' $(grep -rl 'core/InfoDisplayLayout.hpp\|core/ConsoleLog.hpp\|core/ButtonDebounce.hpp' src unit_tests)

perl -pi -e 's/\bcore::InfoDisplayScale\b/adapters::InfoDisplayScale/g; s/\bcore::xRAPixel\b/adapters::xRAPixel/g; s/\bcore::yDECPixel\b/adapters::yDECPixel/g; s/\bcore::progressBarWidth\b/adapters::progressBarWidth/g; s/\bcore::markerRaHours\b/adapters::markerRaHours/g; s/\bcore::markerDecDegrees\b/adapters::markerDecDegrees/g' $(grep -rl 'core::InfoDisplayScale\|core::xRAPixel\|core::progressBarWidth\|core::markerRaHours\|core::markerDecDegrees' src unit_tests)

perl -pi -e 's/\bcore::ConsoleLog\b/adapters::ConsoleLog/g' $(grep -rl '\bcore::ConsoleLog\b' src unit_tests)
perl -pi -e 's/\bcore::ButtonDebounce\b/adapters::ButtonDebounce/g' $(grep -rl '\bcore::ButtonDebounce\b' src unit_tests)
```

- [ ] **Step 4: Verify**

```bash
grep -rn 'core::InfoDisplay\|core::ConsoleLog\|core::ButtonDebounce\|core/InfoDisplayLayout\|core/ConsoleLog\|core/ButtonDebounce' src unit_tests
```
Expected: no output.

- [ ] **Step 5: Build & test**

```bash
pio test -e native && pio run -e ramps
```

- [ ] **Step 6: Commit**

```bash
git add -A && git commit -m "refactor(adapters): move display/input logic out of core"
```

---

## Phase 6: Delete the `StatusFlags` indirection

`ports::IStatusFlags` + `adapters::StatusFlags` are a `core → ports → adapter → core::MountState` round-trip that exists only because the homing loop lives in `app/`. Since `app/` may depend on everything, `app::HallSensorHoming` writes `core::MountState` directly (it already has `set`/`clear`), and the port + adapter are deleted.

**Files:**
- Delete: `src/ports/IStatusFlags.hpp`, `src/adapters/StatusFlags.hpp`, `src/adapters/StatusFlags.cpp`, `unit_tests/test_adapters/test_status_flags.cpp`
- Modify: `src/app/HomingRunner.hpp`, `src/app/HomingRunner.cpp`, `src/app/SensorWiring.cpp`

- [ ] **Step 1: Delete the indirection**

```bash
git rm src/ports/IStatusFlags.hpp src/adapters/StatusFlags.hpp src/adapters/StatusFlags.cpp unit_tests/test_adapters/test_status_flags.cpp
```

- [ ] **Step 2: Point `HallSensorHoming` at `MountState`**

In `src/app/HomingRunner.hpp`, change the constructor parameter and member from `ports::IStatusFlags &` to `core::MountState &` (add `#include "../core/MountState.hpp"` if not already present; the `.cpp` already includes it). In `src/app/HomingRunner.cpp`, change `_status.set(X)` → `_state.set(X)` and `_status.clear(X)` → `_state.clear(X)` (four call sites: lines ~116, 164, 173, 174).

- [ ] **Step 3: Update `src/app/SensorWiring.cpp`**

Replace the `adapters::StatusFlags statusFlags(mountCoordinator.state());` construction and the subsequent `statusFlags` argument with `mountCoordinator.state()` passed directly, and remove the `adapters/StatusFlags.hpp` include.

- [ ] **Step 4: Verify no references remain**

```bash
grep -rn 'IStatusFlags\|StatusFlags' src unit_tests
```
Expected: no output.

- [ ] **Step 5: Build & test**

```bash
pio test -e native && pio run -e ramps
```

- [ ] **Step 6: Commit**

```bash
git add -A && git commit -m "refactor(status): delete StatusFlags adapter, homing writes MountState directly"
```

---

## Phase 7: Move the site/time write protocol into core

`adapters/SiteTimePort` implements `ports::IMountSiteTime` over a `core::SiteTime&` while owning the write protocol (re-anchor sidereal time, flip hemisphere, persist). The write protocol is business logic, so the class moves to `core/SiteTimeService` (namespace `core`, still implements `ports::IMountSiteTime`). The facade keeps owning `core::SiteTime`; the service holds a reference to it and does the writes — no behaviour change.

**Files:**
- Move: `src/adapters/SiteTimePort.{hpp,cpp}` → `src/core/SiteTimeService.{hpp,cpp}`
- Modify: `src/app/BoardWiring.hpp`, `src/app/BoardWiring.cpp`, comments in `src/app/MountSettings.hpp`, `src/app/MountExecutor.cpp`, `src/app/MountReadout.hpp`
- Create: `unit_tests/test_core/test_site_time_service.cpp`

- [ ] **Step 1: Move and rename the class**

```bash
git mv src/adapters/SiteTimePort.hpp src/core/SiteTimeService.hpp
git mv src/adapters/SiteTimePort.cpp src/core/SiteTimeService.cpp
perl -pi -e 's/SiteTimePort/SiteTimeService/g; s/\bnamespace adapters\b/namespace core/g; s/\bcore::DayTime\b/types::DayTime/g; s/\bcore::Latitude\b/types::Latitude/g; s/\bcore::Longitude\b/types::Longitude/g' src/core/SiteTimeService.hpp src/core/SiteTimeService.cpp
```

Keep `#include "core/SiteTime.hpp"` → `#include "SiteTime.hpp"` (same directory) and `#include "core/SiderealClock.hpp"` → `#include "SiderealClock.hpp"`:

```bash
perl -pi -e 's#core/SiteTime\.hpp#SiteTime.hpp#g; s#core/SiderealClock\.hpp#SiderealClock.hpp#g' src/core/SiteTimeService.hpp src/core/SiteTimeService.cpp
```

The `LatitudeObserver` struct stays nested, now `core::SiteTimeService::LatitudeObserver`. The `#include "../../Configuration.hpp"` at the top of the `.cpp` (DEBUG_LEVEL ordering fix) is no longer needed since the logger include ordering problem was in the adapter header — but keep it if the build complains.

- [ ] **Step 2: Update the composition root**

`src/app/BoardWiring.hpp`: change `#include "adapters/SiteTimePort.hpp"` → `#include "core/SiteTimeService.hpp"` and the two externs (`DRAM_ATTR adapters::SiteTimePort siteTimePort;` / `adapters::SiteTimePort siteTimePort;`) to `core::SiteTimeService`.

`src/app/BoardWiring.cpp` (lines ~293/295): change `adapters::SiteTimePort siteTimePort(...)` to `core::SiteTimeService siteTimePort(...)`, and `#include "adapters/SiteTimePort.hpp"` → `#include "core/SiteTimeService.hpp"`.

- [ ] **Step 3: Fix the three comment references**

`src/app/MountSettings.hpp`, `src/app/MountExecutor.cpp` (~line 73), `src/app/MountReadout.hpp` (~line 48) mention `SiteTimePort::LatitudeObserver` in comments; update to `SiteTimeService::LatitudeObserver`.

- [ ] **Step 4: Add a host test for the write protocol**

Create `unit_tests/test_core/test_site_time_service.cpp` covering: (a) `setLatitude` persists via the fake `IMountSettings` and invokes the observer only when the sign changes; (b) `setLongitude` re-anchors the sidereal time; (c) `setHA` produces the same LST as `core::SiderealClock::lstFromHourAngle` would. Use `fakes::FakeClock` (in `test_common/hal_fakes/FakeSystemClock.hpp`) and `fakes::FakeMountSettings` (in `test_common/ports_fakes/FakeMountSettings.hpp`), plus an inline null logger.

```cpp
#include "core/SiteTimeService.hpp"
#include "core/SiderealClock.hpp"
#include <gtest/gtest.h>

#include "../test_common/hal_fakes/FakeSystemClock.hpp"   // fakes::FakeClock
#include "../test_common/ports_fakes/FakeMountSettings.hpp" // fakes::FakeMountSettings

namespace
{
/// Minimal ILogger sink for the test — avoids pulling in adapters::NullLogger.
class NullLogger final : public ports::ILogger
{
  public:
    bool isEnabled(uint16_t) const override
    {
        return false;
    }
    void log(uint16_t, ports::LogFormat, ...) override
    {
    }
};

struct ObserverCall {
    bool called   = false;
    bool northern = false;
};
void recordObserver(void *context, bool northern)
{
    auto *c      = static_cast<ObserverCall *>(context);
    c->called    = true;
    c->northern  = northern;
}
}  // namespace

TEST(SiteTimeServiceTest, SetLatitudePersistsAndFlipsHemisphereOnSignChange)
{
    core::SiteTime siteTime;
    fakes::FakeClock clock;
    fakes::FakeMountSettings store;
    NullLogger logger;
    ObserverCall obs;
    core::SiteTimeService svc(siteTime, clock, store, logger, {&obs, recordObserver});

    svc.setLatitude(types::Latitude(52.0f));
    EXPECT_TRUE(store.hasLatitude());             // persisted
    EXPECT_TRUE(obs.called);
    EXPECT_TRUE(obs.northern);

    obs.called = false;
    svc.setLatitude(types::Latitude(-30.0f));
    EXPECT_TRUE(obs.called);
    EXPECT_FALSE(obs.northern);
}
```

> If `FakeMountSettings` has no `hasLatitude()` accessor, assert on `store.storedLatitude()` (or whatever read-back accessor exists) instead — check `unit_tests/test_common/ports_fakes/FakeMountSettings.hpp`.

- [ ] **Step 5: Build & test**

```bash
pio test -e native && pio run -e ramps
```
Expected: the new `test_site_time_service` passes alongside the existing suites.

- [ ] **Step 6: Commit**

```bash
git add -A && git commit -m "refactor(sitetime): move write protocol into core::SiteTimeService"
```

---

## Phase 8: Update ARCHITECTURE.md + full gates

**Files:** `ARCHITECTURE.md`

- [ ] **Step 1: Update the layer table (§3)**
  - Add a `types/` row: dependency-free value vocabulary, "may be depended on by everything", contains `DayTime`, `Declination`, `Latitude`, `Longitude`, `RightAscension`, `MathUtil`, `Button`, `FocuserDirection`, `MountEvent`.
  - `core/` row: remove `MeadeParser`, `MountStatusReport`, `PendingReplies`, `CommandAssembler`, `InfoDisplayLayout`, `ConsoleLog`, `ButtonDebounce`, `MountEvent`; add `SiteTimeService`.
  - `adapters/` row: add the Meade protocol driver (`adapters/meade/`) and `InfoDisplayLayout`, `ConsoleLog`, `ButtonDebounce`.
  - `ports/` row: add `IEventSink`; note `PortVocabulary` now lives here.

- [ ] **Step 2: Update the dependency-rule text (§2.1)**

Change "adapters may depend on `ports/` and `hal/`" → "adapters may depend on `types/`, `ports/`, and `hal/`", and "core/ may depend only on ports/" → "core/ may depend only on `types/` and `ports/`".

- [ ] **Step 3: Reverse the two wire-format statements (§7, §8)**

  - §7 flow diagram: move `MeadeParser` inside the adapter box (transport → parser → command adapter → ports → core).
  - §8: "a wire format (belongs in `core/meade/`)" → "a wire format belongs in `adapters/meade/`". The "**A Meade command**" row: `core/meade/MeadeParser*.cpp` → `adapters/meade/MeadeParser*.cpp`.

- [ ] **Step 4: Full matrix build**

```bash
python matrix_build.py
```
Expected: all board/env combinations green.

- [ ] **Step 5: Format check**

```bash
bash -c 'shopt -s nullglob globstar;GLOBIGNORE=./src/libs/TimerInterrupt/*; for i in ./{.,src/**,unit_tests,boards/**}/*.{c,cpp,h,hpp}; do clang-format -i $i; done' && git diff --check
```
Expected: `git diff --check` reports no whitespace errors; no unintended formatting churn outside the moved files.

- [ ] **Step 6: Final test + commit**

```bash
pio test -e native && git add -A && git commit -m "docs(arch): reflect types/ layer and adapters/meade protocol driver"
```

---

## Self-review

**Spec coverage** — the two directives map to phases: "adapters ≠ core" is satisfied by Phases 1–3 (vocabulary/events/PortVocabulary leave core) + 5 (presentation leaves core) + 6–7 (delete the two state round-trips); "core = business logic, not protocol" by Phase 4 (Meade → adapters) + Phase 8 (docs). ✅

**Placeholder scan** — the only deliberately deferred item is the `PortVocabulary` `#define`→`enum class` conversion (noted in Phase 3 as out-of-scope hygiene, not a placeholder in a task). The test fake names in Phase 7 Step 4 are flagged to match the real fakes. ✅

**Type consistency** — `types::` namespace introduced in Phase 1 is used consistently in Phases 2–7; `ports::IEventSink` is defined in Phase 2 before Phase 4's `PendingReplies` references it; `adapters::meade` is created in Phase 4 before Phase 8's doc references. ✅

**Known follow-ups (explicitly out of scope here):**
- Convert `ports/PortVocabulary.hpp` `#define`s to `enum class`/`constexpr`.
- `hal/` still includes `ports/ILogger.hpp` (`InterruptCallback.cpp`, `StepperMotorFactory.cpp`) — a separate dependency-rule fix.
- `MountExecutor` facade continues to shrink toward the coordinator (separate migration work).
