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
bool ReadFlag(const wchar_t* valueName, bool& enabled) noexcept
{
    DWORD value = 0;
    DWORD size = sizeof(value);
    const LSTATUS status = RegGetValueW(
        HKEY_CURRENT_USER,
        kProductKey,
        valueName,
        RRF_RT_REG_DWORD,
        nullptr,
        &value,
        &size);
    if (status != ERROR_SUCCESS)
    {
        return false;
    }
    enabled = value != 0;
    return true;
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
bool IsTaskbarTextVisible() noexcept
{
    DWORD value = 1, size = sizeof(value);
    return RegGetValueW(HKEY_CURRENT_USER, kProductKey, L"TaskbarTextVisible", RRF_RT_REG_DWORD,
        nullptr, &value, &size) != ERROR_SUCCESS || size != sizeof(value) || value != 0;
}

bool SetTaskbarTextVisible(bool visible) noexcept
{
    return WriteFlag(L"TaskbarTextVisible", visible);
}

bool IsTrayIconVisible() noexcept
{
    DWORD value = 1, size = sizeof(value);
    return RegGetValueW(HKEY_CURRENT_USER, kProductKey, L"TrayIconVisible", RRF_RT_REG_DWORD,
        nullptr, &value, &size) != ERROR_SUCCESS || size != sizeof(value) || value != 0;
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
    DWORD value = 1, size = sizeof(value);
    return RegGetValueW(HKEY_CURRENT_USER, kProductKey, L"FloatingChartsVisible",
        RRF_RT_REG_DWORD, nullptr, &value, &size) != ERROR_SUCCESS || size != sizeof(value) || value != 0;
}
bool SetFloatingChartsVisible(bool visible) noexcept
{
    return WriteFlag(L"FloatingChartsVisible", visible);
}

bool IsFloatingTextVisible() noexcept
{
    DWORD value = 1, size = sizeof(value);
    return RegGetValueW(HKEY_CURRENT_USER, kProductKey, L"FloatingTextVisible",
        RRF_RT_REG_DWORD, nullptr, &value, &size) != ERROR_SUCCESS || size != sizeof(value) || value != 0;
}
bool SetFloatingTextVisible(bool visible) noexcept
{
    return WriteFlag(L"FloatingTextVisible", visible);
}

bool IsTaskbarDisplayOnLeft() noexcept
{
    DWORD value = 0, size = sizeof(value);
    return RegGetValueW(HKEY_CURRENT_USER, kProductKey, kTaskbarDisplayPositionValue,
        RRF_RT_REG_DWORD, nullptr, &value, &size) == ERROR_SUCCESS &&
        size == sizeof(value) && value == 1;
}

bool SetTaskbarDisplayOnLeft(bool left) noexcept
{
    return WriteFlag(kTaskbarDisplayPositionValue, left);
}

bool IsTaskbarContextMenuEnabled() noexcept
{
    bool enabled = false;
    return ReadFlag(kTaskbarContextMenuValue, enabled) && enabled;
}

bool SetTaskbarContextMenuEnabled(bool enabled) noexcept
{
    return WriteFlag(kTaskbarContextMenuValue, enabled);
}

bool IsFloatingDisplayEnabled() noexcept
{
    bool enabled = false;
    return ReadFlag(kFloatingDisplayValue, enabled) && enabled;
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
