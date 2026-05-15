# adapters/

Thin glue binding domain [`../ports/`](../ports/) to [`../hal/`](../hal/) backends
(and to non-HAL libraries such as TinyGPS data structs).

Examples (planned): `AccelStepperAxis`, `Tmc2209Driver`, `EepromPersistentStore`,
`SerialLogger`, `LcdMenuDisplay`, `Ssd1306InfoDisplay`, `SerialTransport`,
`WifiTransport`, `TinyGpsAdapter`.

No domain logic lives here — adapters translate, they don't decide.

Note: the snippets below are illustrative only. They demonstrate layer responsibilities and dependency direction, not the final refactored code shape; actual code can and will differ.

Minimal go-to example:

```cpp
class StepperAxis {
public:
	explicit StepperAxis(int32_t stepsPerArcSecondX1000)
		: _stepsPerArcSecondX1000(stepsPerArcSecondX1000)
	{
	}

	virtual ~StepperAxis() = default;

	virtual long currentSteps() const = 0;
	virtual bool isBusy() const = 0;
	virtual void stop() = 0;

protected:
	long stepsFromMilliArcSeconds(int32_t targetMilliArcSeconds) const
	{
		return static_cast<long>((static_cast<int64_t>(targetMilliArcSeconds) * _stepsPerArcSecondX1000) / 1000000);
	}

	static int32_t milliArcSecondsFromRa(const RA &target)
	{
		return target.milliArcSeconds();
	}

private:
	int32_t _stepsPerArcSecondX1000;
};

class AccelStepperAxis : public StepperAxis {
public:
	AccelStepperAxis(hal::IStepperMotor &motor, int32_t stepsPerArcSecondX1000)
		: StepperAxis(stepsPerArcSecondX1000), _motor(motor)
	{
	}

	void moveToSteps(long targetSteps)
	{
		_motor.setTargetSteps(targetSteps);
		_motor.startMotion();
	}

	long currentSteps() const override { return _motor.currentSteps(); }
	bool isBusy() const { return _motor.isRunning(); }
	void stop() { _motor.stopMotion(); }

protected:
	void moveToMilliArcSeconds(int32_t targetMilliArcSeconds)
	{
		moveToSteps(stepsFromMilliArcSeconds(targetMilliArcSeconds));
	}

private:
	hal::IStepperMotor &_motor;
};

class RaAxis : public IRaAxis, private AccelStepperAxis {
public:
	RaAxis(hal::IStepperMotor &motor, int32_t stepsPerArcSecondX1000)
		: AccelStepperAxis(motor, stepsPerArcSecondX1000)
	{
	}

	void moveTo(const RA &target) override
	{
		moveToMilliArcSeconds(milliArcSecondsFromRa(target));
	}

	bool isBusy() const override { return AccelStepperAxis::isBusy(); }
	void stop() override { AccelStepperAxis::stop(); }
};
```

Responsibility: keep step-based/transmission-aware behavior in a generic base, implement the concrete motor backend in `AccelStepperAxis`, then add RA-specific translation in `RaAxis`.
Dependency rule: adapters depend on both [`../ports/`](../ports/) and [`../hal/`](../hal/), but they do not decide go-to targets or mount behavior.

See [specs/plan.md](../../specs/plan.md) for the target architecture.
