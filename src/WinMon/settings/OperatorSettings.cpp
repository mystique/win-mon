// SPDX-License-Identifier: MIT
// Copyright (c) 2026 mk2.app

#include "settings/OperatorSettings.h"

#include <windows.h>


namespace
{
constexpr wchar_t kProductKey[] = L"Software\\Win Mon";
constexpr wchar_t kTaskbarContextMenuValue[] = L"TaskbarContextMenuEnabled";
constexpr wchar_t kFloatingDisplayValue[] = L"FloatingDisplayVisible";
constexpr wchar_t kFloatingDisplayPositionValue[] = L"FloatingDisplayPosition";
constexpr wchar_t kTaskbarDisplayPositionValue[] = L"TaskbarDisplayPosition";
bool ReadBool(const wchar_t* valueName, bool defaultValue) noexcept
{
    DWORD value = defaultValue ? 1u : 0u;
    DWORD size = sizeof(value);
    if (RegGetValueW(
            HKEY_CURRENT_USER,
            kProductKey,
            valueName,
            RRF_RT_REG_DWORD,
            nullptr,
            &value,
            &size) != ERROR_SUCCESS ||
        size != sizeof(value) || value > 1u)
    {
        return defaultValue;
    }
    return value == 1u;
}

bool WriteFlag(const wchar_t* valueName, bool enabled) noexcept
{
    const DWORD value = enabled ? 1u : 0u;
    // RegSetKeyValueW creates the product key on first write.
    const LSTATUS status = RegSetKeyValueW(
        HKEY_CURRENT_USER,
        kProductKey,
        valueName,
        REG_DWORD,
        &value,
        sizeof(value));
    return status == ERROR_SUCCESS;
}
}

namespace settings
{
bool HasConfiguredMenuAccess() noexcept
{
    return winmon::HasConfiguredMenuAccess({
        .taskbarContextMenuEnabled = IsTaskbarContextMenuEnabled(),
        .floatingDisplayEnabled = IsFloatingDisplayEnabled(),
        .taskbarTextVisible = IsTaskbarTextVisible(),
        .trayIconVisible = IsTrayIconVisible()});
}

winmon::OperatorMenuToggles LoadStartupToggles() noexcept
{
    winmon::OperatorMenuToggles toggles{
        .taskbarContextMenuEnabled = IsTaskbarContextMenuEnabled(),
        .floatingDisplayEnabled = IsFloatingDisplayEnabled(),
        .taskbarDisplayOnLeft = IsTaskbarDisplayOnLeft(),
        .floatingTextVisible = IsFloatingTextVisible(),
        .floatingChartsVisible = AreFloatingChartsVisible(),
        .floatingDisplayOpacity = GetFloatingDisplayOpacity(),
        .taskbarTextVisible = IsTaskbarTextVisible(),
        .trayIconVisible = IsTrayIconVisible()};
    if (!winmon::HasConfiguredMenuAccess(toggles))
    {
        toggles.trayIconVisible = true;
        toggles.floatingDisplayEnabled = true;
        // Keep live access even if externally restricted registry permissions prevent repair.
        static_cast<void>(SetTrayIconVisible(true));
        static_cast<void>(SetFloatingDisplayEnabled(true));
    }
    return toggles;
}
bool IsLightTheme() noexcept
{
    DWORD value = 0, size = sizeof(value);
    return RegGetValueW(
               HKEY_CURRENT_USER,
               L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
               L"AppsUseLightTheme",
               RRF_RT_REG_DWORD,
               nullptr,
               &value,
               &size) == ERROR_SUCCESS &&
        size == sizeof(value) && value != 0;
}


bool IsTaskbarTextVisible() noexcept
{
    return ReadBool(L"TaskbarTextVisible", true);
}

bool SetTaskbarTextVisible(bool visible) noexcept
{
    return WriteFlag(L"TaskbarTextVisible", visible);
}

bool IsTrayIconVisible() noexcept
{
    return ReadBool(L"TrayIconVisible", true);
}

bool SetTrayIconVisible(bool visible) noexcept
{
    return WriteFlag(L"TrayIconVisible", visible);
}

int GetFloatingDisplayOpacity() noexcept
{
    DWORD value = 100, size = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER,kProductKey,L"FloatingDisplayOpacity",RRF_RT_REG_DWORD,
            nullptr,&value,&size) != ERROR_SUCCESS || size != sizeof(value) || value < 10 || value > 100) return 100;
    return static_cast<int>((value+5)/10*10);
}

bool SetFloatingDisplayOpacity(int percent) noexcept
{
    if (percent < 10 || percent > 100 || percent % 10 != 0) return false;
    const DWORD value = static_cast<DWORD>(percent);
    return RegSetKeyValueW(HKEY_CURRENT_USER,kProductKey,L"FloatingDisplayOpacity",REG_DWORD,&value,sizeof(value)) == ERROR_SUCCESS;
}
bool AreFloatingChartsVisible() noexcept
{
    return ReadBool(L"FloatingChartsVisible", true);
}
bool SetFloatingChartsVisible(bool visible) noexcept
{
    return WriteFlag(L"FloatingChartsVisible", visible);
}

bool IsFloatingTextVisible() noexcept
{
    return ReadBool(L"FloatingTextVisible", true);
}
bool SetFloatingTextVisible(bool visible) noexcept
{
    return WriteFlag(L"FloatingTextVisible", visible);
}

bool IsTaskbarDisplayOnLeft() noexcept
{
    return ReadBool(kTaskbarDisplayPositionValue, false);
}

bool SetTaskbarDisplayOnLeft(bool left) noexcept
{
    return WriteFlag(kTaskbarDisplayPositionValue, left);
}

bool IsTaskbarContextMenuEnabled() noexcept
{
    return ReadBool(kTaskbarContextMenuValue, false);
}

bool SetTaskbarContextMenuEnabled(bool enabled) noexcept
{
    return WriteFlag(kTaskbarContextMenuValue, enabled);
}

bool IsFloatingDisplayEnabled() noexcept
{
    return ReadBool(kFloatingDisplayValue, false);
}

bool SetFloatingDisplayEnabled(bool enabled) noexcept
{
    return WriteFlag(kFloatingDisplayValue, enabled);
}

bool GetFloatingDisplayPosition(POINT& position) noexcept
{
    DWORD size = sizeof(position);
    return RegGetValueW(
               HKEY_CURRENT_USER,
               kProductKey,
               kFloatingDisplayPositionValue,
               RRF_RT_REG_BINARY,
               nullptr,
               &position,
               &size) == ERROR_SUCCESS && size == sizeof(position);
}

bool SetFloatingDisplayPosition(POINT position) noexcept
{
    return RegSetKeyValueW(
               HKEY_CURRENT_USER,
               kProductKey,
               kFloatingDisplayPositionValue,
               REG_BINARY,
               &position,
               sizeof(position)) == ERROR_SUCCESS;
}

}
