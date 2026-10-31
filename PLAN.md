# PLAN: Five Refactorings to Remove Overengineering and Sloppiness

> **For agentic workers:** each item is independently shippable. Work them in the order given
> (cheap/safe first), keep every step compiling, and use `- [ ]` checkboxes for tracking.
> Do not batch two items into one changeset. Do not commit after you are done with an item.
> User will create the commit manually.

**Goal:** delete duplicated, dead and mechanically-repeated code so the tree has one copy of each
fact. Nothing here changes behaviour: no wire format, no domain rule, no pin mapping.

**Scope of this plan:** the five highest-value simplifications found in the current `refactoring`
branch, plus a short list of one-hour quick wins and an explicit list of things *not* worth doing.

---

## How the backlog was produced

1. Layer census and file-size ranking over `src/` (excluding `src/libs/`).
2. Preprocessor-conditional density per file (a direct measure of configuration sloppiness).
3. Duplicate-definition and dead-symbol searches (same name defined twice, zero-caller symbols).
4. GitNexus `impact` (upstream, per symbol) and `check` (import cycles) to rank by blast radius.

Measured baseline (this commit, branch `refactoring`, index `91865b1`):

| Layer | Files | LOC | Notes |
|---|---:|---:|---|
| `src/types/` | 14 | 910 | dependency-free vocabulary |
| `src/core/` | 57 | 4 557 | host-tested |
| `src/ports/` | 25 | 1 489 | |
| `src/hal/` | 63 | 6 030 | |
| `src/adapters/` | 76 | 11 384 | |
| `src/app/` | 69 | 12 568 | largest layer, **zero host tests** |

* Host tests: `test_core` 44 files, `test_adapters` 32, `test_ui` 11, `test_hal` 2, `test_common` 1.
  There is **no `test_app`**, and `[env:native]`'s `build_src_filter` does not compile `src/app/*.cpp`
  at all — only `src/app/ui/`. Every refactor below that touches `src/app/` is therefore guarded by
  *board builds only*. That is the single biggest constraint in this plan.
* Import cycles: **0** (`gitnexus check` → `enumeration: complete, cycleCount: 0`).
* `src/app/main.cpp`: 544 lines, **170 preprocessor directives** (31% of the file).
* `src/app/DriverWiring.cpp`: 574 lines, **457 of them** are ten near-identical `build*Driver()` bodies.
* `src/adapters/meade/MeadeProtocol.hpp`: 1 205 lines, **zero non-comment lines**, zero `#include`rs.
* `ports::IMountSettings` (41 methods) is implemented **three times**.

## Ranked backlog

| # | Refactoring | Saves (est.) | Blast radius | Risk |
|---|---|---|---|---|
| R1 | Empty `main.cpp::setup()` into the wiring modules | ~250 lines, 170 → ~60 `#if` | 1 file + 4 modules | MEDIUM |
| R2 | Collapse the ten `build*Driver()` bodies to five | ~230–370 lines | `DriverWiring.cpp` only | LOW |
| R3 | Replace header-defined globals with a real composition root | 625-line header, 38 `extern` | 8 files, 1 TU includes it | MEDIUM |
| R4 | Delete the four "overlay" shadow types | 4 classes, 4 vtables | 24 impacted / 7 direct | MEDIUM |
| R5 | Delete the `MountSettingsStore` pass-through | 232 lines, 1 layer | 4 files | LOW |

Suggested order: **quick wins → R5 → R4 → R2 → R1 → R3**.

---

## Quick wins (do these first, ~1 hour total)

- [x] **Delete `src/adapters/meade/MeadeProtocol.hpp`.**
      1 205 lines, `grep -vcE '^\s*(//|$)'` returns **0**: the file is pure comment. Nothing
      `#include`s it — only comments reference it by name. Move it to `docs/MEADE-PROTOCOL.md`
      (it *is* the protocol reference, and it is what the wiki generator parses) and delete the
      header.
- [x] **Fix `scripts/MeadeCommandParser.py`.** It reads
      `../src/core/meade/MeadeProtocol.hpp`, a path that has not existed since the Meade protocol
      moved to `adapters/meade/`. Point it at `MODULE_PATH`-relative `../docs/MEADE-PROTOCOL.md`
      so it no longer depends on the caller's cwd.
- [x] **Delete the dead `RAString`/`DECString` pair.** `MountReadout::RAString/DECString`
      ([MountReadout.hpp:80](/Users/andre/repos/OpenAstroTech/OpenAstroTracker-Firmware/src/app/MountReadout.hpp:80))
      and their `MountExecutor` forwarders
      ([MountExecutor.hpp:328](/Users/andre/repos/OpenAstroTech/OpenAstroTracker-Firmware/src/app/MountExecutor.hpp:328))
      have **no callers**; the `const char *raText()/decText()` pair superseded them.
      *(Correction found while implementing: only the two `MountExecutor` forwarders were dead.
      `MountReadout::'s` pair still had three internal callers — `raText`/`decText` wrapped it, and
      `fillInfoDisplayState` copied its `String` to guard against `scratchBuffer` aliasing. Done as
      the same deletion with the formatting bodies moved into `raText`/`decText`, which removes two
      `String` heap allocations per panel refresh.)*
- [x] **Fix `CLAUDE.md`.** It claims `src/app/main.cpp` defines `setup()` **and** `loop()`.
      `loop()` actually lives in `src/app/MenuHost.hpp` (lines 138 and 620). Correct the doc now;
      R3 moves both into `main.cpp` for real.
- [~] **Triage the 18 commented-out code lines and 6 `TODO`/`FIXME`/`HACK` markers** in `src/`
      (excluding `src/libs/`): delete or convert to an issue. They are noise at every re-read.
      *(Commented-out code deleted: 13 lines — the `//LOG(...)` scaffolding in `MountExecutorSlew`,
      `MountExecutorTick` and `MountReadout`, the duplicated `return` in `EepromSettingsFormat`,
      and the dead `focus()->setSpeed()` call. The 5 `TODO`s were left in place by decision: all
      five are genuine open questions (slew-mode check in `setSpeed`, guide steps vs `GoHome`, DEC
      guide pulses, Southern-hemisphere direction), not stale notes.)*

Verify: `pio run -e ramps` + `pio test -e native` still green.

---

## R1 — Empty `main.cpp::setup()` into the wiring modules

**Why (measured).** `setup()` is one function spanning lines 38–544 (507 lines) and containing
**159 of the file's 170 preprocessor directives**.
Lines [171–311](/Users/andre/repos/OpenAstroTech/OpenAstroTracker-Firmware/src/app/main.cpp:171)
are five near-identical axis-init blocks (RA, DEC, AZ, ALT, FOCUS), each of the shape:

```cpp
int xxLine = addConsoleText(F("Init XX axis..."));
pinMode(XX_EN_PIN, OUTPUT);
digitalWrite(XX_EN_PIN, HIGH);
#if XX_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART
    #if defined(XX_DIAG_PIN)      pinMode(XX_DIAG_PIN, INPUT);            #endif
    #ifdef XX_SERIAL_PORT
        #if defined(ARDUINO_ARCH_RP2040) XX_SERIAL_PORT.begin(115200);    #else XX_SERIAL_PORT.begin(57600); #endif
    #endif
#endif
updateConsoleText(xxLine, F("Init XX axis... OK"));
```

The five copy-paste sites differ only in the axis prefix — the platform branch
(`115200` vs `57600`) and the DIAG/SERIAL guards are repeated verbatim five times. Lines
[507–513](/Users/andre/repos/OpenAstroTech/OpenAstroTracker-Firmware/src/app/main.cpp:507)
repeat the same `#if RA_DRIVER_TYPE/DEC_DRIVER_TYPE` test a second time.

`app/` already has the right homes: `MotorWiring.{hpp,cpp}`, `DriverWiring.{hpp,cpp}`,
`SensorWiring.{hpp,cpp}`, `BoardWiring.{hpp,cpp}`. `setup()` still does board pin work that
belongs in them.

**Target shape.**

* `DriverWiring`: add `void initDriverPins(AxisPins, uint8_t driverType)` — one function, five
  call sites with literals, one place holding the RP2040 baud branch.
  Prefer passing a small `struct AxisPins { pin en; pin diag; Stream *serial; uint32_t baud; }`
  built by the board header, so the `#if` collapses into initialisation data.
* `SensorWiring`: `initHomingSensorPins()` and `initEndSwitchPins()`.
* `ConsoleLog`/`MenuSurface`: the splash, the "hold DOWN to clear EEPROM" sequence, and the
  `menu.addItem(...)` registration list (currently ~40 lines of `#if` inside `setup()`).
* `setup()` keeps only: debug-stub init, `initPersistentStore()`, `attachSteppers/attachDrivers/
  attachSensors`, the LCD startup, and the final "boot complete" flag.

**Steps.**

- [x] Move the five axis-init blocks into `DriverWiring::initDriverPins()`; replace each block with
      one call. Build `ramps` + `esp32` + `rp2040_skr_pico` — the RP2040 baud branch is the one
      behaviour that differs per platform.
      *(Done as `initDriverPins(DriverAxis)` over a `AxisPinPlan` descriptor, so `main.cpp` keeps no
      pin `#if` at all. The enum values are named `RightAscension`/`Declination`/… rather than
      `RA`/`DEC` because `Print.h` defines `DEC` as a macro. The UART `begin()` goes through one
      templated `beginDriverUart()` — the concrete type differs per platform (`HardwareSerial` on
      AVR/ESP32, `SerialUART` on RP2040), so a `Stream *` member could not call `begin()`.
      Verified by extracting the ordered `pinMode`/`digitalWrite`/`begin` actions from the old
      blocks: 32 actions, and the new path performs the same ones in the same order per axis.
      `main.cpp` 544 -> 441 lines, its `#if|#ifdef|#elif` count 80 -> **51**, already under the
      target.)*
- [x] Move the two homing-sensor `pinMode` blocks into `SensorWiring::initHomingSensorPins()`.
- [x] Move the splash + EEPROM-clear-on-DOWN sequence into `MenuSurface` as `showSplash()` and
      `awaitFactoryResetRequest()`. The latter reports the answer; `mount.clearConfiguration()`
      stays in `setup()` because the store is the mount's.
- [x] Move the `menu.addItem(...)` ladder into `UiScreens::registerMenuItems()` (next to the
      `extern` singletons it registers). This is safe from the layering checker:
      `scripts/check_architecture.py` scans `src/core`, `src/ports`, `src/hal` and `src/app/ui`
      only, so `src/app/UiScreens.cpp` may keep the feature `#if`s — but do not move any of it
      *into* `src/app/ui/`.
- [ ] Delete the duplicated `#if RA_DRIVER_TYPE/DEC_DRIVER_TYPE` block at 507–513.
      *(Checked: that block is `#if UART_CONNECTION_TEST_TX` calling `mount.testRA_UART_TX()` /
      `testDEC_UART_TX()`. It re-uses the same driver-type condition but does a different job, so
      deleting it would remove the UART TX bring-up test. Left alone — the premise was wrong.)*
- [x] `grep -cE '^\s*#\s*(if|ifdef|ifndef|elif)' src/app/main.cpp` lands at **42** (from 80), and
      the file at 380 lines (from 544).

**Verification.**

```shell
pio run -e ramps -e esp32 -e mksgenlv21 -e oaeboardv1 -e rp2040_skr_pico
python matrix_build.py -b ramps
pio test -e native
```

**Risk.** MEDIUM and mechanical: `src/app` is not host-compiled, so no unit test can catch a
mis-transcribed pin. Mitigation: move blocks **verbatim** (no reformatting in the same commit),
build one AVR env and one RP2040 env per step, and confirm the `Init ... OK` console lines still
appear in the same order.

---

## R2 — Collapse the ten `build*Driver()` bodies to five

**Why (measured).** `DriverWiring.cpp` is 574 lines. Ten functions occupy lines
[67–521](/Users/andre/repos/OpenAstroTech/OpenAstroTracker-Firmware/src/app/DriverWiring.cpp:67)
— two variants each of `buildRADriver`, `buildDECDriver`, `buildAZDriver`, `buildALTDriver`,
`buildFocusDriver`, selected by `#if SW_SERIAL_UART == 0/1`. Diffing the RA pair, the bodies are
**identical** except for:

| | software serial (`SW_SERIAL_UART == 1`) | hardware serial (`SW_SERIAL_UART == 0`) |
|---|---|---|
| ctor | `Tmc2209Driver(RA_SW_RX, RA_SW_TX, ...)` | `Tmc2209Driver(serial, ...)` |
| begin | `beginSerial(19200); mstep_reg_select(true); pdn_disable(true);` | `begin();` |
| register order | `semin` then `microsteps` | `microsteps` then `semin` |

Everything else — `testDriverConnection`, the `RA_EN_PIN` safety write, `toff(0)`,
`I_scale_analog(false)` under `USE_VREF == 0`, `rms_current`, `pdn_disable`, `toff(kTmc2209Toff)`,
`en_spreadCycle(RA_UART_STEALTH_MODE == 0)`, `blank_time(24)`, `fclktrim(4)`, `TCOOLTHRS`,
`semin(0)`, `SGTHRS`, and the five `OAT_LOG` diagnostics — is
copy-pasted ten times. The `semin`/`microsteps` ordering divergence is pure accident and is exactly
the kind of thing copy-paste produces.

**Target shape.**

```cpp
// One configuration sequence, one place.
void configureTmc2209(ports::IMotorDriver &driver,
                      const core::MountConfig &config,
                      Tmc2209Settings settings,   // rmsCurrent, stallValue, spreadCycle, microsteps
                      const char *axisName);

// Two transports, one line each, still chosen at compile time.
ports::IMotorDriver *makeTmc2209HardwareSerial(Stream *serial, float rsense, byte address);
ports::IMotorDriver *makeTmc2209SoftwareSerial(uint16_t rx, uint16_t tx, float rsense, byte address);
```

Then each axis is a ~10-line builder that picks a transport and calls `configureTmc2209`. Five
builders instead of ten; the `#if SW_SERIAL_UART` moves down into the transport factory, where it
belongs.

**Steps.**

- [x] Extract `configureTmc2209()` — now `buildTmc2209Axis()` — from the hardware-serial RA variant.
      A `Tmc2209Traits` struct carries the three register writes that are genuinely per-axis
      (`pdn_disable`, DEC's `iholddelay`/`TPOWERDOWN`, RA's `fclktrim`); everything else is one
      sequence.
- [x] Verify the software-serial variant produces the same register values. Done mechanically: the
      ten old bodies and the new shared sequence were extracted as ordered call lists and diffed.
      8/10 match exactly; the two software-serial axes differ only by (a) `semin` before vs after
      `microsteps` — different registers, inert — and (b) one extra `pdn_disable` that repeats the
      value the preamble already set.
- [x] Convert RA first, then DEC, AZ, ALT, FOCUS. Both `SW_SERIAL_UART` values build.
- [x] `wc -l src/app/DriverWiring.cpp` — **574 -> 340**.

*(Two things the plan missed, both found by checking rather than trusting it. First: DEC's two
variants had **already diverged** — only the hardware-serial one ever programmed
`iholddelay`/`TPOWERDOWN`, so unifying the sequence would have changed the standstill hold current
on RAMPS/MKS boards. Preserved behind `kDecStandstillPowerDown` and flagged as a follow-up rather
than silently "fixed". Second: the shared helpers are guarded by a `HAS_TMC2209_AXIS` `#if`, because
a board running A4988s throughout would otherwise define them and never call them, which `-Werror`
rejects. Transport selection collapsed to one `TMC2209_PORT(axis)` macro, so `attachDrivers()` lost
its ten `#if` blocks. Measured on `ramps`: flash 173 766 -> 173 530 bytes, RAM 3 588 -> 3 626 — the
+38 bytes are the five initialised `Tmc2209Axis` aggregates, which AVR cannot keep in flash because
they hold pointers.)*

**Verification.** `pio run -e ramps -e rp2040_skr_pico -e oaeboardv1 -e esp32` covers both
`SW_SERIAL_UART` values. `gitnexus impact(attachDrivers)` reports **1 upstream caller, LOW** — the
ten builders are file-local, so no other file can be affected.

**Risk.** LOW. The only behaviour transfer is register-write ordering, and it is inert.

---

## R3 — Replace header-defined globals with a real composition root

**Why (measured).** `src/app/MenuHost.hpp` is 625 lines that **define**, not declare:

* five global objects — `lcd_key`, `homeMenu`, `raMenu`, `decMenu`, `focusMenu`
  ([lines 33–42](/Users/andre/repos/OpenAstroTech/OpenAstroTracker-Firmware/src/app/MenuHost.hpp:33));
* five non-inline functions — `startupAwaitingHa/Roll/Pole()`, `applyStartupOutcome()`,
  `screenInput()`;
* **`void loop()`** — the Arduino sketch entry point
  ([line 138](/Users/andre/repos/OpenAstroTech/OpenAstroTracker-Firmware/src/app/MenuHost.hpp:138),
  and a second definition at line 620 for `DISPLAY_TYPE == 0`).

A header with object definitions in it can be included by exactly one translation unit; today that
is `main.cpp` alone, which works purely by convention. One future `#include` from a second file is
a duplicate-symbol link error, and there is no compiler diagnostic warning anybody.

Around it sits an `extern` object graph: 38 `extern` declarations — `BoardWiring.hpp` (23),
`UiScreens.hpp` (7), `LoopState.hpp` (3 free mutable `bool`s), and one each in `UiSession.hpp`,
`UiPorts.hpp`, `MenuSurface.hpp`, `WifiControl.hpp`, `ports/ILogger.hpp`. `CLAUDE.md` describes
`app/BoardWiring.cpp` as the composition root that "owns the object graph" — in practice nobody
*owns* it; it is ambient.

**Target shape.** Keep the global objects (AVR SRAM budget and the existing lifetime story justify
one instance each), but make them *owned and reachable* rather than *ambient*:

* Move `MenuHost.hpp` → `MenuHost.cpp` (add `MenuHost.hpp` with declarations only, or fold the
  declarations into the existing `UiScreens.hpp`/`MenuSurface.hpp` pair).
* Move the two `loop()` definitions into `main.cpp` next to `setup()`, so the sketch entry points
  live where `CLAUDE.md` already says they do. `loop()` becomes a two-line call into
  `app::menuLoop()`.
* Group `LoopState.hpp`'s three free `bool`s into one `struct LoopState` instance — they are
  always read and written together (`inStartup`, `okToUpdateMenu`, `quitSerialOnNextButtonRelease`).
* Leave the `extern` graph itself for a later pass; the win here is that no header *defines*
  anything, and the entry points are where the documentation says they are.

**Steps.**

- [x] `git mv src/app/MenuHost.hpp src/app/MenuHost.cpp`; add a declarations-only header that
      `main.cpp` includes. `SerialPump.hpp` had the same defect and had to be split with it (see
      the note below).
- [x] Rename `loop()` → `app::menuLoop()`; add `void loop()` to `main.cpp` calling it. The
      `#else` (no-display) variant is the same one call, because `menuLoop()` picks its body with
      the same `#if` the two definitions used.
- [x] Move `MenuHost.cpp`'s five object definitions next to `UiScreens.cpp`'s singletons, or into
      `MenuHost.cpp`'s anonymous namespace if they are file-local (check the `#if LCD_BUTTON_TEST`
      and `#if DISPLAY_TYPE > 0` guards first). They were file-local — nothing outside referenced
      `lcd_key`, `lastTrackingStatusPrint`, `homeMenu`, `raMenu`, `decMenu` or `focusMenu` — so they
      went into the anonymous namespace with the five helpers.
- [ ] Bundle `LoopState.hpp`'s bools into `struct LoopState { bool inStartup; bool okToUpdateMenu;
      bool quitSerialOnNextButtonRelease; }` and update the ~handful of readers.
- [x] Confirm no header in `src/app/` contains a top-level object definition:
      `grep -nE "^[A-Za-z_][A-Za-z0-9_:<>* ]+ [A-Za-z_][A-Za-z0-9_]*;" src/app/*.hpp` should return
      only `extern` lines.

*(`SerialPump.hpp` was the second define-everything header, and R3 could not land without splitting
it: it defined `serialLoop()`, `serialEvent()` and `processSerialData()`, so the moment `MenuHost.cpp`
and `main.cpp` both included it the link failed with four duplicate symbols. `main.cpp` also lost the
transitive includes the old header was quietly supplying — `UiScreens.hpp` for `registerMenuItems`,
`MeadeCommandAdapter.hpp` for `MeadeCommandPorts` — and now names them itself, which is the whole
point of the change.)*

**Verification.** `pio run -e ramps -e esp32 -e mksgenlv21` (the display and no-display variants
differ by `DISPLAY_TYPE`, so at least one env of each is required) and
`python scripts/check_architecture.py`. Note that **no test covers `MenuHost` or `UiScreens`** —
`pio test -e native` builds only `src/app/ui/**`, so it will not catch a mistake here.

**Risk.** MEDIUM. `MenuHost` is the UI dispatch loop, and `src/app` has no host test. Mitigation:
this is a pure move — no line of logic changes. If a step needs more than a rename, split it out.

---

## R4 — Delete the four "overlay" shadow types

**Why (measured).** `src/adapters/` defines `DayTime`, `Declination`, `Latitude`, `Longitude` as
**global-namespace subclasses** of the `types::` values:

```cpp
class DayTime : public types::DayTime { ... virtual const char *ToString() const; ... };
```

Consequences, all measured:

* **Two identities per concept.** The four concepts are referenced **303 times unqualified**
  (through the shadow classes) and **355 times** as `types::X` — the tree cannot decide which name
  is the type. `MountSettingsStore::storeHATime` calls `_store.storeHATime(DayTime(ha))` where
  `ha` is already a `types::DayTime` and the target parameter is `const types::DayTime &` — a
  round-trip through the subclass that does nothing
  ([MountSettingsStore.hpp:28](/Users/andre/repos/OpenAstroTech/OpenAstroTracker-Firmware/src/adapters/MountSettingsStore.hpp:28)).
* **Pointless virtuals.** `virtual const char *ToString()` in `DayTime`, `Latitude`, `Longitude`,
  and `virtual ... formatString()` in `Longitude`. Nothing derives from these classes. On AVR each
  is a vtable slot plus a vptr in every instance, for zero dispatch.
* **Untestable by construction.** `platformio.ini`'s `[env:native]` excludes all four `.cpp` files
  ("The four type overlays are `String`-based wire/display formatting"), so the 332 lines that
  parse and format the Meade wire strings are the only value-formatting code in the tree with
  **no host test**. The exclusion reason is `String` — an output parameter does not need it.
* **API duplication.** `MountReadout` carries both `String RAString(byte, byte)` and
  `const char *raText(uint8_t, uint8_t)` for the same rendering (see the quick win above).

**Target shape.**

```cpp
// src/adapters/DayTimeText.hpp
namespace adapters {
bool parseMeadeDayTime(const char *s, types::DayTime &out);          // "-45*32:11" / "23:44:22"
void formatDayTime(char *buf, size_t size, const types::DayTime &t); // "14:45:06"
void formatDeclination(char *buf, size_t size, const types::Declination &d, bool northern);
}
```

Free functions, no inheritance, no vtable, `char *` out-parameters. Any call site that genuinely
has an Arduino `String` keeps a one-line `String` overload in the same header that forwards with
`.c_str()`.

**Steps.**

- [x] Add `src/adapters/DayTimeText.{hpp,cpp}` with the free functions. The bodies moved verbatim,
      with two corrections the `String` removal needed: `snprintf("%.Nf")` in place of
      `String(float, N)`, and a 48-byte buffer because the old 32 truncated
      `-180:00:00 (-180.0000, -180.0000)`.
      *(The two Meade parsers and `Longitude::formatStringForMeade` were **not** carried over: they
      had no callers, so re-adding them would have recreated dead code. `Declination::FromSeconds`
      and `fromCelestialSeconds` became `decFromCelestialArcSeconds` / `decFromCelestialSeconds`.)*
- [x] Convert the call sites that parse/format for the wire: `app/MountReadout.cpp`,
      `app/MountExecutorConfig.cpp`, `adapters/MeadeCommandAdapter.cpp`. `MountExecutorSlew.cpp`
      only needed its `DayTime`/`Declination` spellings qualified; `MeadeParserSet.cpp` needed
      nothing (its mention was a comment).
- [x] Delete the four overlay headers/sources and their `String` constructors.
- [x] Remove the four `-<adapters/...>` exclusions from `[env:native]`'s `build_src_filter`.
- [x] Add `unit_tests/test_adapters/test_DayTimeText.cpp` (9 cases). This is the test that could
      not exist today.
- [x] Drop the redundant `DayTime(ha)` wrap — R5 deleted the class it lived in.

*(Measured on `ramps`: flash 175 968 -> 173 766 bytes, RAM 3 760 -> 3 588. Output was verified
**byte-identical** to the deleted overlays by compiling the old classes from `HEAD` against a
minimal `String` shim and diffing: 8/8 strings matched. Two pre-existing oddities are now pinned by
tests rather than fixed — `formatLatitude` emits a double sign (`++45:00:00`) and `%{d}` emits one
for declination. Both only reach DEBUG logs.)*

**Verification.** `pio test -e native` **now covers the formatters** — that is the point.
`pio run -e ramps -e esp32`, `python scripts/check_architecture.py` (these files live in
`adapters/`, which is not #if-scanned, but `core/` must still not see them).

**Risk.** MEDIUM. `gitnexus impact(DayTime, Class)` = 24 impacted / 7 direct, **lower-bound** with 5
interface-dispatch boundaries — so treat 7 as a floor and grep the shadow name before deleting each
class. Do one type at a time, smallest first (`DayTime`: 2 methods; then `Latitude`, `Longitude`,
`Declination`), and build after each.

---

## R5 — Delete the `MountSettingsStore` pass-through

**Why (measured).** The settings accessor list exists **three times**:

| Layer | Location | Size |
|---|---|---|
| port | `ports::IMountSettings` | 41 pure virtuals |
| format | `adapters::EepromSettingsFormat` | 40 `getX`/`storeX` pairs |
| store | `adapters::MountSettingsStore` | **41 one-line forwards, 232 lines** |

`MountSettingsStore` adds no behaviour: every method is `return _store.getX();` /
`_store.storeX(v);`. Its own header admits it ("every method a single forward"), justified by "an
optimising build collapses it". What the justification misses is the maintenance cost: adding one
setting means editing three files (and their tests), and the third copy can drift out of sync with
the port without any error until someone calls the missing method.

It is also wired from exactly one place — `BoardWiring.cpp:144` — and referenced by no test
(`test_EepromSettingsFormat.cpp` drives `EepromSettingsFormat` directly).

**Target shape.** `EepromSettingsFormat` implements `ports::IMountSettings` directly:

```cpp
class EepromSettingsFormat final : public ports::IMountSettings
```

The class already has every matching signature returning `types::` values
(`types::DayTime getHATime()`, `types::Latitude getLatitude()`, …) and its own extra API
(`initialize()`, `commit()`, `displayContents()`, `isPresent()`, …) is unaffected. `Mount` keeps
taking `ports::IMountSettings&`, so testability is unchanged — a fake port still substitutes.

**Steps.**

- [x] Make `EepromSettingsFormat final : public ports::IMountSettings`; add `override` to the 41
      matching methods. The compiler verifies the list is complete — this is the whole point.
- [x] Delete `src/adapters/MountSettingsStore.hpp`.
- [x] `BoardWiring.{hpp,cpp}`: `extern adapters::MountSettingsStore settingsStore;` →
      pass `eepromStore` where `settingsStore` was passed.
- [x] Update `src/adapters/README.md`'s table row and the "exception" paragraph.
- [x] Grep for any remaining `MountSettingsStore` reference (there are 4: the two `BoardWiring`
      files, the README, and the header itself).

*(Measured on `ramps`: flash 178 676 → 175 968 bytes, RAM 3 762 → 3 760. The deleted header's
claim that "an optimising build collapses it" was false — −2 708 bytes. Also retires the
`DayTime(ha)` re-wrap that the store did on `storeHATime`.)*

**Verification.** `pio test -e native` (the settings tests already exist and are the guard),
`pio run -e ramps -e esp32`, `python scripts/check_architecture.py`.

**Risk.** LOW. 41 methods, one implementation, one wire site, and a compile error if anything is
missed.

---

## Explicit non-goals (and why)

These looked like candidates and are deliberately **not** in this plan. Recording them stops the
next pass from re-litigating them.

* **The twelve `IMeade*Handlers` interfaces in `adapters/meade/MeadeParser.hpp`**
  (115 pure virtuals; five interfaces with exactly one method each, e.g. `IMeadeGpsHandlers`).
  `gitnexus impact(IMeadeHandlers)` = **23 impacted / 17 direct / HIGH**, with 25 dispatch
  boundaries, and the unit tests depend on the split: `test_MeadeGet.cpp` implements only
  `IMeadeGetHandlers`, `test_MeadeExtra.cpp` only `IMeadeExtraHandlers`. Collapsing them would
  force every fake to implement 115 methods and buy nothing measurable. Leave it.
* **The `MountExecutor` god object** (563-line header, **118 public members**, **38 includes**,
  105 method definitions across 8 `.cpp` files, 19 of them pure one-line forwarders;
  `gitnexus impact` = 21 impacted / 13 direct / MEDIUM). This is the largest real target in the
  tree, but `src/app` has **no host test** and is not compiled by `[env:native]`. Doing it first
  means a refactor with no safety net. Prerequisite: add a `unit_tests/test_app` with a fake
  `MountDeps` and put `src/app` into the native `build_src_filter`. That is a plan of its own.
* **`adapters/MeadeCommandAdapter.cpp`** (1 164 lines) is excluded from `[env:native]` and therefore
  untested, even though the parser it drives is well tested. Worth fixing, but it needs a fake
  `ITransport` and a host-buildable `String`, i.e. more work than the refactor itself.
* **`EepromSettingsFormat`'s 40 `getX`/`storeX` pairs** (890 lines). Flat but honest — each pair is
  real serialisation against a real EEPROM layout. R5 removes the redundant *third* copy; the
  format itself should not be table-driven into something less readable.
* **The 38 `extern` declarations.** R3 fixes the parts that are actively dangerous (definitions in
  headers, entry points in the wrong file). Turning the whole graph into an injected `AppContext`
  is a large, low-urgency change; revisit after a `test_app` exists.
* **`scripts/check_architecture.py`'s rules.** They are correct and cheap; they are what keeps
  `core/` and `app/ui` honest. Do not relax a rule to make a refactor easier.

---

## Verification protocol (every item)

```shell
# 1. Host tests + the architecture rules — run first, they are seconds.
pio test -e native
python scripts/check_architecture.py

# 2. Every board still builds (this is the only guard on src/app/).
pio run -e ramps -e esp32 -e mksgenlv21 -e mksgenlv2 -e mksgenlv1 -e oaeboardv1 -e rp2040_jackw01 -e rp2040_skr_pico

# 3. Configuration matrix for anything touching wiring or drivers.
python matrix_build.py -b ramps

# 4. Formatter (v12; CI fails on drift).
bash -c 'shopt -s nullglob globstar;GLOBIGNORE=./src/libs/TimerInterrupt/*; for i in ./{.,src/**,unit_tests,boards/**}/*.{c,cpp,h,hpp}; do clang-format -i $i; done'

# 5. Scope check before committing.
gitnexus detect_changes(scope: "all", repo: "OpenAstroTracker-Firmware")
```

Rules for every item in this plan:

1. **One item per commit.** A move commit changes no logic; a logic commit changes no moves.
2. **Never silently reorder register writes or pin writes** — R2/R1 both risk it. If order changes,
   say so in the commit body.
3. **`detect_changes` must list only the files the item names.** `partial: true` or
   `truncated: true` means re-run, not "close enough".
4. **`core/` and `ports/` gain no `#if` and no new include of `hal/`/`adapters/`** — R4 is the one
   item that could tempt this; it must not.
