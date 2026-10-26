#pragma once

/// The heap that is still free, in bytes.
///
/// A platform query, not a domain one: the status screen shows it, the info panel draws it and the
/// factory console prints it. Each core answers it differently (or not at all — the AVR builds
/// report 0, because nothing there allocates after boot).
int freeMemory();
