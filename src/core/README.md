# core/

Pure domain logic. **No Arduino, no hardware libraries, no `#ifdef` feature flags.**
Everything here must be buildable and unit-testable on the host (`native_core` PIO env).

Depends only on the C++ standard library and on interfaces in [`../ports/`](../ports/).

See [specs/plan.md](../../specs/plan.md) for the target architecture.
