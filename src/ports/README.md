# ports/

Domain-level interfaces consumed by [`../core/`](../core/): what the domain *needs*
(axis position, persistent value, "now", a log sink, a transport, …).

Pure C++ interfaces only. No Arduino, no hardware libraries, no `#ifdef` feature flags.
Adapters in [`../adapters/`](../adapters/) implement these ports on top of [`../hal/`](../hal/).

See [specs/plan.md](../../specs/plan.md) for the target architecture.
