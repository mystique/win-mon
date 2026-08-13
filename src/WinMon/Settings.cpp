#include "Settings.h"

#include <windows.h>

namespace
{
constexpr wchar_t kProductKey[] = L"Software\\Win Mon";
constexpr wchar_t kRateStripContextMenuValue[] = L"RateStripContextMenu";
}

namespace settings
{

bool IsRateStripContextMenuEnabled() noexcept
{
    DWORD value = 0;
    DWORD size = sizeof(value);
    const LSTATUS status = RegGetValueW(
        HKEY_CURRENT_USER,
        kProductKey,
        kRateStripContextMenuValue,
        RRF_RT_REG_DWORD,
        nullptr,
        &value,
        &size);
    return status == ERROR_SUCCESS && value != 0;
}

bool SetRateStripContextMenuEnabled(bool enabled) noexcept
{
    const DWORD value = enabled ? 1u : 0u;
    // RegSetKeyValueW creates the product key on first write.
    const LSTATUS status = RegSetKeyValueW(
        HKEY_CURRENT_USER,
        kProductKey,
        kRateStripContextMenuValue,
        REG_DWORD,
        &value,
        sizeof(value));
    return status == ERROR_SUCCESS;
}

}
