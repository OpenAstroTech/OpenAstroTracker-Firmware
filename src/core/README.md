# core/

Pure domain logic. **No Arduino, no hardware libraries, no `#ifdef` feature flags.**
Everything here must be buildable and unit-testable on the host (`native_core` PIO env).

Depends only on the C++ standard library and on interfaces in [`../ports/`](../ports/).

Note: the snippets below are illustrative only. They demonstrate layer responsibilities and dependency direction, not the final refactored code shape; actual code can and will differ.

Minimal go-to example:

```cpp
struct EquatorialTarget {
	RA ra;
	int32_t decMilliArcSeconds;
};

class IRaAxis {
public:
	virtual ~IRaAxis() = default;
	virtual void moveTo(const RA &target) = 0;
};

class IDecAxis {
public:
	virtual ~IDecAxis() = default;
	virtual void moveToMilliArcSeconds(int32_t targetMilliArcSeconds) = 0;
};

class GoToController {
public:
	GoToController(IRaAxis &raAxis, IDecAxis &decAxis)
		: _raAxis(raAxis), _decAxis(decAxis)
	{
	}

	void goTo(const EquatorialTarget &target)
	{
		_raAxis.moveTo(target.ra);
		_decAxis.moveToMilliArcSeconds(target.decMilliArcSeconds);
	}

private:
	IRaAxis &_raAxis;
	IDecAxis &_decAxis;
};
```

Responsibility: decide what each axis should do for a go-to in domain units such as RA and DEC.
Dependency rule: core depends only on [`../ports/`](../ports/)-style interfaces, never on adapters, HAL, or Arduino APIs.

See [specs/plan.md](../../specs/plan.md) for the target architecture.
