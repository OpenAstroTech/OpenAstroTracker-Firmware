# RP2040 Wiring Guide

Pin assignments for the two RP2040 boards supported by the OpenAstroTracker
firmware:

| Board | Board ID | Pins file |
|---|---|---|
| BigTreeTech **SKR Pico v1.0** | `BOARD_RP2040_SKR_PICO` (2201) | `boards/RP2040_SKR_PICO/pins_RP2040_SKR_PICO.hpp` |
| **JackW01** (pi-pico-printer-board carrier) | `BOARD_RP2040_JACKW01` (2101) | `boards/RP2040_JACKW01/pins_RP2040_JACKW01.hpp` |

Everything below reflects the **default** pin mapping compiled into the
firmware. Peripherals are enabled in your `Configuration_local_*.hpp` (see
`Configuration_local_examples/Configuration_local_rp2040_skr_pico.hpp` for a
complete annotated example). GPIO numbers refer to the `GPxx` labels on the
board, not to physical pin positions.

All GPIO signals are **3.3 V logic**.

---

## Power (SKR Pico)

The board can be powered from USB, from the VIN terminals (12-24 V), or both -
but **never leave the USB-power jumper on while VIN is applied**. The jumper
ties the 5 V rail to USB VBUS; with a supply also feeding the rail, the
contention can hang the board during stepper driver initialization. Use:

- **Jumper removed + VIN (12-24 V)** - normal operation. Motors and drivers are
  powered, and the USB connector still works for flashing and serial (its data
  lines are independent of VBUS).
- **USB only (jumper fitted)** - the firmware boots and serial works, but the
  stepper drivers have no motor power; nothing moves.

## Stepper motors

Drivers must be `DRIVER_TYPE_TMC2209_UART` (the boards carry no A4988/standalone
wiring - MS pins are not routed).

### SKR Pico (on-board TMC2209s)

| Axis | Slot | STEP | DIR | EN | Endstop input | TMC2209 UART address |
|---|---|---|---|---|---|---|
| RA | X | GP11 | GP10 | GP12 | GP4 | 0 |
| DEC | Y | GP6 | GP5 | GP7 | GP3 | 2 |
| AZ | Z | GP19 | GP28 | GP2 | - | 1 |
| ALT | E0 | GP14 | GP13 | GP15 | - | 3 |

Note the address order X=0, **Y=2, Z=1**, E0=3 - set by the MS1/MS2 strapping
on the SKR Pico PCB and the opposite of the JackW01 board.

AZ and ALT are optional: set `AZ_DRIVER_TYPE`/`ALT_DRIVER_TYPE` to
`DRIVER_TYPE_NONE` if your mount has no vertical axis motors.

### JackW01 (TMC2209 modules)

| Axis | Slot | STEP | DIR | EN | Endstop input | TMC2209 UART address |
|---|---|---|---|---|---|---|
| RA | X | GP7 | GP6 | GP5 | GP4 | 0 |
| DEC | Y | GP11 | GP10 | GP5 | GP3 | 1 |
| AZ | Z | GP13 | GP12 | GP5 | - | 2 |
| ALT | E | GP15 | GP14 | GP5 | - | 3 |

- **EN is shared**: all four enable lines are tied to GP5, so enable/disable
  always affects every axis. Per-axis current control happens over UART
  (IRUN/IHOLD) instead.
- Use BTT TMC2209 modules with the PDN_UART pad bridge (the on-board resistor
  linking RESET/TX and MS3/RX pads to PDN_UART). The carrier routes UART1
  (GP8 TX, GP9 RX) to the driver sockets through R202/R201.

## USB serial (control and flashing)

- The USB port enumerates as a virtual COM port (USB CDC) and carries the
  Meade/LX200 protocol used by OATControl, the ASCOM driver, and INDI. No
  handshaking lines are required - hosts that open the port without asserting
  DTR still receive replies.
- Flashing: the board resets into BOOTSEL mode when a serial port opens at
  1200 baud (the standard arduino-pico upload path), or hold BOOTSEL while
  applying power.
- `:XGM#` reports `RP2040` as the hardware type.

## Info display (SSD1306 128x64 OLED, I2C)

Config keys (set in your local config; the pins default correctly per board):

```cpp
#define INFO_DISPLAY_TYPE             INFO_DISPLAY_TYPE_I2C_SSD1306_128x64
#define INFO_DISPLAY_I2C_ADDRESS      0x3C   // default
#define INFO_DISPLAY_UPSIDE_DOWN      0      // 1 = rotate 180 degrees
#define INFO_DISPLAY_MIRRORED         0      // 1 = flip horizontally
```

- **SKR Pico**: SDA = GP0, SCL = GP1 (I2C0) — the only header-accessible
  I2C-capable GPIO pair on this board, and the firmware default when the
  display is enabled. Wire SDA→IO0, SCL→IO1, VCC→5V, GND→GND, all on the
  left-side header (bench-verified with a common 4-pin SSD1306 module on 5V).
  **GP0/GP1 are shared with the default GPS wiring** — enable either the
  display or GPS, not both (enforced at compile time).
- **JackW01**: defaults are SDA = GP20, SCL = GP21 (free on that carrier);
  override with `INFO_DISPLAY_I2C_SDA_PIN`/`..._SCL_PIN` to use other pins.

> **Important (SKR Pico):** do **not** use GP20/GP21 for the display on the SKR
> Pico. They only drive on-board MOSFETs (the HB/FAN/Laser power outputs,
> labelled + and − on the board) and are never exposed as raw GPIO on any
> connector. Wiring an I2C display to those pins hangs the bus and the
> firmware (enforced at compile time).

## GPS (SKR Pico)

Wired to the left-side 5-pin header (IO0, IO1, GND, 5V) using UART0:

| GPS module pin | Connect to | RP2040 GPIO |
|---|---|---|
| RX (module input) | IO0 | GP0 (MCU TX - fixed mux, TX only) |
| TX (module output) | IO1 | GP1 (MCU RX - fixed mux, RX only) |
| VCC | 5V | - |
| GND | GND | - |

Enable with `#define USE_GPS 1`. Tested with an ATGM336H-5N; any GPS module
with a 3.3 V UART output works (the RP2040 GPIO are not 5 V tolerant).
GP0/GP1 are shared with the info display's default pins on this board —
enable either GPS or the display, not both (enforced at compile time).

The JackW01 board has no default GPS wiring; leave `USE_GPS` at 0.

## Hall homing sensors (RA and DEC)

Active-LOW inputs on the endstop headers:

| Axis | Input | GPIO | Config |
|---|---|---|---|
| RA | X-STOP | GP4 | `USE_HALL_SENSOR_RA_AUTOHOME`, `RA_HOMING_SENSOR_PIN`, `RA_HOMING_SENSOR_ACTIVE_STATE LOW` |
| DEC | Y-STOP | GP3 | `USE_HALL_SENSOR_DEC_AUTOHOME`, `DEC_HOMING_SENSOR_PIN`, `DEC_HOMING_SENSOR_ACTIVE_STATE LOW` |

The inputs are configured without pull-ups; open-drain Hall sensors need an
external pull-up (10 kOhm to 3.3 V). The search distance defaults to 30 degrees
(`RA_HOMING_SENSOR_SEARCH_DEGREES` etc. in `Configuration_adv.hpp`).

## DEC endstops (SKR Pico)

The two remaining endstop inputs can back up Hall homing with hardware limits:

| Limit | Input | GPIO | Config key |
|---|---|---|---|
| DEC down | Z-STOP | GP25 | `DEC_ENDSWITCH_DOWN_SENSOR_PIN` |
| DEC up | E-STOP | GP16 | `DEC_ENDSWITCH_UP_SENSOR_PIN` |

Both are active LOW (`DEC_END_SWITCH_ACTIVE_STATE`). RA endstops have no
default wiring on these boards - all four endstop inputs are allocated above.

## Fan (SKR Pico)

The FAN1 header is switched by a MOSFET on GP17 and carries the VIN rail
(12/24 V). Set the power percentage in your config:

```cpp
#define RP2040_FAN1_POWER_PERCENT 60   // 0-100, default 100
```

## Not available by default on RP2040

- **StallGuard driver autohome** - not implemented in the OAT firmware on any
  platform; use Hall sensor homing instead (the DIAG outputs are unused)
- **LCD menu display** - `DISPLAY_TYPE` must be `DISPLAY_TYPE_NONE`
  (enforced at compile time)
- **Gyro auto-leveling** - `USE_GYRO_LEVEL` must be 0
- **Focuser stepper** - all four driver slots are used by RA/DEC/AZ/ALT
- **Dew heater output** - no pin is wired on these boards
- **WiFi/Bluetooth** - ESP32 builds only
- SKR Pico's NeoPixel (GP24) and BLTouch (GP22/GP29) pins exist on the board
  but are unused by the OAT firmware

---

Source of truth for any pin: the pins files linked at the top. If your wiring
needs differ, every pin macro is `#ifndef`-guarded and can be overridden in
your local configuration file.
