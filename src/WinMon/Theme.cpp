#include "Theme.h"

#include <windows.h>

namespace
{
constexpr wchar_t kPersonalizeKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
constexpr wchar_t kAppsUseLightThemeValue[] = L"AppsUseLightTheme";
}

namespace theme
{
Mode ReadMode() noexcept
{
    DWORD value = 0;
    DWORD size = sizeof(value);
    const LSTATUS status = RegGetValueW(
        HKEY_CURRENT_USER,
        kPersonalizeKey,
        kAppsUseLightThemeValue,
        RRF_RT_REG_DWORD,
        nullptr,
        &value,
        &size);
    return status == ERROR_SUCCESS && value != 0 ? Mode::Light : Mode::Dark;
}
}
