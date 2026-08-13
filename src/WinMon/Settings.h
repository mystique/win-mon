#pragma once

namespace settings
{
// Returns true if the Rate Strip is allowed to answer right-click with the
// Operator Menu. Defaults to false when the value is absent or unreadable.
[[nodiscard]] bool IsRightClickSpeedTextEnabled() noexcept;

// Persists the Right-Click Speed Text setting under the product key.
// Returns true on success.
bool SetRightClickSpeedTextEnabled(bool enabled) noexcept;
}
