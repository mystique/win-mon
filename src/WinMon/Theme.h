#pragma once

namespace theme
{
enum class Mode
{
    Dark,
    Light,
};

// Reads the Windows app theme used by both the Tray Icon and Rate Strip.
// Missing or unreadable registry state falls back to dark mode.
[[nodiscard]] Mode ReadMode() noexcept;
}
