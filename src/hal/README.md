# hal/

Hardware Abstraction Layer. Pure C++ interfaces describing *what the hardware can do*
(pin toggles, UART bytes, timer ticks, EEPROM cells, OLED panels, …) plus one backend
per platform.

Planned subdirectories:
- `arduino/` — generic Arduino implementation.
- `avr/` — AVR-specific bits (Timer1/Timer3 ISR, fast pin IO).
- `esp32/` — ESP32-specific (hardware timers, Wi-Fi glue).

Feature `#ifdef`s are allowed only here, for platform/backend selection.

See [specs/plan.md](../../specs/plan.md) for the target architecture.
