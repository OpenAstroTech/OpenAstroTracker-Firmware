# ports/

Domain-level interfaces consumed by [`../core/`](../core/): what the domain *needs*
(axis position, persistent value, "now", a log sink, a transport, …).

Pure C++ interfaces only. No Arduino, no hardware libraries, no `#ifdef` feature flags.
Adapters in [`../adapters/`](../adapters/) implement these ports on top of [`../hal/`](../hal/).

Note: the snippets below are illustrative only. They demonstrate layer responsibilities and dependency direction, not the final refactored code shape; actual code can and will differ.

Minimal go-to example:

```cpp
class RA {
public:
	explicit RA(int32_t milliArcSeconds) : _milliArcSeconds(milliArcSeconds) {}
	int32_t milliArcSeconds() const { return _milliArcSeconds; }

private:
	int32_t _milliArcSeconds;
};

class IRaAxis {
public:
	virtual ~IRaAxis() = default;
	virtual void moveTo(const RA &target) = 0;
	virtual bool isBusy() const = 0;
	virtual void stop() = 0;
};
```

Responsibility: define the axis contract the domain needs for a go-to in domain terms such as RA, not steps.
Dependency rule: ports are consumed by [`../core/`](../core/) and implemented by [`../adapters/`](../adapters/); they do not depend on HAL details.

See [specs/plan.md](../../specs/plan.md) for the target architecture.
