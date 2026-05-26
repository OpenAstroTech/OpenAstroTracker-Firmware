# hal/

Hardware Abstraction Layer. Pure C++ interfaces describing *what the hardware can do*
(pin toggles, UART bytes, timer ticks, EEPROM cells, OLED panels, …) plus one backend
per platform.

Planned subdirectories:
- `arduino/` — generic Arduino implementation.
- `avr/` — AVR-specific bits (Timer1/Timer3 ISR, fast pin IO).
- `esp32/` — ESP32-specific (hardware timers, Wi-Fi glue).

Feature `#ifdef`s are allowed only here, for platform/backend selection.

Note: the snippets below are illustrative only. They demonstrate layer responsibilities and dependency direction, not the final refactored code shape; actual code can and will differ.

Minimal go-to example:

```cpp
namespace hal {

class IStepperMotor {
public:
	virtual ~IStepperMotor() = default;
	virtual void setTargetSteps(long targetSteps) = 0;
	virtual long currentSteps() const = 0;
	virtual void startMotion() = 0;
	virtual bool isRunning() const = 0;
	virtual void stopMotion() = 0;
};

} // namespace hal
```

Responsibility: expose raw motor-driving capability such as target position and motion start/stop.
Dependency rule: HAL knows about hardware capabilities and platform backends, not about go-to policy, RA/DEC semantics, or domain state machines.

See [specs/plan.md](../../specs/plan.md) for the target architecture.
