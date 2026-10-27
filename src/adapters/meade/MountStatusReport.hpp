#pragma once

#include <stddef.h>

namespace adapters
{
namespace meade
{

/// Everything `:D#` reports, gathered by the mount.
///
/// The wire shape is frozen: Stellarium, ASCOM and OATControl all parse it, so this struct is the
/// single place the field order is written down and the only place it may change.
struct MountStatusFacts {
    /// The activity word (`"Parked"`, `"SlewToTarget"`, `"Idle"`, …). May be empty.
    const char *activity;

    /// The six-character direction glyph row, pre-filled with '-' by the caller is *not* needed:
    /// the flags below decide each position.
    bool raRunning;
    bool decRunning;
    bool trackingAxisRunning;
    bool raTargetBelowCurrent;
    bool decTargetBelowCurrent;
    bool azRunning;
    bool azTargetBelowCurrent;
    bool altRunning;
    bool altTargetBelowCurrent;
    bool focusRunning;
    bool focusTargetBelowCurrent;
    /// True when the tracking axis is moving but no slew is in flight (the `T` in position 3).
    bool trackingOnly;

    long raSteps;
    long decSteps;
    long trkSteps;
    /// Narrower than the others: the field is empty on a board without a focuser.
    bool hasFocus;
    long focusSteps;

    /// Compact coordinate text, as the display and the protocol both use.
    const char *raText;
    const char *decText;
};

/// Writes `:D#`'s answer into `buffer`. Returns false if the buffer was too small, in which case
/// the contents are unspecified; the caller keeps its previous readout.
bool formatMountStatus(char *buffer, size_t size, const MountStatusFacts &facts);

}  // namespace meade
}  // namespace adapters
