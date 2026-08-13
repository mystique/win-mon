#pragma once

namespace autostart
{
// Returns true if the Win Mon Run key entry exists under HKCU.
[[nodiscard]] bool IsEnabled() noexcept;

// Writes the Run key entry pointing at the current executable (quoted path).
// Returns true on success.
[[nodiscard]] bool Enable() noexcept;

// Removes the Run key entry. Returns true if removed or was already absent.
[[nodiscard]] bool Disable() noexcept;
}
