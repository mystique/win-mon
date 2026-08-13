#pragma once

struct RateFontSelection;

namespace settings
{
// Returns true if the Rate Strip is allowed to answer right-click with the
// Operator Menu. Defaults to false when the value is absent or unreadable.
[[nodiscard]] bool IsRightClickSpeedTextEnabled() noexcept;

// Persists the Right-Click Speed Text setting under the product key.
// Returns true on success.
bool SetRightClickSpeedTextEnabled(bool enabled) noexcept;

// Reads the persisted Rate Font. Returns false when absent or invalid.
[[nodiscard]] bool GetRateFont(RateFontSelection& selection) noexcept;

// Persists the selected Rate Font face, style, and point size atomically.
bool SetRateFont(const RateFontSelection& selection) noexcept;
}
