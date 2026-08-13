#include "Settings.h"

#include <windows.h>

namespace
{
constexpr wchar_t kProductKey[] = L"Software\\Win Mon";
constexpr wchar_t kRightClickSpeedTextValue[] = L"RightClickSpeedText";
// Name used before the menu relabel; read once so a saved choice survives.
constexpr wchar_t kLegacyRightClickSpeedTextValue[] = L"RateStripContextMenu";

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

bool IsRightClickSpeedTextEnabled() noexcept
{
    bool enabled = false;
    if (ReadFlag(kRightClickSpeedTextValue, enabled))
    {
        return enabled;
    }
    if (!ReadFlag(kLegacyRightClickSpeedTextValue, enabled))
    {
        return false;
    }
    // Drop the old value only once the new one is durable, so a failed write
    // cannot lose the setting.
    if (WriteFlag(kRightClickSpeedTextValue, enabled))
    {
        static_cast<void>(RegDeleteKeyValueW(
            HKEY_CURRENT_USER, kProductKey, kLegacyRightClickSpeedTextValue));
    }
    return enabled;
}

bool SetRightClickSpeedTextEnabled(bool enabled) noexcept
{
    return WriteFlag(kRightClickSpeedTextValue, enabled);
}

}
