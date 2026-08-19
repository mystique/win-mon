#pragma once

#include <windows.h>

struct RateFontSelection;

namespace settings
{
// Returns true if the Rate Strip is allowed to answer right-click with the
// Operator Menu. Defaults to false when the value is absent or unreadable.
[[nodiscard]] bool IsRightClickSpeedTextEnabled() noexcept;

// Persists the Right-Click Speed Text setting under the product key.
// Returns true on success.
bool SetRightClickSpeedTextEnabled(bool enabled) noexcept;

// Returns true when the Floating Rate Display should be shown at launch.
[[nodiscard]] bool IsFloatingRateDisplayEnabled() noexcept;

bool SetFloatingRateDisplayEnabled(bool enabled) noexcept;

// Returns false when the saved desktop position is absent or unreadable.
[[nodiscard]] bool GetFloatingRateDisplayPosition(POINT& position) noexcept;

bool SetFloatingRateDisplayPosition(POINT position) noexcept;

// Reads the persisted Rate Font. Returns false when absent or invalid.
[[nodiscard]] bool GetRateFont(RateFontSelection& selection) noexcept;

// Persists the selected Rate Font face, style, and point size atomically.
bool SetRateFont(const RateFontSelection& selection) noexcept;
}
