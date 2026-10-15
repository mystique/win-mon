// SPDX-License-Identifier: MIT
// Copyright (c) 2026 mk2.app

#pragma once

#include <windows.h>

namespace settings
{
[[nodiscard]] bool IsTaskbarTextVisible() noexcept;
bool SetTaskbarTextVisible(bool visible) noexcept;
[[nodiscard]] bool IsTrayIconVisible() noexcept;
bool SetTrayIconVisible(bool visible) noexcept;
int GetFloatingDisplayOpacity() noexcept;
bool SetFloatingDisplayOpacity(int percent) noexcept;
[[nodiscard]] bool AreFloatingChartsVisible() noexcept;
bool SetFloatingChartsVisible(bool visible) noexcept;
[[nodiscard]] bool IsFloatingTextVisible() noexcept;
bool SetFloatingTextVisible(bool visible) noexcept;
[[nodiscard]] bool IsTaskbarDisplayOnLeft() noexcept;
bool SetTaskbarDisplayOnLeft(bool left) noexcept;
// Returns true if the Rate Strip is allowed to answer right-click with the
// Operator Menu. Defaults to false when the value is absent or unreadable.
[[nodiscard]] bool IsTaskbarContextMenuEnabled() noexcept;

// Persists the Enable Taskbar Context Menu setting under the product key.
// Returns true on success.
bool SetTaskbarContextMenuEnabled(bool enabled) noexcept;

// Returns true when the Floating Rate Display should be shown at launch.
[[nodiscard]] bool IsFloatingDisplayEnabled() noexcept;

bool SetFloatingDisplayEnabled(bool enabled) noexcept;

// Returns false when the saved desktop position is absent or unreadable.
[[nodiscard]] bool GetFloatingDisplayPosition(POINT& position) noexcept;

bool SetFloatingDisplayPosition(POINT position) noexcept;

}
