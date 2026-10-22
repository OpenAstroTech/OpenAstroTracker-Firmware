# On-hardware test plan

The refactor in [PLAN.md](PLAN.md) replaced the inside of the firmware without changing what
the mount does. The host suite (`pio test -e native`, 527 cases) covers the extracted logic,
and the CI matrix covers compilation for every board — but nothing in either runs on a
stepper driver, a hall sensor or a real serial peer. This document is the manual suite that
closes that gap.

**Run this before merging the refactor branch, and again before a release that contains it.**
Every case below names the refactor work it guards, so a failure points at a change rather
than at "the mount".

---

## 1. Prerequisites

### 1.1 Builds to have on hand

| Board | Why |
|---|---|
| RAMPS 1.4 + Mega2560 (`ramps`) | the widest AVR shape: LCD keypad, TMC2209 UART, AZ/ALT/Focus, hall sensors |
| MKS Gen L v2.1 (`mksgenlv21`) | second AVR shape, different pin map |
| ESP32 (`esp32`) | the FreeRTOS stepper-task path, Wi-Fi transport |
| RP2040 (`rp2040_skr_pico`) | the PIO/timer path |

Build with `pio run -e <env> -t upload`. AVR builds are **headless by design** (§11.6) —
no LCD, no info display. ESP32 and RP2040 keep theirs, so run §9.4/§9.5 on those.

### 1.2 Rig

- Mount with RA and DEC motors, drivers powered from the mount's supply, **not** USB.
- A serial terminal at 19200 8N1 (the firmware's monitor baud), or the OAT ASCOM driver.
- For guiding cases: ST-4 or pulse-guide source.
- For homing cases: the hall sensors fitted and reachable.
- For park cases: a known mechanical park position, and enough clearance for a full RA/DEC
  travel. **Remove the telescope or fit a dummy payload** — a park or home slew runs to the
  mechanical end stops.

### 1.3 Safety

- Keep a hand on the power switch for §5 (park) and §6 (homing). A homing failure drives an
  axis into its stop.
- Do not run §5 or §6 with the OTA fitted.

### 1.4 Recording

Fill in §10 as you go. A case is only "pass" when the *expected* column matches, not when
"nothing smoked". Log the firmware version (`:GVN#`) with each run.

---

## 2. Boot, storage and identity

| # | Case | Steps | Expected | Guards |
|---|---|---|---|---|
| 2.1 | Cold boot, blank EEPROM | Erase EEPROM (`:XFR#` or a fresh board), power up | Boots, reports the firmware version, RA/DEC steps-per-degree default to the `Configuration*.hpp` values, tracking starts if configured | `MountConfig` defaults (measured, not landed — still compile-time), `EPROMStore` magic markers |
| 2.2 | Persistence round-trip | Change latitude, steps-per-degree, backlash steps, park position; power cycle | Every value comes back identical; no "different firmware version" surprise | `adapters::MountSettingsStore`, `core::MountState` reset |
| 2.3 | Firmware-version change | Flash a build with a different version, boot | Coordinates are reset (not garbage), a log line says the version changed | `readPersistentData()` |
| 2.4 | `:GVN#` / `:GVP#` | Send both | Version string and product name, no leading garbage | `IMeadeHandlers` → `MeadeCommandAdapter` |
| 2.5 | Status word `:GX#` | Send while idle, tracking, slewing, parking | Each bit matches the physical state; no stale `Slewing`/`Parking` bit after the move ends | `core::MountState`, `applyParkingStatus()`, `EventBus` |
| 2.6 | `:GU#` (if present) | Send | Well-formed status line for a host driver | `core::MountState` bit layout (wire format — a reshuffle here is silent otherwise) |

---

## 3. Manual slewing and speed

| # | Case | Steps | Expected | Guards |
|---|---|---|---|---|
| 3.1 | All eight directions | N, S, E, W, NE, NW, SE, SW at rate 4 | Correct direction, no axis swapped, speed matches the rate | `MountRequests` admission, `startSlewing()` |
| 3.2 | Four speeds | `:XSMn#` (n=1..4) then a direction | Perceptibly different speeds, monotonic | `core::SlewController`, tracking-rate math |
| 3.3 | Stop on all axes | Direction, then `:Q#` | Axis stops within a fraction of a second; no runaway | `stopSlewing()` |
| 3.4 | Direction reversal | W then E without stopping | Reverses cleanly; no lost steps or grinding | Backlash handling |
| 3.5 | Backlash correction | Enable backlash steps, slew W→E, inspect | The configured correction is applied once, in the expected direction | `core::SlewController::backlashOwed()` |
| 3.6 | Rate persisted | Set rate, power cycle, move | Same speed after boot | `MountSettingsStore` |

---

## 4. Go-to, tracking and guiding

| # | Case | Steps | Expected | Guards |
|---|---|---|---|---|
| 4.1 | Tracking start/stop | `:St#`? (tracking on), then stop | Sidereal motion at 1×; stops within a second | `core::TrackingController` |
| 4.2 | Tracking calibration | Set a calibration factor, run 10 min | Motion matches the factor; `:GT#`/`:Gt#` consistent | `TrackingController` calibration |
| 4.3 | Go-to RA only | `:Sr#`, `:MS#`, poll `:D#` | Slews, arrives, `:D#` returns "not slewing"; RA target reached within a few arcminutes | `core::SlewCompletion`, `CompletionPlan` |
| 4.4 | Go-to DEC only | `:Sd#`, `:MS#`, poll | As 4.3 for DEC | same |
| 4.5 | Go-to both axes | Both set, `:MS#` | Both arrive; no axis left running | `SlewCompletion` (park/backlash legs not triggered) |
| 4.6 | Go-to across the meridian | Target on the other side | Flip happens once, no oscillation; the DEC/RA directions are the documented ones | `calculateRAandDECSteppers` (untouched, but the biggest regression risk) |
| 4.7 | Go-to cancelled mid-slew | `:MS#`, then `:Q#` | Stops immediately; tracking does **not** restart by itself | `SlewCompletion` cancelled path |
| 4.8 | Guide pulse RA | `:MGd#`-style or ST-4 pulse, 500 ms | Pulse of the right direction and length; tracking resumes afterwards | `core::GuidingController`, `MountCoordinator::tick()` early return |
| 4.9 | Guide pulse DEC | as 4.8 on DEC | same, and the DEC guide microstep rate is used | `setDecMicrosteps(DEC_GUIDE_MICROSTEPPING)`, `core::microstepRegisterValue` |
| 4.10 | Guide pulse expiry | Start a pulse, do not stop it | Stops on its own at the deadline | `GuidingController` deadlines |
| 4.11 | Guide while not tracking | Send a pulse with tracking off | Ignored, no motion | `MountRequests` (`RefusedWhileNotTracking`) |
| 4.12 | Guiding during slew | Start a go-to, then pulse | Pulse refused/ignored; no axis fight | `MountRequests`, tick order (guiding owns the tick) |

---

## 5. Parking (the case the refactor changed most)

| # | Case | Steps | Expected | Guards |
|---|---|---|---|---|
| 5.1 | **Full cycle: park → unpark → go-to → park** | `:hP#`, wait, then unpark (`:hU#`), go-to a target, `:hP#` again | Both parks complete; the mount is at the mechanical park position each time; `:GX#` shows `Parked` (and no `Parking`) afterwards | `core::ParkingController`, `SlewCompletion` park legs, `applyParkingStatus()` |
| 5.2 | Park status transitions | During 5.1 watch `:GX#` | `Parking` set during homing leg, `ParkingPos` during the position leg, `Parked` at the end, others clear | `ParkingController` phase ↔ bits |
| 5.3 | Park event count | Subscribe to the status display / log | `Parked` is published **once** per park (the refactor deliberately fixed a double publish) | `EventBus`, `SlewCompletion` |
| 5.4 | Already at park position | Park twice in a row | Second park completes immediately without a spurious move | `SlewCompletion::parkLegIsNoOp()` **post-move** evaluation |
| 5.5 | Go-to while parking | Start a park, then send `:MS#` | **Refused** (silent, as `:MS#` only acknowledges). This is the one sanctioned behaviour change of the refactor | `MountRequests` go-to-while-parking refusal |
| 5.6 | Manual slew while parking | Start a park, then a direction | Refused; the park finishes normally | `MountRequests` (`RefusedWhileParking`) |
| 5.7 | Park with tracking on | Track, then park | Tracking stops at the home leg, does not restart during the park leg, and the mount is parked | `CompletionPlan` (`stopTracking`, no `startTracking`) |
| 5.8 | Unpark | `:hU#` from parked | Mount becomes commandable; `Parked` bit clears | `ParkingController::clear()` |
| 5.9 | Power loss mid-park | Cut power during the position leg, restore | Boots into a sane state (not "parked" unless it is), can be homed | `readPersistentData()`, `ParkingController` not persisted |

---

## 6. Homing and end switches

| # | Case | Steps | Expected | Guards |
|---|---|---|---|---|
| 6.1 | RA hall homing | `:hF#` (find home) with the sensor clear | Axis moves off the sensor, searches, latches at the midpoint + offset, stops | `core::HomingController`, `HallSensorHoming` shim |
| 6.2 | RA homing from the sensor | Repeat starting with the sensor covered | Debounce path taken; same final position | `HomingController` scan edges |
| 6.3 | DEC homing | As 6.1 for DEC | same | same |
| 6.4 | Homing failure | Disconnect the sensor | Fails after the expected travel, reports failure, does not spin forever | `HomingController` reverse-pass failures, `:XGH#`/`:XGHR#` |
| 6.5 | Homing while parking | Do 5.1 and watch the home leg | The home leg uses the homing state machine, not a raw move | `SlewCompletion` + `ParkingController` |
| 6.6 | RA end switch | Drive RA into the limit | Axis stops at the switch; `:GX#` shows the limit; tracking does not resume into it | `EndSwitch`, `checkRALimit()` |
| 6.7 | DEC end switch | same for DEC | same | `EndSwitch` |

---

## 7. Optional axes (AZ/ALT and focus)

> This is the area Phase 4 changed most: these axes are now **runtime-present** — a board
> without them holds a null pointer and must behave exactly as if the code were absent.
> Test both a board **with** them and one **without**.

| # | Case | Steps | Expected | Guards |
|---|---|---|---|---|
| 7.1 | AZ/ALT move | `:MAAH#`-style relative move on each axis | Correct direction and distance; `:XGZ#`/`:XGA#` report the position | `core::AzAltController`, `ports::IStepperAxis*` presence |
| 7.2 | AZ/ALT stop | Move, then stop | Both stop; enable pins are driven high (drivers unpowered) and positions persisted | `disableAzAltMotors()` (now presence-checked) |
| 7.3 | AZ/ALT positions survive reboot | Move, power cycle | Positions restored | `storeAZPosition`/`storeALTPosition` |
| 7.4 | Board with no AZ/ALT | Build for a board without them | Boots, slews, parks normally; no phantom AZ/ALT motion; no crash | `kHasAzStepper`/`kHasAltStepper` guards, `hal/avr/InterruptStepperMotor` |
| 7.5 | Focus move | `:FPnnn#`, `:Fn#` | Moves by the requested steps; `:FB#` reports the position | `core::FocusController` |
| 7.6 | Focus continuous | `:FS#`, then `:FQ#` | Runs at the selected rate, stops on command | `FocusController` continuous mode |
| 7.7 | Focus stops on its own | Move, wait | "Stopped" transition happens once; driver disabled | `FocusUpdate::Stopped`, `disableFocusMotor()` |
| 7.8 | **Board with no focuser** | Build for a board without focus | Boots and idles with no crash, especially after several `loop()` passes | `FocusController` null-safety (a null dereference was fixed here — this is the regression test for it) |

---

## 8. Drivers and microstepping

| # | Case | Steps | Expected | Guards |
|---|---|---|---|---|
| 8.1 | TMC2209 UART connect | Boot with UART drivers | Both drivers answer; `:GIS#`/`:GIT#` report the expected state | `adapters::Tmc2209Driver`, connect checks |
| 8.2 | Microstep switching on slew | Track, then go-to, then track again | Audible/visible change in smoothness at the switch points; **no lost steps** (position after the move is correct) | `setRaMicrosteps`/`setDecMicrosteps`, `core::microstepRegisterValue` |
| 8.3 | Microstep on guide | Pulse-guide | DEC switches to the guide rate and back | same |
| 8.4 | A4988 / standalone TMC | Build with a non-UART driver | Boots; no UART error; microstepping taken from pin strapping | `#if`-free microstep path (no-op driver) |
| 8.5 | UART self-test | `:XFR#`-adjacent flow with `UART_CONNECTION_TEST_*` enabled | Motors move as the test describes | `testUART_vactual`, `setRaMicrosteps` |
| 8.6 | Driver current settings | Boot with UART, read the log | Currents match the configuration; no "driver not found" | `configure*Driver()` |

---

## 9. Transports, displays and platform specifics

| # | Case | Steps | Expected | Guards |
|---|---|---|---|---|
| 9.1 | Serial framing | Send `:GR#`, a command with trailing CR/LF, a 30-character command, and `0x06` | Normal replies; the 30-character command is **discarded whole** (see §11.5 — the old code executed its tail); `0x06` answers `P`, and answers nothing when the debug log shares the port | `core::meade::CommandAssembler`, `adapters::SerialTransport` (Phase 5) |
| 9.2 | Wi-Fi transport | Connect to the mount's AP, send the same commands, including a 200-character one and a mid-command `0x06` | Identical replies to 9.1; the over-long command is dropped and logged; the handshake answers `P`; serial and Wi-Fi replies never cross | `SerialTransport<128>` in `WifiControl::tcpLoop()` |
| 9.3 | Both transports at once | Serial and Wi-Fi simultaneously | Each peer gets only its own replies | `PendingReply` routing (Phase 5) |
| 9.4 | Status display | Run a slew and a park on a board with the info display | Display updates, no flicker/lockup; events arrive once per transition | `EventBus`, `updateInfoDisplay()` |
| 9.5 | LCD menu | Navigate every menu, start a slew and a park from the menu | No lockup; state matches the Meade view | menu screens (out of scope for the refactor — smoke only) |
| 9.6 | ESP32 stepper task | On ESP32: track continuously for 10 min while polling `:D#` | No missed steps, no watchdog reset, serial stays responsive | `stepperControlTask`, `MountCoordinator::tick()` order |
| 9.7 | RP2040 stepping | On RP2040: same as 9.6 | same | `hal/rp2040` timer service |
| 9.8 | AVR timer stepping | On AVR: 10 min tracking, then a long slew, then guide pulses | No stutter, no reset, no dropped steps. **Highest-risk untested change in the branch:** the TIMER1/3/4/5 vectors moved out of `Core.cpp` into `src/hal/avr/TimerIsrVectors.cpp`. The code is character-identical, but no host test can execute an interrupt vector | `hal/avr/TimerIsrVectors.cpp`, engine ISR split |
| 9.9 | GPS (if fitted) | `:gT#` with a fix | Time and site set | `TinyGpsAdapter` |
| 9.10 | Gyro/level (if fitted) | Menu level function | Reports tilt | untouched, smoke only |

---

## 10. Sign-off

| Section | Board | Date | Firmware `:GVN#` | Result | Notes |
|---|---|---|---|---|---|
| 2 Boot/storage | | | | | |
| 3 Manual slew | | | | | |
| 4 Go-to/guide | | | | | |
| 5 Parking | | | | | |
| 6 Homing/limits | | | | | |
| 7 AZ/ALT/focus | | | | | |
| 8 Drivers | | | | | |
| 9 Transports/displays | | | | | |

---

## 11. Deliberate behaviour changes to confirm (not bugs)

These were chosen during the refactor; each is covered above but listed together so a
reviewer does not "fix" them back:

1. **A go-to during parking is refused** (5.5). Previously `:MS#` could reprogram both axes
   underneath a park sequence that was still homing.
2. **`Parked` is published once per park** (5.3), not twice.
3. **The tick order is explicit** — published events are delivered at the top of `loop()`,
   before any controller is polled, and a guide pulse owns the tick (4.8–4.12).
4. **A board without an optional axis or focuser is a runtime no-op**, not compiled-out code
   (7.4, 7.8). Externally identical; internally the pointer is null.
5. **An over-long serial command is discarded up to its `#`** (9.1). The old loop restarted
   its buffer at the overflow point and then dispatched whatever tail arrived before the
   terminator, so a too-long command could run as a shorter valid one. If a host driver turns
   out to rely on that, it was relying on a bug — but note it, because it is the only change
   here that alters what a command *does*.
6. **AVR has no display** (all sections). `DISPLAY_TYPE` and `INFO_DISPLAY_TYPE` are forced to
   `NONE` on `__AVR_ATmega2560__` builds: the display stack does not fit beside the refactor's
   runtime configuration, and with it restored the board sat at 94.9 % of flash. Run every
   display case on ESP32/RP2040, and treat a headless AVR board as correct.
7. **The microstep rates and steps-per-degree defaults are runtime data** (8.2, 8.3, 2.1).
   They come from `app::buildMountConfig()` at boot instead of being compile-time constants.
   Externally identical — but it is why 8.2/8.3 deserve attention: a wrong value there is now
   a boot-time value rather than a build-time one.

## 12. What cannot be covered here

- The **AVR display restore** (§7.3 of the plan): display support is currently compiled out of
  AVR builds to fit flash. Restoring it is a separate change and needs its own pass of 9.4/9.5.
- **Timing-sensitive ISR work**: the AVR timer/ISR vectors were not moved, so nothing new to
  test there — but 9.8 confirms the refactored `loop()` did not disturb them.
- **Long-duration soak** (hours of tracking) is useful but is not a merge gate; note it in §10
  if run.
