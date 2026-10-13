// SPDX-License-Identifier: MIT
// Copyright (c) 2026 mk2.app

#include "settings/OperatorSettings.h"

#include <windows.h>


namespace
{
constexpr wchar_t kProductKey[] = L"Software\\Win Mon";
constexpr wchar_t kRightClickSpeedTextValue[] = L"RightClickSpeedText";
constexpr wchar_t kFloatingRateDisplayValue[] = L"FloatingRateDisplay";
constexpr wchar_t kFloatingRateDisplayPositionValue[] = L"FloatingRateDisplayPosition";
constexpr wchar_t kSpeedTextPositionValue[] = L"SpeedTextPosition";
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
int GetFloatingRateOpacity() noexcept
{
    DWORD value = 100, size = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER,kProductKey,L"FloatingRateOpacity",RRF_RT_REG_DWORD,
            nullptr,&value,&size) != ERROR_SUCCESS || size != sizeof(value) || value < 10 || value > 100) return 100;
    return static_cast<int>((value+5)/10*10);
}

bool SetFloatingRateOpacity(int percent) noexcept
{
    if (percent < 10 || percent > 100 || percent % 10 != 0) return false;
    const DWORD value = static_cast<DWORD>(percent);
    return RegSetKeyValueW(HKEY_CURRENT_USER,kProductKey,L"FloatingRateOpacity",REG_DWORD,&value,sizeof(value)) == ERROR_SUCCESS;
}
bool IsFloatingRateChartVisible() noexcept
{
    DWORD value = 1, size = sizeof(value);
    return RegGetValueW(HKEY_CURRENT_USER, kProductKey, L"FloatingRateChartVisible",
        RRF_RT_REG_DWORD, nullptr, &value, &size) != ERROR_SUCCESS || size != sizeof(value) || value != 0;
}
bool SetFloatingRateChartVisible(bool visible) noexcept
{
    return WriteFlag(L"FloatingRateChartVisible", visible);
}

bool IsFloatingRateTextVisible() noexcept
{
    DWORD value = 1, size = sizeof(value);
    return RegGetValueW(HKEY_CURRENT_USER, kProductKey, L"FloatingRateTextVisible",
        RRF_RT_REG_DWORD, nullptr, &value, &size) != ERROR_SUCCESS || size != sizeof(value) || value != 0;
}
bool SetFloatingRateTextVisible(bool visible) noexcept
{
    return WriteFlag(L"FloatingRateTextVisible", visible);
}

bool IsSpeedTextOnLeft() noexcept
{
    DWORD value = 0, size = sizeof(value);
    return RegGetValueW(HKEY_CURRENT_USER, kProductKey, kSpeedTextPositionValue,
        RRF_RT_REG_DWORD, nullptr, &value, &size) == ERROR_SUCCESS &&
        size == sizeof(value) && value == 1;
}

bool SetSpeedTextOnLeft(bool left) noexcept
{
    return WriteFlag(kSpeedTextPositionValue, left);
}

bool IsRightClickSpeedTextEnabled() noexcept
{
    bool enabled = false;
    return ReadFlag(kRightClickSpeedTextValue, enabled) && enabled;
}

bool SetRightClickSpeedTextEnabled(bool enabled) noexcept
{
    return WriteFlag(kRightClickSpeedTextValue, enabled);
}

bool IsFloatingRateDisplayEnabled() noexcept
{
    bool enabled = false;
    return ReadFlag(kFloatingRateDisplayValue, enabled) && enabled;
}

bool SetFloatingRateDisplayEnabled(bool enabled) noexcept
{
    return WriteFlag(kFloatingRateDisplayValue, enabled);
}

bool GetFloatingRateDisplayPosition(POINT& position) noexcept
{
    DWORD size = sizeof(position);
    return RegGetValueW(
               HKEY_CURRENT_USER,
               kProductKey,
               kFloatingRateDisplayPositionValue,
               RRF_RT_REG_BINARY,
               nullptr,
               &position,
               &size) == ERROR_SUCCESS && size == sizeof(position);
}

bool SetFloatingRateDisplayPosition(POINT position) noexcept
{
    return RegSetKeyValueW(
               HKEY_CURRENT_USER,
               kProductKey,
               kFloatingRateDisplayPositionValue,
               REG_BINARY,
               &position,
               sizeof(position)) == ERROR_SUCCESS;
}

}
