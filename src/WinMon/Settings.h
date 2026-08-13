#pragma once

namespace settings
{
// Returns true if the Rate Strip is allowed to answer right-click with the
// Operator Menu. Defaults to false when the value is absent or unreadable.
[[nodiscard]] bool IsRateStripContextMenuEnabled() noexcept;

// Persists the Rate Strip right-click setting under the product key.
// Returns true on success.
bool SetRateStripContextMenuEnabled(bool enabled) noexcept;
}
